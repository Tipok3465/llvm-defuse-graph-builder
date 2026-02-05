#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include "GraphVisualizer.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

GraphVisualizer::GraphVisualizer() : runtimeValuesLoaded_(false) {}

void GraphVisualizer::clearError() {
  lastError_ = MyError::none;
  lastErrorMsg_.clear();
}

void GraphVisualizer::setError(MyError code, const std::string &msg) {
  lastError_ = code;
  lastErrorMsg_ = msg;
}

MyError GraphVisualizer::lastError() const { return lastError_; }

const std::string &GraphVisualizer::lastErrorMessage() const {
  return lastErrorMsg_;
}

// FIXME[Dkay] Bro. I'm tilted as fuck. I have NOT taught you like that. Why
// the fuck do you have a 300+ LOC function?
bool GraphVisualizer::buildCombinedGraph(Module &module,
                                         const std::string &runtimeLogFile) {
  // FIXME[Dkay]: if you need to reset those fields, then why do you store them
  // as fields and not locals?
  nodes_.clear(); // TODO[flops]: Make reset method and use it there
  runtimeValues_.clear();
  functionToEntryNode_.clear();
  runtimeValuesLoaded_ = false;

  // FIXME[Dkay]: i dont want logging in production mode. make it turnable-off
  // with defines, or some logging lib
  std::cout << "Building combined CFG + Def-Use graph...\n";

  if (!runtimeLogFile.empty()) {
    runtimeValuesLoaded_ = loadRuntimeValues(
        runtimeLogFile); // TODO [Dkay] Why return value is used only for
                         // logging? What if its broken? You just continue
                         // execution
    if (runtimeValuesLoaded_) {
      std::cout << "  Loaded " << runtimeValues_.size() << " runtime values\n";
    }
  }

  // FIXME[Dkay]: Why this is not a method?
  // create nodes for all functions, basic blocks, instructions, arguments
  for (auto &function : module) {
    if (function.isDeclaration())
      continue;
    std::string funcName = function.getName().str();

    for (auto &arg : function.args()) {
      std::string nodeId = getNodeId(&arg);
      GraphNode node; // [flops]: Use = designated initializer GraphNode node =
                      // {.id=nodeId, ...};

      // FIXME[Dkay]: At the point of this comment you have a GraphNode instance
      // with broken invariants, please, learn more about classes and
      // constructors before using them
      node.id = nodeId;
      node.label = getValueLabel(&arg);
      node.type = "argument";
      node.nodeType = NodeType::Argument;
      node.value = &arg;
      node.functionName = funcName;

      // FIXME[Dkay]: Why this is not a method? + What is happend here is
      // unclear to me. Please, work on architecture of your solution
      //
      // runtime value for args
      if (runtimeValuesLoaded_) {
        std::string argName = arg.getName().str();
        std::vector<std::string> possibleKeys = {
            nodeId, funcName + "_%" + argName, "%" + argName, argName};

        // TODO[Dkay]: As I said above its unclear to me, but you probably can
        // use some regular expressions here
        for (const auto &key : possibleKeys) {
          auto it = runtimeValues_.find(key);
          if (it != runtimeValues_.end() && !it->second.empty()) {
            node.runtimeValue = it->second;
            node.hasRuntimeValue = true;
            node.label = node.label + "    VALUE=" + it->second;
            break;
          }
        }
      }

      nodes_[nodeId] = node;
    }

    for (auto &block : function) {
      std::string blockId = getNodeId(&block);
      basicBlockCount_++;

      for (auto &instr : block) {
        std::string instrId = getNodeId(&instr);
        GraphNode node;
        node.id = instrId;
        node.label = getInstructionLabel(instr);
        node.type = getInstructionType(&instr);
        node.nodeType = NodeType::Instruction;
        node.isTerminator = instr.isTerminator();
        node.value = &instr;
        node.ownerBlock = &block;
        node.functionName = funcName;

        if (runtimeValuesLoaded_) {
          std::string instrName = getInstructionName(instr);
          std::vector<std::string> possibleKeys = {
              instrId,
              funcName + "_%" + instr.getName().str(),
              funcName + "::" + instrName,
              instrName,
              "%" + instr.getName().str(),
              getShortInstructionLabel(node)};

          for (const auto &key : possibleKeys) {
            auto it = runtimeValues_.find(key);
            if (it != runtimeValues_.end() && !it->second.empty()) {
              node.runtimeValue = it->second;
              node.hasRuntimeValue = true;

              std::string instrText = getInstructionLabel(instr);
              while (!instrText.empty() &&
                     (instrText.back() == '\n' || instrText.back() == ' ')) {
                instrText.pop_back();
              }
              node.label = instrText + "    VALUE=" + it->second;
              break;
            }
          }
        }

        for (unsigned i = 0; i < instr.getNumOperands(); i++) {
          Value *operand = instr.getOperand(i);

          if (isa<BasicBlock>(operand) || isa<MetadataAsValue>(operand)) {
            continue;
          }

          bool isConst = isa<ConstantInt>(operand) || isa<ConstantFP>(operand);

          std::string baseId = getNodeId(operand);
          // i wanna make constants inside function blocks
          // it looks much prettier
          std::string operandId = isConst ? (funcName + "::" + baseId) : baseId;

          if (isConst) {
            if (nodes_.find(operandId) == nodes_.end()) {
              GraphNode constNode;
              constNode.id = operandId;
              constNode.label = getValueLabel(operand);
              constNode.type = "constant";
              constNode.nodeType = NodeType::Constant;
              constNode.value = operand;
              constNode.functionName = funcName;

              if (runtimeValuesLoaded_) {
                std::vector<std::string> keys;
                keys.push_back(operandId);
                keys.push_back(baseId);
                keys.push_back(getValueLabel(operand));
                if (auto *ci = dyn_cast<ConstantInt>(operand)) {
                  keys.push_back(std::to_string(ci->getSExtValue()));
                }

                for (const auto &k : keys) {
                  auto it = runtimeValues_.find(k);
                  if (it != runtimeValues_.end() && !it->second.empty()) {
                    constNode.runtimeValue = it->second;
                    constNode.hasRuntimeValue = true;
                    constNode.label =
                        constNode.label + "    VALUE=" + it->second;
                    break;
                  }
                }
              }

              nodes_[operandId] = constNode;
            }
          }

          node.operands.push_back(operandId);
        }

        nodes_[instrId] = node;
      }
    }
  }

  // build edges. i really fucked up here
  int callOrderCounter = 0;

  // for avoiding duplicating
  std::set<std::pair<std::string, std::string>> cfgSeen;

  for (auto &function : module) {
    if (function.isDeclaration())
      continue;

    // find entry id for call edges (not PHI nodes)
    BasicBlock &entryBlock = function.getEntryBlock();
    for (auto &instr : entryBlock) {
      if (!isa<PHINode>(&instr)) {
        functionToEntryNode_[&function] = getNodeId(&instr);
        break;
      }
    }

    for (auto &block : function) {
      std::string prevId;
      bool havePrev = false;

      for (auto &instr : block) {
        std::string curId = getNodeId(&instr);
        if (havePrev) {
          if (cfgSeen.insert({prevId, curId}).second) {
            nodes_[prevId].cfgSuccessors.push_back(curId);
          }
        }
        prevId = curId;
        havePrev = true;
      }

      if (auto *terminator = block.getTerminator()) {
        std::string termId = getNodeId(terminator);

        for (unsigned i = 0; i < terminator->getNumSuccessors(); i++) {
          BasicBlock *succ = terminator->getSuccessor(i);
          Instruction *firstInstr = succ->getFirstNonPHI();

          if (!firstInstr)
            continue;

          std::string firstId = getNodeId(firstInstr);
          if (cfgSeen.insert({termId, firstId}).second) {
            nodes_[termId].cfgSuccessors.push_back(firstId);
          }
        }
      }

      // def-use + call edges
      for (auto &instr : block) {
        std::string instrId = getNodeId(&instr);
        for (const auto &operandId : nodes_[instrId].operands) {
          if (nodes_.find(operandId) != nodes_.end()) {
            nodes_[operandId].defUseSuccessors.push_back(instrId);
          }
        }

        if (auto *callInst = dyn_cast<CallInst>(&instr)) {
          if (Function *calledFunc = callInst->getCalledFunction()) {
            if (!calledFunc->isDeclaration()) {
              CallEdge e;
              e.callee = calledFunc;
              e.callSiteId = instrId;
              e.callOrder = ++callOrderCounter;
              callEdges_.push_back(e);
            }
          }
        }
      }
    }
  }

  std::cout << "  Nodes: " << nodes_.size() << "\n";
  std::cout << "  Calls: " << callEdges_.size() << "\n";
  return true;
}

bool GraphVisualizer::loadRuntimeValues(const std::string &logFile) {
  std::ifstream log(logFile);
  if (!log.is_open()) {
    setError(MyError::cannot_open_file, "can't open runtime log: " + logFile);
    return false;
  }

  std::string line;
  int cnt = 0;
  int badLines = 0;

  // TODO [Dkay]: You can use Json and save yourself from weird parsing. LLVM
  // also has json lib.
  while (std::getline(log, line)) {
    if (line.empty())
      continue;

    size_t p = line.rfind(':');
    if (p == std::string::npos)
      continue;

    std::string key = line.substr(0, p);
    std::string value = line.substr(p + 1);

    // kill fucking whitespaces
    key.erase(0, key.find_first_not_of(" \t\r\n"));
    key.erase(key.find_last_not_of(" \t\r\n") + 1);
    value.erase(0, value.find_first_not_of(" \t\r\n"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);
    if (key.empty() || value.empty()) {
      badLines++;
      continue;
    }
    runtimeValues_[key] = value;
    cnt++;
  }
  log.close();
  if (cnt > 0) {
    return true;
  }
  if (badLines > 0) {
    setError(MyError::runtime_log_parse,
             "runtime log has no valid key:value lines: " + logFile);
  } else {
    setError(MyError::runtime_log_parse, "runtime log is empty: " + logFile);
  }
  return false;
}

std::string GraphVisualizer::getNodeId(Value *value) {
  if (!value) {
    setError(MyError::bad_input, "value is null");
    return "";
  }

  std::stringstream ss;

  if (Instruction *instr = dyn_cast<Instruction>(value)) {
    // get parent basic block, then parent function
    Function *func = instr->getParent()->getParent();
    std::string funcName = func->getName().str();
    if (instr->hasName()) {
      return funcName + "_%" + instr->getName().str();
    } else {
      ss << funcName << "_%inst_" << std::hex << (uintptr_t)instr;
      return ss.str();
    }
  } else if (Argument *arg = dyn_cast<Argument>(value)) {
    Function *func = arg->getParent();
    return func->getName().str() + "_%" + arg->getName().str();
  } else if (ConstantInt *ci = dyn_cast<ConstantInt>(value)) {
    ss << "const_" << ci->getSExtValue();
    return ss.str();
  } else {
    ss << "val_" << std::hex << reinterpret_cast<uintptr_t>(value);
    return ss.str();
  }
}

std::string GraphVisualizer::getValueLabel(Value *value) {
  if (!value) {
    setError(MyError::bad_input, "value is null");
    return "";
  }

  std::string str;
  raw_string_ostream rso(str);

  if (ConstantInt *ci = dyn_cast<ConstantInt>(value)) {
    // i32 5 VALUE=5
    ci->getType()->print(rso);
    rso << " " << ci->getSExtValue();
  } else if (ConstantFP *cf = dyn_cast<ConstantFP>(value)) {
    // float 3.14 VALUE=3.14
    cf->getType()->print(rso);
    rso << " ";
    if (cf->getType()->isFloatTy()) {
      rso << cf->getValueAPF().convertToFloat();
    } else {
      rso << cf->getValueAPF().convertToDouble();
    }
  } else if (Argument *arg = dyn_cast<Argument>(value)) {
    // i32 %a (arg0) VALUE=...
    arg->getType()->print(rso);
    rso << " %" << arg->getName().str() << " (arg" << arg->getArgNo() << ")";
  } else if (Instruction *instr = dyn_cast<Instruction>(value)) {
    return getInstructionLabel(*instr);
  } else if (BasicBlock *bb = dyn_cast<BasicBlock>(value)) {
    return getBasicBlockLabel(*bb);
  } else {
    value->getType()->print(rso);
    rso << " ";
    value->printAsOperand(rso, false);
  }
  return rso.str();
}

std::string GraphVisualizer::getInstructionType(Instruction *instr) const {
  if (!instr) {
    return "unknown";
  }

  switch (instr->getOpcode()) {
  case Instruction::Add:
    return "add";
  case Instruction::Sub:
    return "sub";
  case Instruction::Mul:
    return "mul";

  case Instruction::UDiv:
  case Instruction::SDiv:
    return "div";

  case Instruction::URem:
  case Instruction::SRem:
    return "rem";

  case Instruction::Shl:
    return "shl";
  case Instruction::LShr:
    return "lshr";
  case Instruction::AShr:
    return "ashr";

  case Instruction::And:
    return "and";
  case Instruction::Or:
    return "or";
  case Instruction::Xor:
    return "xor";

  case Instruction::Alloca:
    return "alloca";
  case Instruction::Load:
    return "load";
  case Instruction::Store:
    return "store";

  case Instruction::Br:
    return "br";
  case Instruction::Ret:
    return "ret";

  case Instruction::Call:
    return "call";

  case Instruction::Select:
    return "select";

  case Instruction::GetElementPtr:
    return "gep";

  default:
    break;
  }

  if (isa<ICmpInst>(instr)) {
    return "icmp";
  }
  if (isa<FCmpInst>(instr)) {
    return "fcmp";
  }
  if (isa<PHINode>(instr)) {
    return "phi";
  }
  if (isa<CastInst>(instr)) {
    return "cast";
  }

  return instr->getOpcodeName();
}

std::string GraphVisualizer::getInstructionLabel(Instruction &instr) const {
  std::string str;
  raw_string_ostream rso(str);
  instr.print(rso);
  return rso.str();
}

std::string GraphVisualizer::getBasicBlockLabel(BasicBlock &block) const {
  std::string str;
  raw_string_ostream rso(str);
  if (block.hasName()) {
    rso << block.getName().str();
  } else {
    // in future i can use basic blocks, but now it looks worse
    if (Function *func = block.getParent()) {
      rso << func->getName().str() << "_BB";
    } else {
      rso << "BB";
    }
  }
  int count = 0;
  for (auto &instr : block) {
    if (!isa<PHINode>(&instr))
      count++;
  }
  rso << " (" << count << " instrs)";
  return rso.str();
}

bool GraphVisualizer::exportToDot(const std::string &filename) {
  std::ofstream out(filename);
  if (!out.is_open()) {
    setError(MyError::cannot_open_file, "cannot open dot output: " + filename);
    return false;
  }

  std::cout << "Exporting to DOT: " << filename << "\n";

  // graph header
  out << "digraph CombinedCFGDefUse {\n";
  out << "  rankdir=TB;\n";
  out << "  compound=true;\n";
  out << "  nodesep=0.5;\n";
  out << "  ranksep=0.8;\n";
  out << "  node [fontname=\"Courier New\", fontsize=10];\n";
  out << "  edge [fontname=\"Arial\", fontsize=9];\n\n";
  out << "  // ========== BASIC BLOCKS (Grouped by Function) ==========\n";

  std::map<std::string, std::vector<std::string>> funcToNodes;
  std::map<std::string, std::vector<std::string>> funcToArguments;
  std::map<std::string, std::vector<std::string>> funcToConstants;

  for (const auto &pair : nodes_) {
    const GraphNode &node = pair.second;
    if (node.functionName.empty()) {
      continue;
    }
    if (node.nodeType == NodeType::Argument) {
      funcToArguments[node.functionName].push_back(pair.first);
    } else if (node.nodeType == NodeType::Constant) {
      funcToConstants[node.functionName].push_back(pair.first);
    } else if (node.nodeType == NodeType::Instruction) {
      funcToNodes[node.functionName].push_back(pair.first);
    }
  }

  for (const auto &funcPair : funcToNodes) {
    std::string funcName = funcPair.first;

    out << "  subgraph \"cluster_" << funcName << "\" {\n";
    out << "    label=\"" << escapeForDot(funcName) << "()\";\n";
    out << "    style=filled;\n";
    out << "    fillcolor=\"#f0f8ff\";\n";
    out << "    color=\"#3366cc\";\n";
    out << "    penwidth=2;\n";
    out << "    fontsize=11;\n";
    out << "    labelloc=\"t\";\n\n";

    out << "    // Arguments\n";
    out << "    node [shape=ellipse, style=filled, fillcolor=\"#d0e8ff\"];\n";
    if (funcToArguments.find(funcName) != funcToArguments.end()) {
      for (const auto &argId : funcToArguments[funcName]) {
        const GraphNode &node = nodes_.at(argId);
        out << "    \"" << argId << "\" [label=\"" << escapeForDot(node.label)
            << "\"];\n";
      }
    }

    out << "\n    // Constants\n";
    out << "    node [shape=oval, style=filled, fillcolor=\"#e0e0e0\", "
           "fontsize=8, height=0.3, width=0.5];\n";
    if (funcToConstants.find(funcName) != funcToConstants.end()) {
      for (const auto &constId : funcToConstants[funcName]) {
        const GraphNode &node = nodes_.at(constId);
        out << "    \"" << constId << "\" [label=\"" << escapeForDot(node.label)
            << "\"];\n";
      }
    }

    std::map<std::string, std::vector<const GraphNode *>> instrsByBlock;
    for (const auto &nodeId : funcPair.second) {
      const GraphNode &node = nodes_.at(nodeId);
      if (node.ownerBlock) {
        std::string blockName = getBasicBlockLabel(*node.ownerBlock);
        instrsByBlock[blockName].push_back(&node);
      } else {
        instrsByBlock["unknown"].push_back(&node);
      }
    }

    for (const auto &bbPair : instrsByBlock) {
      const auto &bbInstrs = bbPair.second;
      for (const GraphNode *n : bbInstrs) {
        std::string shape = "box";
        std::string fill = "white";
        std::string color = "black";
        std::string style = "filled";

        if (n->nodeType == NodeType::Instruction && n->isTerminator) {
          shape = "box";
          fill = "#ffe0e0";
          color = "#cc0000";
          style = "filled";
        } else if (n->type == "phi") {
          shape = "hexagon";
          fill = "#f0e0ff";
          color = "#800080";
          style = "filled";
        } else if (n->type == "icmp" || n->type == "fcmp") {
          shape = "diamond";
          fill = "#fff2cc";
          color = "#ff9900";
          style = "filled";
        } else if (n->type == "call") {
          shape = "parallelogram";
          fill = "#d9ffff";
          color = "#1aa3a3";
          style = "filled";
        }

        out << "      \"" << n->id << "\" [shape=" << shape
            << ", style=" << style << ", fillcolor=\"" << fill << "\", color=\""
            << color << "\", label=\"" << escapeForDot(n->label) << "\"];\n";
      }
    }

    out << "  }\n\n";
  }

  std::set<std::pair<std::string, std::string>> allEdges;

  out << "\n  // ========== CFG EDGES (Control Flow) ==========\n";
  out << "  edge [color=\"#0066cc\", penwidth=2.5, style=solid, "
         "arrowhead=normal];\n";

  for (const auto &pair : nodes_) {
    const GraphNode &node = pair.second;
    for (const auto &succId : node.cfgSuccessors) {
      out << "  \"" << node.id << "\" -> \"" << succId << "\";\n";
      allEdges.insert({node.id, succId});
    }
  }

  out << "\n  // ========== DEF-USE EDGES (Data Flow) ==========\n";
  out << "  edge [color=\"black\", penwidth=1.2, style=dashed, "
         "arrowhead=vee];\n";

  for (const auto &pair : nodes_) {
    const GraphNode &node = pair.second;
    for (const auto &succId : node.defUseSuccessors) {
      out << "  \"" << node.id << "\" -> \"" << succId << "\";\n";
      allEdges.insert({node.id, succId});
    }
  }

  if (!callEdges_.empty()) {
    out << "\n  // ========== FUNCTION CALL EDGES ==========\n";
    out << "  edge [color=\"#cc3366\", penwidth=2.0, style=\"bold\", "
           "arrowhead=\"vee\"];\n";

    for (const auto &call : callEdges_) {
      auto it = functionToEntryNode_.find(call.callee);
      if (it != functionToEntryNode_.end()) {
        out << "  \"" << call.callSiteId << "\" -> \"" << it->second
            << "\" [label=\"call #" << call.callOrder
            << "\", fontsize=9, fontcolor=\"#cc3366\"];\n";
      }
    }
  }

  out << "\n  // ========== CONSTANT/ARGUMENT INPUT EDGES ==========\n";
  out << "  edge [color=\"gray\", penwidth=1, style=dotted, arrowhead=odot];\n";

  for (const auto &pair : nodes_) {
    const GraphNode &node = pair.second;
    if (node.nodeType != NodeType::Instruction)
      continue;

    for (const auto &operandId : node.operands) {
      auto it = nodes_.find(operandId);
      if (it == nodes_.end())
        continue;

      if (it->second.nodeType == NodeType::Constant ||
          it->second.nodeType == NodeType::Argument) {
        // avoid duplicating edges
        if (allEdges.find({operandId, node.id}) == allEdges.end()) {
          out << "  \"" << operandId << "\" -> \"" << node.id << "\";\n";
          allEdges.insert({operandId, node.id});
        }
      }
    }
  }

  out << "\n  // ========== LEGEND ==========\n";
  out << "  subgraph \"cluster_legend\" {\n";
  out << "    label=\"Legend\";\n";
  out << "    labelloc=\"t\";\n";
  out << "    fontsize=11;\n";
  out << "    color=\"#888888\";\n";
  out << "    style=\"rounded,filled\";\n";
  out << "    fillcolor=\"#ffffff\";\n";
  out << "    rank=\"min\";\n";
  out << "    node [fontname=\"Arial\", fontsize=10, shape=box, "
         "style=\"rounded,filled\", fillcolor=\"#ffffff\"];\n";
  out << "    \"leg_call_a\" [label=\"Call\"];\n";
  out << "    \"leg_call_b\" [label=\"\"];\n";
  out << "    \"leg_du_a\" [label=\"Def-Use\"];\n";
  out << "    \"leg_du_b\" [label=\"\"];\n";
  out << "    \"leg_cfg_a\" [label=\"CFG\"];\n";
  out << "    \"leg_cfg_b\" [label=\"\"];\n";
  out << "    { rank=same; \"leg_call_a\"; \"leg_call_b\"; \"leg_du_a\"; "
         "\"leg_du_b\"; \"leg_cfg_a\"; \"leg_cfg_b\"; }\n";
  out << "    \"leg_call_a\" -> \"leg_call_b\" [color=\"#cc3366\", "
         "penwidth=2.0, style=\"bold\", arrowhead=\"vee\", label=\"call "
         "edges\", fontcolor=\"#cc3366\", fontsize=9];\n";
  out << "    \"leg_du_a\" -> \"leg_du_b\" [color=\"black\", penwidth=1.2, "
         "style=dashed, arrowhead=vee, label=\"Def-Use edges\", "
         "fontcolor=\"black\", fontsize=9];\n";
  out << "    \"leg_cfg_a\" -> \"leg_cfg_b\" [color=\"#0066cc\", penwidth=2.5, "
         "style=solid, arrowhead=normal, label=\"CFG edges\", "
         "fontcolor=\"#0066cc\", fontsize=9];\n";
  out << "  }\n\n";

  out << "}\n";
  out.close();

  return true;
}

std::string GraphVisualizer::escapeForDot(const std::string &text) const {
  std::string result;

  for (char c : text) {
    switch (c) {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    case '<':
      result += "\\<";
      break;
    case '>':
      result += "\\>";
      break;
    case '{':
      result += "\\{";
      break;
    case '}':
      result += "\\}";
      break;
    case '|':
      result += "\\|";
      break;
    default:
      result += c;
    }
  }
  if (result.length() > 140) {
    result = result.substr(0, 137) + "...";
  }

  return result;
}

std::string GraphVisualizer::getInstructionName(Instruction &instr) const {
  std::string str;
  raw_string_ostream rso(str);

  if (instr.hasName()) {
    rso << "%" << instr.getName().str();
  } else {
    rso << getInstructionType(&instr);
  }

  return rso.str();
}

std::string GraphVisualizer::getShortInstructionLabel(const GraphNode &node) {
  if (!node.value) {
    setError(MyError::bad_input, "node.value is null");
    return "";
  }

  Instruction *instr = dyn_cast<Instruction>(node.value);
  if (!instr)
    return node.label;

  std::string str;
  raw_string_ostream rso(str);

  instr->print(rso);

  if (!node.runtimeValue.empty()) {

    std::string cleanLabel = rso.str();
    if (!cleanLabel.empty() && cleanLabel.back() == '\n') {
      cleanLabel.pop_back();
    }
    cleanLabel += "    VALUE=" + node.runtimeValue;
    return cleanLabel;
  }
  return rso.str();
}

void GraphVisualizer::calcStatistics() {
  instrCount_ = 0;
  constCount_ = 0;
  argCount_ = 0;
  bbCount_ = basicBlockCount_;
  cfgEdges_ = 0;
  duEdges_ = 0;
  runtimeCount_ = runtimeValues_.size();
  for (const auto &pair : nodes_) {
    if (pair.second.nodeType == NodeType::Instruction)
      instrCount_++;
    if (pair.second.nodeType == NodeType::Constant)
      constCount_++;
    if (pair.second.nodeType == NodeType::Argument)
      argCount_++;

    cfgEdges_ += pair.second.cfgSuccessors.size();
    duEdges_ += pair.second.defUseSuccessors.size();
  }
}

void GraphVisualizer::printStatistics() {
  calcStatistics();
  std::cout << "\n=== GRAPH STATISTICS ===\n"
            << "Basic Blocks:      " << bbCount_ << "\n"
            << "Instructions:      " << instrCount_ << "\n"
            << "Arguments:         " << argCount_ << "\n"
            << "Constants:         " << constCount_ << "\n"
            << "CFG Edges:         " << cfgEdges_ << "\n"
            << "Def-Use Edges:     " << duEdges_ << "\n"
            << "Runtime Values:    " << runtimeCount_ << "\n"
            << "========================\n";
}
