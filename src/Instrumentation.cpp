#include "Instrumentation.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

// FIXME[Dkay]: I want a detailed explanation why do you need next two lines. I
// don't see any overloading resolution problems and I don't see any cases you
// want to explicitly mark ctor and dtor as default for.
Instrumentation::Instrumentation() = default;
Instrumentation::~Instrumentation() = default;

// FIXME[Dkay]: Why does it not take Module?
bool Instrumentation::instrumentModule(const std::string &inputFile,
                                       const std::string &outputFile) {
  LLVMContext context;
  SMDiagnostic error;
  std::unique_ptr<Module> module = parseIRFile(inputFile, error, context);
  if (!module) {
    // FIXME[Dkay]: make loggig turnable off, make such checks via template
    // function or define
    errs() << "Error: Failed to load module: " << inputFile << "\n";
    return false;
  }

  // FIXME[Dkay]: Why do you want to store this as a field if you clear it?
  instrumentedValues_.clear();

  // FIXME[DKay]: Why these function exist? They are way too single-purposed.
  getOrDeclarePrintI32WithId(*module);
  getOrDeclarePrintI64WithId(*module);
  getOrDeclarePrintFloatWithId(*module);

  for (auto &function : *module) {
    if (function.isDeclaration())
      continue;
    instrumentFunction(function, *module);
  }

  std::error_code ec;
  raw_fd_ostream out(outputFile, ec);
  if (ec) {
    errs() << "Error: Cannot open output file: " << outputFile << "\n";
    return false;
  }

  module->print(out, nullptr);
  return true;
}

void Instrumentation::instrumentFunction(Function &function, Module &module) {
  std::string funcName = function.getName().str();

  // instrument function arguments
  for (auto &arg : function.args()) {
    instrumentValue(&arg, module, funcName);
  }

  // instrument all instructions
  for (auto &block : function) {
    for (auto &instr : block) {
      // skip phi nodes, because it's a pain in the ass
      // FIXME[Dkay] Why not to insert yopur pass before phi nodes start to
      // exist?
      if (isa<PHINode>(&instr)) {
        continue;
      }

      // instrument the instruction itself
      instrumentValue(&instr, module, funcName);

      // instrument all operands
      for (unsigned i = 0; i < instr.getNumOperands(); i++) {
        Value *operand = instr.getOperand(i);

        // skip basic blocks and metadata
        if (isa<BasicBlock>(operand) || isa<MetadataAsValue>(operand)) {
          continue;
        }

        if (isa<ConstantInt>(operand) || isa<ConstantFP>(operand)) {
          instrumentValue(operand, module, funcName);
        }
      }
    }
  }
}

void Instrumentation::instrumentValue(Value *value, Module &module,
                                      const std::string &funcName) {
  if (!value) // FIXME[Dkay]: Why method can revieve an null pointer? Why it is
              // not privat and class invariants are not saving it from null
              // values?
    return;

  Type *type = value->getType();
  if (!type->isIntegerTy(32) && !type->isIntegerTy(64) && !type->isFloatTy()) {
    return;
  }

  std::string valueId = getValueId(value, funcName);

  // check if already instrumented
  if (instrumentedValues_.count(valueId)) {
    return;
  }

  std::string valueName;

  // FIXME[Dkay] Following code is unreadable
  if (Instruction *instr = dyn_cast<Instruction>(value)) {
    if (instr->hasName()) {
      valueName = "%" + instr->getName().str();
    } else {
      valueName = "inst";
    }
  } else if (Argument *arg = dyn_cast<Argument>(value)) {
    valueName = "%" + arg->getName().str();
  } else if (ConstantInt *ci = dyn_cast<ConstantInt>(value)) {
    std::string str;
    raw_string_ostream rso(str);
    rso << ci->getSExtValue();
    valueName = rso.str();
  } else if (ConstantFP *cf = dyn_cast<ConstantFP>(value)) {
    std::string str;
    raw_string_ostream rso(str);
    if (cf->getType()->isFloatTy()) {
      rso << cf->getValueAPF().convertToFloat();
    } else {
      rso << cf->getValueAPF().convertToDouble();
    }
    valueName = rso.str();
  } else {
    valueName = "val";
  }

  Constant *idStr = createGlobalString(module, valueId, "id_" + valueId);
  Constant *nameStr = createGlobalString(module, valueName, "name_" + valueId);

  if (type->isIntegerTy(32)) {
    insertPrintCall(module, *getOrDeclarePrintI32WithId(module), value, idStr,
                    nameStr);
  } else if (type->isIntegerTy(64)) {
    insertPrintCall(module, *getOrDeclarePrintI64WithId(module), value, idStr,
                    nameStr);
  } else if (type->isFloatTy()) {
    insertPrintCall(module, *getOrDeclarePrintFloatWithId(module), value, idStr,
                    nameStr);
  }
  instrumentedValues_.insert(valueId);
}

Constant *Instrumentation::createGlobalString(Module &module,
                                              const std::string &str,
                                              const std::string &globalName) {
  Constant *strConst = ConstantDataArray::getString(module.getContext(), str);

  auto *globalVar = module.getNamedGlobal(globalName);

  if (!globalVar) {
    globalVar =
        new GlobalVariable(module, strConst->getType(), true,
                           GlobalValue::PrivateLinkage, strConst, globalName);
  }

  // get pointer to first element
  Constant *zero = ConstantInt::get(Type::getInt32Ty(module.getContext()), 0);
  Constant *indices[] = {zero, zero};
  return ConstantExpr::getGetElementPtr(globalVar->getValueType(), globalVar,
                                        indices);
}

std::string Instrumentation::getValueId(Value *value,
                                        const std::string &funcName) {

  if (!value) // FIXME[DKay]: This should never be true since this is a method
              // protected by class' invariants
    return funcName + "_null";

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

void Instrumentation::insertPrintCall(Module &module, Function &printFunc,
                                      Value *value, Constant *idStr,
                                      Constant *nameStr) {
  Instruction *insertPoint = nullptr;

  if (Instruction *instr = dyn_cast<Instruction>(value)) {
    // insert right after the instruction
    BasicBlock::iterator it(instr);
    ++it;
    if (it != instr->getParent()->end()) {
      insertPoint = &*it;
    } else {
      // insert before terminator
      insertPoint = instr->getParent()->getTerminator();
    }
  } else if (Argument *arg = dyn_cast<Argument>(value)) {
    // insert at the beginning of the function
    insertPoint = &*arg->getParent()->getEntryBlock().getFirstInsertionPt();
  } else if (isa<Constant>(value)) {
    // insert at the beginning of main it's crap, but idk how to do it better;)
    Function *mainFunc = module.getFunction("main");
    if (mainFunc && !mainFunc->empty()) {
      insertPoint = &*mainFunc->getEntryBlock().getFirstInsertionPt();
    }
  }

  if (insertPoint) {
    IRBuilder<> builder(insertPoint);
    builder.CreateCall(&printFunc, {value, idStr, nameStr});
  }
}

// FIXME[Dkay]: Function is a way too specialized.
Function *Instrumentation::getOrDeclarePrintI32WithId(Module &module) {
  return getOrDeclarePrintFunction(module, "print_i32_with_id",
                                   Type::getInt32Ty(module.getContext()));
}

// FIXME[Dkay]: Function is a way too specialized.
Function *Instrumentation::getOrDeclarePrintI64WithId(Module &module) {
  return getOrDeclarePrintFunction(module, "print_i64_with_id",
                                   Type::getInt64Ty(module.getContext()));
}

// FIXME[Dkay]: Function is a way too specialized.
Function *Instrumentation::getOrDeclarePrintFloatWithId(Module &module) {
  return getOrDeclarePrintFunction(module, "print_float_with_id",
                                   Type::getFloatTy(module.getContext()));
}

Function *Instrumentation::getOrDeclarePrintFunction(Module &module,
                                                     const std::string &name,
                                                     Type *valueType) {
  Function *func = module.getFunction(name);
  if (func) {
    return func;
  }

  LLVMContext &ctx = module.getContext();

  Type *voidType = Type::getVoidTy(ctx);
  Type *i8PtrType = Type::getInt8Ty(ctx);

  std::vector<Type *> params;
  params.push_back(valueType);
  params.push_back(i8PtrType);
  params.push_back(i8PtrType);

  // FIXME[DKay]: Why not to use in-place creation of a vector
  // like this:
  //  FunctionType *funcType = FunctionType::get(voidType, {valueType,
  //  i8PtrType, i8PtrType}, false);

  FunctionType *funcType = FunctionType::get(voidType, params, false);

  func = Function::Create(funcType, GlobalValue::ExternalLinkage, name, module);
  func->setCallingConv(CallingConv::C); // FIXME[Dkay] Why? Isn't it default?

  return func;
}
