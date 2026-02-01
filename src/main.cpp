#include "../include/GraphVisualizer.h"
#include "../include/Instrumentation.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

static bool endsWith(const std::string &s, const std::string &suffix) {
  if (s.size() < suffix.size())
    return false;
  // extensions comparison
  return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static void printHelp() {
  std::cout << "LLVM Def-Use Graph Builder\n\n"
            << "Usage:\n"
            << "  ./bin/defuse-analyzer --help\n"
            << "  ./bin/defuse-analyzer -analyze <input.c|input.ll> [out_dir]\n"
            << "\n"
            << "One-step full pipeline:\n"
            << "  -analyze <file.c|file.ll> [out_dir]\n"
            << "    делает: C->LL (если нужно) -> mem2reg -> instrument -> run "
               "-> graph\n"
            << "\n"
            << "Separate steps (если хочется руками):\n"
            << "  -emit-llvm   <file.c>  <out.ll>\n"
            << "  -mem2reg     <in.ll>   <out.ll>\n"
            << "  -instrument  <in.ll>   <out.ll>\n"
            << "  -run         <instrumented.ll> <out_runtime.log> [out_exe]\n"
            << "  -graph       <in.ll>   [runtime.log] [out_dot]\n"
            << "\n";
}

static bool runCmd(const std::string &cmd) {
  int rc = std::system(cmd.c_str());
  return rc == 0;
}

static void ensureDir(const std::string &path) {
  std::string cmd = "mkdir -p \"" + path + "\"";
  (void)std::system(cmd.c_str());
}

static std::string getOptMem2RegCmd() {
  if (std::system("which opt > /dev/null 2>&1") != 0)
    return "";

  if (std::system("opt -S -passes=mem2reg --help > /dev/null 2>&1") == 0)
    return "opt -S -passes=mem2reg";

  // if new format not supported, fallback to old one
  return "opt -S -mem2reg";
}

static bool emitllFromC(const std::string &cFile, const std::string &outLl) {
  std::string cmd = "clang -S -emit-llvm -O0 -Xclang -disable-O0-optnone "
                    "-fno-discard-value-names "
                    "\"" +
                    cFile + "\" -o \"" + outLl + "\"";
  return runCmd(cmd);
}

static bool mem2reg(const std::string &inLl, const std::string &outLl) {
  std::string optCmd = getOptMem2RegCmd();
  if (optCmd.empty()) {
    std::cerr << "error: opt not found (install llvm)\n";
    return false;
  }
  std::string cmd = optCmd + " \"" + inLl + "\" -o \"" + outLl + "\"";
  return runCmd(cmd);
}

static bool instrumentll(const std::string &inLl, const std::string &outLl) {
  Instrumentation inst;
  return inst.instrumentModule(inLl, outLl);
}

static bool buildAndRun(const std::string &instrumentedLl,
                        const std::string &outRuntimeLog,
                        const std::string &outExe) {
  std::string exePath = outExe.empty() ? "program" : outExe;

  std::string buildCmd = "clang -O0 runtime/core_runtime.c \"" +
                         instrumentedLl + "\" -o \"" + exePath + "\"";
  if (!runCmd(buildCmd)) {
    std::cerr << "error: failed to compile instrumented program\n";
    return false;
  }

  std::string runCmdLine =
      "./" + outExe + " > " + outRuntimeLog + " 2>/dev/null";
  // DO NOT RETURN A NON-ZERO VALUE FROM MAIN, YOU WILL BE RAPED BY TOUCAN
  if (!runCmd(runCmdLine)) {
    std::cerr << "error: running instrumented program failed\n";
    return false;
  }
  return true;
}

static bool loadModule(const std::string &llFile,
                       std::unique_ptr<llvm::Module> &outModule,
                       llvm::LLVMContext &ctx) {
  llvm::SMDiagnostic err;
  outModule = llvm::parseIRFile(llFile, err, ctx);
  if (!outModule) {
    std::cerr << "error: can't read IR: " << llFile << "\n";
    err.print(llFile.c_str(), llvm::errs());
    return false;
  }
  return true;
}

static bool buildGraph(const std::string &llFile, const std::string &runtimeLog,
                       const std::string &outDot) {
  llvm::LLVMContext ctx;
  std::unique_ptr<llvm::Module> mod;
  if (!loadModule(llFile, mod, ctx))
    return false;

  GraphVisualizer vis;
  if (!vis.buildCombinedGraph(*mod, runtimeLog)) {
    std::cerr << "error: buildCombinedGraph failed\n";
    return false;
  }

  vis.printStatistics();

  if (!vis.exportToDot(outDot)) {
    std::cerr << "error: exportToDot failed\n";
    return false;
  }

  if (std::system("which dot > /dev/null 2>&1") == 0) {
    std::string png = outDot;
    std::string svg = outDot;

    if (endsWith(png, ".dot")) {
      png = png.substr(0, png.size() - 4) + ".png";
      svg = svg.substr(0, svg.size() - 4) + ".svg";
    } else {
      png += ".png";
      svg += ".svg";
    }
    runCmd("dot -Tpng \"" + outDot + "\" -o \"" + png + "\" 2>/dev/null");
    runCmd("dot -Tsvg \"" + outDot + "\" -o \"" + svg + "\" 2>/dev/null");
  }
  return true;
}

static std::string baseNameNoExt(const std::string &path) {
  std::string s = path;
  size_t slash = s.find_last_of("/\\");
  if (slash != std::string::npos)
    s = s.substr(slash + 1);
  size_t dot = s.find_last_of('.');
  if (dot != std::string::npos)
    s = s.substr(0, dot);
  return s;
}

static int doAnalyze(const std::string &inputFile, const std::string &outDir) {
  std::string name = baseNameNoExt(inputFile);
  std::string root = outDir.empty() ? ("outputs/" + name) : outDir;

  std::string llvmDir = root + "/llvm";
  ensureDir(root);
  ensureDir(llvmDir);

  std::string ll0 = llvmDir + "/" + name + ".ll";
  std::string ll1 = llvmDir + "/" + name + "_m2r.ll";
  std::string instLl = llvmDir + "/" + name + "_instrumented.ll";
  std::string rtLog = root + "/runtime.log";
  std::string dot = root + "/enhanced_graph.dot";
  std::string exe = root + "/program";

  std::string irForGraph;

  if (endsWith(inputFile, ".c")) {
    std::cout << "[1/5] clang -> LLVM IR\n";
    if (!emitllFromC(inputFile, ll0)) {
      std::cerr << "error: clang failed\n";
      return 2;
    }
    irForGraph = ll0;
  } else if (endsWith(inputFile, ".ll")) {
    irForGraph = inputFile;
  } else {
    std::cerr << "error: input must be .c or .ll\n";
    return 2;
  }

  std::cout << "[2/5] mem2reg\n";
  if (mem2reg(irForGraph, ll1)) {
    irForGraph = ll1;
  } else {
    std::cerr << "warn: mem2reg failed, continue with original IR\n";
  }

  std::cout << "[3/5] instrument\n";
  if (!instrumentll(irForGraph, instLl)) {
    std::cerr << "error: instrumentation failed\n";
    return 3;
  }

  std::cout << "[4/5] run instrumented program (collect runtime.log)\n";
  if (!buildAndRun(instLl, rtLog, exe)) {
    return 4;
  }

  std::cout << "[5/5] build graph (dot/png/svg)\n";
  if (!buildGraph(irForGraph, rtLog, dot)) {
    return 5;
  }

  std::cout << "\nDone.\n";
  std::cout << "Output folder: " << root << "\n";
  std::cout << "  IR:   " << irForGraph << "\n";
  std::cout << "  log:  " << rtLog << "\n";
  std::cout << "  dot:  " << dot << "\n";
  if (endsWith(dot, ".dot")) {
    std::cout << "  png:  " << dot.substr(0, dot.size() - 4) << ".png\n";
    std::cout << "  svg:  " << dot.substr(0, dot.size() - 4) << ".svg\n";
  }
  return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    printHelp();
    return 1;
  }

  std::string cmd = argv[1];

  if (cmd == "--help" || cmd == "-h" || cmd == "help") {
    printHelp();
    return 0;
  }

  if (!cmd.empty() && cmd[0] == '-') {
    if (cmd == "-analyze") {
      if (argc < 3) {
        std::cerr << "error: -analyze needs <input.c|input.ll>\n";
        return 1;
      }
      std::string input = argv[2];
      std::string outDir = (argc >= 4) ? argv[3] : "";
      return doAnalyze(input, outDir);
    }

    if (cmd == "-emit-llvm") {
      if (argc < 4) {
        std::cerr << "error: -emit-llvm <file.c> <out.ll>\n";
        return 1;
      }
      return emitllFromC(argv[2], argv[3]) ? 0 : 2;
    }

    if (cmd == "-mem2reg") {
      if (argc < 4) {
        std::cerr << "error: -mem2reg <in.ll> <out.ll>\n";
        return 1;
      }
      return mem2reg(argv[2], argv[3]) ? 0 : 2;
    }

    if (cmd == "-instrument") {
      if (argc < 4) {
        std::cerr << "error: -instrument <in.ll> <out.ll>\n";
        return 1;
      }
      return instrumentll(argv[2], argv[3]) ? 0 : 2;
    }

    if (cmd == "-run") {
      if (argc < 4) {
        std::cerr << "error: -run <instrumented.ll> <runtime.log> [out_exe]\n";
        return 1;
      }
      std::string exe = (argc >= 5) ? argv[4] : "";
      return buildAndRun(argv[2], argv[3], exe) ? 0 : 2;
    }

    if (cmd == "-graph") {
      if (argc < 3) {
        std::cerr << "error: -graph <in.ll> [runtime.log] [out.dot]\n";
        return 1;
      }
      std::string inLl = argv[2];
      std::string rt = (argc >= 4) ? argv[3] : "";
      std::string outDot = (argc >= 5) ? argv[4] : "enhanced_graph.dot";
      return buildGraph(inLl, rt, outDot) ? 0 : 2;
    }

    std::cerr << "error: unknown option: " << cmd << "\n";
    printHelp();
    return 1;
  }

  std::cerr << "error: unknown command: " << cmd << "\n";
  printHelp();
  return 1;
}