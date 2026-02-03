#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <sstream> //TODO[Dkay]: my LSP says that this header is unused. Pls, setup yours too
#include <string>
#include <unordered_set>

// FIXME[Dkay]: He was afraid of `include`. But why.
namespace llvm {
class Module;
class Function;
class Value;
class Instruction;
class Constant;
class Type;
} // namespace llvm

class Instrumentation {
public:
  Instrumentation();

  // FIXME[Dkay]: Class should be either marked final or have an virtual dtor
  // FIXME[Dkay]: break of the rule of zero: class has untrivial dtor
  // FIXME[Dkay]: break of the rule of five: class has untrivial dtor and has not copy-, move- operators and copy-, move- ctors
  ~Instrumentation();
  
  // TODO[Dkay]: why `Module` is not a field?
  // This class almost doesn't store any state and that's weird.
  // Pass some args to ctor, save them as fields and make your methods interface thiner
  bool instrumentModule(const std::string &inputFile,
                        const std::string &outputFile);

private:
  void instrumentFunction(llvm::Function &function, llvm::Module &module);
  void instrumentValue(llvm::Value *value, llvm::Module &module,
                       const std::string &funcName,
                       const std::string &valueType);

  llvm::Constant *createGlobalString(llvm::Module &module,
                                     const std::string &str,
                                     const std::string &globalName);

  std::string getValueId(llvm::Value *value, const std::string &funcName);

  void insertPrintCall(llvm::Module &module, llvm::Function &printFunc,
                       llvm::Value *value, llvm::Constant *idStr,
                       llvm::Constant *nameStr, const std::string &funcName);

  llvm::Function *getOrDeclarePrintI32WithId(llvm::Module &module);
  llvm::Function *getOrDeclarePrintI64WithId(llvm::Module &module);
  llvm::Function *getOrDeclarePrintFloatWithId(llvm::Module &module);

  llvm::Function *getOrDeclarePrintFunction(llvm::Module &module,
                                            const std::string &name,
                                            llvm::Type *valueType);

  std::unordered_set<std::string> instrumentedValues_;
};

#endif // INSTRUMENTATON_H
