#ifndef INSTRUMENTATION_H
#define INSTRUMENTATION_H

#include <sstream>
#include <string>
#include <unordered_set>

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
  ~Instrumentation();

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