#ifndef GRAPH_VISUALIZER_H
#define GRAPH_VISUALIZER_H

#include "llvm/IR/Value.h"
#include <cstdint> //TODO[Dkay]: my LSP says that this header is unused. Pls, setup yours too
#include <map>
#include <set> //TODO[Dkay]: my LSP says that this header is unused. Pls, setup yours too
#include <string>
#include <unordered_map>
#include <vector>

// FIXME{Dkay}: this is weird. Why not to use `#inlcude` directive like
// everyone?
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

  // FIXME[Dkay]: Class should be either marked final or have an virtual dtor
  // FIXME[Dkay]: break of the rule of zero: class has untrivial dtor
  // FIXME[Dkay]: break of the rule of five: class has untrivial dtor and has not copy-, move- operators and copy-, move- ctors
  ~GraphVisualizer();

  bool buildCombinedGraph(llvm::Module &module,
                          const std::string &runtimeLogFile = "");

  bool exportToDot(const std::string &filename) const;

  void printStatistics() const;

private:
  // TODO[Dkay]: why to hide GraphNode interface inside, move it outside the class to improve code radability.
  struct GraphNode {
    llvm::Value *value = nullptr;
    std::string id;
    std::string label;
    std::string type;
    std::string runtimeValue;

    bool isInstruction =
        false; // FIXME[flops]: Use enum for this or determine value type via
               // llvm isa (or dyn_cast: it uses isa and casts value to needed
               // class if possible)
               // [Dkay]: +++
    
    bool isBasicBlock = false;
    bool isConstant = false;
    bool isArgument = false;
    bool isTerminator = false;
    bool hasRuntimeValue = false;

    llvm::BasicBlock *parentBlock = nullptr; // TODO[flops]: use union there

    std::vector<std::string> operands;
    std::vector<std::string> defUseSuccessors;
    std::vector<std::string> cfgSuccessors;

    std::string functionName;
  };

  struct BasicBlockInfo { // FIXME[flops]: BasicBlock class already contains
                          // that info
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

  struct FunctionCallInfo { // FIXME[Dkay]: llvm's function callee has same info
    std::string caller;
    std::string callee;
    int callOrder;
    std::string callSiteId;
  };

  std::vector<FunctionCallInfo> functionCalls_;
  std::map<std::string, std::string> functionToEntryNode_;
};

#endif // GRAPH_VISUALIZER_H
