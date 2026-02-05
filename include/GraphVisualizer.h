#ifndef GRAPH_VISUALIZER_H
#define GRAPH_VISUALIZER_H

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"

enum class NodeType { // enum really looks MUCH better here
  Instruction,
  BasicBlock,
  Argument,
  Constant,
  UnknownType,
};

enum class MyError {
  none,
  bad_input,
  cannot_open_file,
  runtime_log_parse,
  dot_export_failed,
  graphviz_failed
};

struct GraphNode {
  llvm::Value *value = nullptr;
  std::string id;
  std::string label;
  std::string type;
  std::string runtimeValue;

  NodeType nodeType = NodeType::UnknownType;
  bool isTerminator = false;
  bool hasRuntimeValue = false;

  llvm::BasicBlock *ownerBlock = nullptr;

  std::vector<std::string> operands;
  std::vector<std::string> defUseSuccessors;
  std::vector<std::string> cfgSuccessors;

  std::string functionName;
};

class GraphVisualizer final {
public:
  GraphVisualizer();
  ~GraphVisualizer() = default;

  bool buildCombinedGraph(llvm::Module &module,
                          const std::string &runtimeLogFile = "");

  bool exportToDot(const std::string &filename);

  void printStatistics();

  MyError lastError() const;
  const std::string &lastErrorMessage() const;

private:
  int basicBlockCount_ = 0;

  struct EdgeInfo {
    std::string from;
    std::string to;
    std::string runtimeValue;
  };

  bool loadRuntimeValues(const std::string &logFile);

  void calcStatistics();

  std::string getNodeId(llvm::Value *value);
  std::string getValueLabel(llvm::Value *value);
  std::string getInstructionType(llvm::Instruction *instr) const;
  std::string getInstructionLabel(llvm::Instruction &instr) const;
  std::string getBasicBlockLabel(llvm::BasicBlock &block) const;
  std::string escapeForDot(const std::string &text) const;
  std::string getInstructionName(llvm::Instruction &instr) const;
  std::string getShortInstructionLabel(const GraphNode &node);

  std::unordered_map<std::string, GraphNode> nodes_;
  std::unordered_map<std::string, std::string> runtimeValues_;

  void clearError();
  void setError(MyError code, const std::string &msg);

  bool runtimeValuesLoaded_ = false;

  struct CallEdge {
    const llvm::Function *callee = nullptr;
    std::string callSiteId;
    int callOrder = 0;
  };
  std::vector<CallEdge> callEdges_;
  std::map<const llvm::Function *, std::string> functionToEntryNode_;

  int instrCount_ = 0;
  int constCount_ = 0;
  int argCount_ = 0;
  int bbCount_ = 0;
  int cfgEdges_ = 0;
  int duEdges_ = 0;
  int runtimeCount_ = 0;

  MyError lastError_ = MyError::none;
  std::string lastErrorMsg_;
};

#endif // GRAPH_VISUALIZER_H
