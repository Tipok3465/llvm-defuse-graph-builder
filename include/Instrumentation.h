#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include <string>
#include <unordered_set>

class Instrumentation final {
public:
  void instrumentModule(llvm::Module &module);
  void setVerbose(bool verbose) { verbose_ = verbose; }

private:
  void instrumentFunction(llvm::Function &function);
  void instrumentValue(llvm::Value *value, const std::string &funcName);

  std::string getValueNameForLog(llvm::Value *value);

  llvm::Constant *createGlobalString(const std::string &str,
                                     const std::string &globalName);

  std::string getValueId(llvm::Value *value, const std::string &funcName);

  void insertPrintCall(llvm::Function &printFunc, llvm::Value *value,
                       llvm::Constant *idStr, llvm::Constant *nameStr);

  llvm::Function *getOrDeclarePrintFunction(const std::string &name,
                                            llvm::Type *valueType);

  std::unordered_set<std::string> instrumentedValues_;
  llvm::Module *curModule_ = nullptr;
  bool verbose_ = false;
};

#endif // INSTRUMENTATON_H
