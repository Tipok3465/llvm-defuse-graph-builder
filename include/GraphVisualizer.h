#ifndef GRAPH_VISUALIZER_H
#define GRAPH_VISUALIZER_H

#include "llvm/IR/Value.h"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

namespace llvm {
class Value;
class Instruction;
class BasicBlock;
class Function;
class Module;
class TerminatorInst;
} // namespace llvm

class GraphVisualizer {
public:
  GraphVisualizer();
  ~GraphVisualizer();

  bool buildCombinedGraph(llvm::Module &module,
                          const std::string &runtimeLogFile = "");

  bool exportToDot(const std::string &filename) const;

  void printStatistics() const;

private:
  struct GraphNode {
    llvm::Value *value = nullptr;
    std::string id;
    std::string label;
    std::string type;
    std::string runtimeValue;

    bool isInstruction = false;
    bool isBasicBlock = false;
    bool isConstant = false;
    bool isArgument = false;
    bool isTerminator = false;
    bool hasRuntimeValue = false;

    llvm::BasicBlock *parentBlock = nullptr;

    std::vector<std::string> operands;
    std::vector<std::string> defUseSuccessors;
    std::vector<std::string> cfgSuccessors;

    std::string functionName;
  };

  struct BasicBlockInfo {
    std::string id;
    std::string label;
    llvm::BasicBlock *blockPtr = nullptr;
    std::vector<std::string> instructions;
    std::string functionName;
  };

  bool loadRuntimeValues(const std::string &logFile);

  std::string getNodeId(llvm::Value *value) const;
  std::string getValueLabel(llvm::Value *value) const;
  std::string getInstructionType(llvm::Instruction *instr) const;
  std::string getInstructionLabel(llvm::Instruction &instr) const;
  std::string getBasicBlockLabel(llvm::BasicBlock &block) const;
  std::string escapeForDot(const std::string &text) const;
  std::string getInstructionName(llvm::Instruction &instr) const;
  std::string getShortInstructionLabel(const GraphNode &node) const;

  std::unordered_map<std::string, GraphNode> nodes_;
  std::unordered_map<std::string, BasicBlockInfo> basicBlocks_;
  std::unordered_map<std::string, std::string> runtimeValues_;

  bool runtimeValuesLoaded_;

  struct FunctionCallInfo {
    std::string caller;
    std::string callee;
    int callOrder;
    std::string callSiteId;
  };

  std::vector<FunctionCallInfo> functionCalls_;
  std::map<std::string, std::string> functionToEntryNode_;
};

#endif // GRAPH_VISUALIZER_H