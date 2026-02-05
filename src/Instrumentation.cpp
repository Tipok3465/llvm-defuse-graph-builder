#include "Instrumentation.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void Instrumentation::instrumentModule(llvm::Module &module) {
  curModule_ = &module;
  instrumentedValues_.clear();
  getOrDeclarePrintFunction("print_i32_with_id",
                            Type::getInt32Ty(module.getContext()));
  getOrDeclarePrintFunction("print_i64_with_id",
                            Type::getInt64Ty(module.getContext()));
  getOrDeclarePrintFunction("print_float_with_id",
                            Type::getFloatTy(module.getContext()));
  for (auto &function : module) {
    if (function.isDeclaration()) {
      continue;
    }
    instrumentFunction(function);
  }
}

void Instrumentation::instrumentFunction(Function &function) {
  std::string funcName = function.getName().str();

  // instrument function arguments
  for (auto &arg : function.args()) {
    instrumentValue(&arg, funcName);
  }

  // instrument all instructions
  for (auto &block : function) {
    for (auto &instr : block) {
      // instrument the instruction itself
      instrumentValue(&instr, funcName);

      // instrument all operands
      for (unsigned i = 0; i < instr.getNumOperands(); i++) {
        Value *operand = instr.getOperand(i);

        // skip basic blocks and metadata
        if (isa<BasicBlock>(operand) || isa<MetadataAsValue>(operand)) {
          continue;
        }

        if (isa<ConstantInt>(operand) || isa<ConstantFP>(operand)) {
          instrumentValue(operand, funcName);
        }
      }
    }
  }
}

std::string Instrumentation::getValueNameForLog(Value *value) {
  if (auto *instr = dyn_cast<Instruction>(value)) {
    if (instr->hasName()) {
      return "%" + instr->getName().str();
    }
    return "inst";
  }

  if (auto *arg = dyn_cast<Argument>(value)) {
    return "%" + arg->getName().str();
  }

  if (auto *ci = dyn_cast<ConstantInt>(value)) {
    return std::to_string(ci->getSExtValue());
  }

  if (auto *cf = dyn_cast<ConstantFP>(value)) {
    if (cf->getType()->isFloatTy()) {
      return std::to_string(cf->getValueAPF().convertToFloat());
    }
    return std::to_string(cf->getValueAPF().convertToDouble());
  }

  return "val";
}

void Instrumentation::instrumentValue(Value *value,
                                      const std::string &funcName) {
  Type *type = value->getType();
  if (!type->isIntegerTy(32) && !type->isIntegerTy(64) && !type->isFloatTy()) {
    return;
  }

  std::string valueId = getValueId(value, funcName);

  // check if already instrumented
  if (instrumentedValues_.count(valueId)) {
    return;
  }

  std::string valueName = getValueNameForLog(value);

  Constant *idStr = createGlobalString(valueId, "id_" + valueId);
  Constant *nameStr = createGlobalString(valueName, "name_" + valueId);

  if (type->isIntegerTy(32)) {
    Function *pf = getOrDeclarePrintFunction(
        "print_i32_with_id", Type::getInt32Ty(curModule_->getContext()));
    insertPrintCall(*pf, value, idStr, nameStr);
  } else if (type->isIntegerTy(64)) {
    Function *pf = getOrDeclarePrintFunction(
        "print_i64_with_id", Type::getInt64Ty(curModule_->getContext()));
    insertPrintCall(*pf, value, idStr, nameStr);
  } else if (type->isFloatTy()) {
    Function *pf = getOrDeclarePrintFunction(
        "print_float_with_id", Type::getFloatTy(curModule_->getContext()));
    insertPrintCall(*pf, value, idStr, nameStr);
  }

  instrumentedValues_.insert(valueId);
}

Constant *Instrumentation::createGlobalString(const std::string &str,
                                              const std::string &globalName) {
  Constant *strConst =
      ConstantDataArray::getString(curModule_->getContext(), str);

  auto *globalVar = curModule_->getNamedGlobal(globalName);

  if (!globalVar) {
    globalVar =
        new GlobalVariable(*curModule_, strConst->getType(), true,
                           GlobalValue::PrivateLinkage, strConst, globalName);
  }

  // get pointer to first element
  Constant *zero =
      ConstantInt::get(Type::getInt32Ty(curModule_->getContext()), 0);
  Constant *indices[] = {zero, zero};
  return ConstantExpr::getGetElementPtr(globalVar->getValueType(), globalVar,
                                        indices);
}

std::string Instrumentation::getValueId(Value *value,
                                        const std::string &funcName) {

  if (Instruction *instr = dyn_cast<Instruction>(value)) {
    if (instr->hasName()) {
      return funcName + "_%" + instr->getName().str();
    }
    // unnamed: use opcode as key
    return funcName + "::" + std::string(instr->getOpcodeName());
  }

  if (Argument *arg = dyn_cast<Argument>(value)) {
    if (arg->hasName()) {
      return funcName + "_%" + arg->getName().str();
    }
    return funcName + "::arg";
  }

  if (ConstantInt *ci = dyn_cast<ConstantInt>(value)) {
    return funcName + "::const_" + std::to_string(ci->getSExtValue());
  }
  if (ConstantFP *cf = dyn_cast<ConstantFP>(value)) {
    std::string s;
    raw_string_ostream rso(s);
    cf->getValueAPF().print(rso);
    return funcName + "::constfp_" + rso.str();
  }

  // not recognized, need to fuck yourself
  return funcName + "::val";
}

void Instrumentation::insertPrintCall(Function &printFunc, Value *value,
                                      Constant *idStr, Constant *nameStr) {
  Instruction *insertPoint = nullptr;

  if (Instruction *instr = dyn_cast<Instruction>(value)) {
    // insert right after the instruction
    BasicBlock *bb = instr->getParent();
    if (isa<PHINode>(instr)) {
      // llvm requires all phi nodes to be grouped at the start of the block,
      // so we insert after the last phi (first non-phi instruction).
      insertPoint = bb->getFirstNonPHI();
      if (!insertPoint) {
        insertPoint = bb->getTerminator();
      }
    } else {
      BasicBlock::iterator it(instr);
      ++it;
      if (it != bb->end()) {
        insertPoint = &*it;
      } else {
        insertPoint = bb->getTerminator();
      }
    }
  } else if (Argument *arg = dyn_cast<Argument>(value)) {
    // insert at the beginning of the function
    insertPoint = &*arg->getParent()->getEntryBlock().getFirstInsertionPt();
  } else if (isa<Constant>(value)) {
    // insert at the beginning of main it's crap, but idk how to do it better;)
    Function *mainFunc = curModule_->getFunction("main");
    if (mainFunc && !mainFunc->empty()) {
      insertPoint = &*mainFunc->getEntryBlock().getFirstInsertionPt();
    }
  }

  if (insertPoint) {
    IRBuilder<> builder(insertPoint);
    builder.CreateCall(&printFunc, {value, idStr, nameStr});
  }
}

Function *Instrumentation::getOrDeclarePrintFunction(const std::string &name,
                                                     Type *valueType) {
  Function *func = curModule_->getFunction(name);
  if (func) {
    return func;
  }

  LLVMContext &ctx = curModule_->getContext();

  Type *voidType = Type::getVoidTy(ctx);
  Type *i8PtrType = Type::getInt8Ty(ctx);

  FunctionType *funcType =
      FunctionType::get(voidType, {valueType, i8PtrType, i8PtrType}, false);

  func = Function::Create(funcType, GlobalValue::ExternalLinkage, name,
                          *curModule_);

  return func;
}
