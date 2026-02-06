#include "GraphVisualizer.h"
#include "Instrumentation.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <sys/types.h>
#include <system_error>

static bool endsWith(const std::string &str, const std::string &suffix);
static void printHelp(const char *prog);
static bool runCmd(const std::string &cmd);
static void makeDir(const std::string &path);
static std::string getOptMem2RegCmd();
static bool emitllFromC(const std::string &cFile, const std::string &outLl);
static bool mem2reg(const std::string &inLl, const std::string &outLl);
static bool instrumentLl(const std::string &inLl, const std::string &outLl);
static bool buildAndRun(const std::string &instrumentedLl,
                        const std::string &outRuntimeLog,
                        const std::string &outExe);
static bool loadModule(const std::string &llFile,
                       std::unique_ptr<llvm::Module> &outModule,
                       llvm::LLVMContext &ctx);
static bool buildGraph(const std::string &llFile, const std::string &runtimeLog,
                       const std::string &outDot);
static std::string baseNameNoExt(const std::string &path);
static bool pickInputIr(const std::string &inputFile,
                        const std::filesystem::path &rawLlPath,
                        std::string &irForGraph);
static int runFullGraphAnalysis(const std::string &inputFile,
                                const std::string &outDir);
static int handleGraphCmd(int argc, char **argv);

enum Error {
  None,
  BadInput,
  InstrumentationFailed,
  RunFailed,
  GraphBuildFailed,
};

int main(int argc, char **argv) {
  if (argc < 2) {
    printHelp(argv[0]);
    return EXIT_FAILURE;
  }

  std::string cmd = argv[1];

  if (cmd == "--help" || cmd == "-h" || cmd == "help") {
    printHelp(argv[0]);
    return 0;
  }

  if (!cmd.empty() && cmd[0] == '-') {
    if (cmd == "-analyze") {
      if (argc < 3) {
        std::cerr << "error: -analyze needs <input.c|input.ll>\n";
        return EXIT_FAILURE;
      }
      std::string input = argv[2];
      std::string outDir = (argc >= 4) ? argv[3] : "";
      return runFullGraphAnalysis(input, outDir);
    }

    if (cmd == "-emit-llvm") {
      if (argc < 4) {
        std::cerr << "error: -emit-llvm <file.c> <out.ll>\n";
        return EXIT_FAILURE;
      }
      return emitllFromC(argv[2], argv[3]) ? 0 : 2;
    }

    if (cmd == "-mem2reg") {
      if (argc < 4) {
        std::cerr << "error: -mem2reg <in.ll> <out.ll>\n";
        return EXIT_FAILURE;
      }
      return mem2reg(argv[2], argv[3]) ? 0 : 2;
    }

    if (cmd == "-instrument") {
      if (argc < 4) {
        std::cerr << "error: -instrument <in.ll> <out.ll>\n";
        return EXIT_FAILURE;
      }
      return instrumentLl(argv[2], argv[3]) ? 0 : 2;
    }

    if (cmd == "-run") {
      if (argc < 4) {
        std::cerr << "error: -run <instrumented.ll> <runtime.log> [out_exe]\n";
        return EXIT_FAILURE;
      }
      std::string exe = (argc >= 5) ? argv[4] : "";
      return buildAndRun(argv[2], argv[3], exe) ? 0 : 2;
    }

    if (cmd == "-graph") {
      return handleGraphCmd(argc, argv);
    }

    std::cerr << "error: unknown option: " << cmd << "\n";
    printHelp(argv[0]);
    return EXIT_FAILURE;
  }

  std::cerr << "error: unknown command: " << cmd << "\n";
  printHelp(argv[0]);
  return EXIT_FAILURE;
}

static bool endsWith(const std::string &str, const std::string &suffix) {
  return llvm::StringRef(str).ends_with(suffix);
}

static void printHelp(const char *prog) {
  const char *p = (prog && prog[0]) ? prog : "defuse-analyzer";

  std::cout << "LLVM Def-Use Graph Builder\n\n"
            << "Usage:\n"
            << "  " << p << " --help\n"
            << "  " << p << " -analyze <input.c|input.ll> [out_dir]\n"
            << "\n"
            << "One-step full pipeline:\n"
            << "  -analyze <file.c|file.ll> [out_dir]\n"
            << "    C->LL -> mem2reg -> instrument -> run -> graph\n"
            << "\n"
            << "Separate steps:\n"
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

static void makeDir(const std::string &path) {
  std::error_code ec = llvm::sys::fs::create_directories(path);
  if (ec) {
    std::cerr << "error: cannot create directory: " << path << "\n";
  }
}

static std::string getOptMem2RegCmd() {
#ifdef DEFUSE_OPT_PATH
  const std::string optPath = DEFUSE_OPT_PATH;
#else
  const std::string optPath = "opt";
#endif
  if (runCmd("\"" + optPath + "\" -S -passes=mem2reg --help > /dev/null 2>&1"))
    return "\"" + optPath + "\" -S -passes=mem2reg";
  return "\"" + optPath + "\" -S -mem2reg";
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
  const std::filesystem::path inPath(inLl);
  const std::filesystem::path outPath(outLl);

  const std::string cmd =
      optCmd + " \"" + inPath.string() + "\" -o \"" + outPath.string() + "\"";
  return runCmd(cmd);
}

static bool instrumentLl(const std::string &inLl, const std::string &outLl) {
  Instrumentation inst;
  llvm::LLVMContext context;
  llvm::SMDiagnostic error;
  std::unique_ptr<llvm::Module> module = parseIRFile(inLl, error, context);
  if (!module) {
    llvm::errs() << "Error: Failed to load module: " << inLl << "\n";
    return false;
  }
  inst.instrumentModule(*module);
  std::error_code ec;
  llvm::raw_fd_ostream out(outLl, ec);
  if (ec) {
    llvm::errs() << "Error: Cannot open output file: " << outLl << "\n";
    return false;
  }
  module->print(out, nullptr);
  return true;
}

static bool buildAndRun(const std::string &instrumentedLl,
                        const std::string &outRuntimeLog,
                        const std::string &outExe) {
  if (outExe.empty()) {
    std::cerr << "error: output exe path is empty\n";
    return false;
  }

  const std::filesystem::path exePath(outExe);
  std::filesystem::path runtimeC =
      std::filesystem::path("runtime") / "core_runtime.c";
  if (!std::filesystem::exists(runtimeC)) {
    runtimeC = exePath.parent_path() / "runtime" / "core_runtime.c";
  }

  if (!std::filesystem::exists(runtimeC)) {
    std::cerr << "error: can't find runtime/core_runtime.c\n";
    return false;
  }
  const std::string buildCmd = "clang -O0 \"" + runtimeC.string() + "\" \"" +
                               instrumentedLl + "\" -o \"" + exePath.string() +
                               "\"";
  if (!runCmd(buildCmd)) {
    std::cerr << "error: failed to compile instrumented program\n";
    return false;
  }

  const std::string runCmdLine =
      "\"" + exePath.string() + "\" > \"" + outRuntimeLog + "\" 2>/dev/null";
  // DO NOT RETURN A NON-ZERO VALUE FROM MAIN
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
  if (!outModule) { // TODO[Dkay]: Use some logging + return macro, since I dont
                    // want to have debug output in production mode return
                    // std::optional / std::expected in such cases
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

  // vis.printStatistics(); // TODO Make debugging mode

  if (!vis.exportToDot(outDot)) {
    std::cerr << "error: exportToDot failed\n";
    return false;
  }

  if (runCmd("which dot > /dev/null 2>&1")) {
    std::string base = outDot;
    if (endsWith(base, ".dot")) {
      base = base.substr(0, base.size() - 4);
    }

    runCmd("dot -Tpng \"" + outDot + "\" -o \"" + base + ".png\" 2>/dev/null");
    runCmd("dot -Tsvg \"" + outDot + "\" -o \"" + base + ".svg\" 2>/dev/null");
  } else {
    std::cerr << "warn: 'dot' not found, skipping png/svg export\n";
  }
  return true;
}

static std::string baseNameNoExt(const std::string &path) {
  llvm::StringRef p(path);
  return llvm::sys::path::stem(p).str();
}

static bool pickInputIr(const std::string &inputFile,
                        const std::filesystem::path &rawLlPath,
                        std::string &irForGraph) {
  if (endsWith(inputFile, ".c")) {
    std::cout << "[1/5] clang -> LLVM IR\n";
    if (!emitllFromC(inputFile, rawLlPath.string())) {
      std::cerr << "error: clang failed\n";
      return false;
    }
    irForGraph = rawLlPath.string();
    return true;
  }

  if (endsWith(inputFile, ".ll")) {
    irForGraph = inputFile;
    return true;
  }

  std::cerr << "error: input must be .c or .ll\n";
  return false;
}

static int runFullGraphAnalysis(const std::string &inputFile,
                                const std::string &outDir) {
  std::string name = baseNameNoExt(inputFile);
  const std::filesystem::path root =
      outDir.empty() ? (std::filesystem::path("outputs") / name)
                     : std::filesystem::path(outDir);
  const std::filesystem::path llvmDir = root / "llvm";
  makeDir(root);
  makeDir(llvmDir);

  const std::filesystem::path rawLlPath = llvmDir / (name + ".ll");
  const std::filesystem::path mem2regLlPath = llvmDir / (name + "_m2r.ll");
  const std::filesystem::path instrumentedLlPath =
      llvmDir / (name + "_instrumented.ll");
  const std::filesystem::path runtimeLogPath = root / "runtime.log";
  const std::filesystem::path dotPath = root / "enhanced_graph.dot";
  const std::filesystem::path exePath = root / "program";

  std::string irForGraph;
  if (!pickInputIr(inputFile, rawLlPath, irForGraph)) {
    return Error::BadInput;
  }

  std::cout << "[2/5] mem2reg\n";
  if (mem2reg(irForGraph, mem2regLlPath.string())) {
    irForGraph = mem2regLlPath.string();
  } else {
    std::cerr << "warn: mem2reg failed, continue with original IR\n";
  }

  std::cout << "[3/5] instrument\n";
  if (!instrumentLl(irForGraph, instrumentedLlPath.string())) {
    std::cerr << "error: instrumentation failed\n";
    return Error::InstrumentationFailed;
  }

  std::cout << "[4/5] run instrumented program (collect runtime.log)\n";
  if (!buildAndRun(instrumentedLlPath.string(), runtimeLogPath.string(),
                   exePath.string())) {
    return Error::RunFailed;
  }

  std::cout << "[5/5] build graph (dot/png/svg)\n";
  if (!buildGraph(irForGraph, runtimeLogPath.string(), dotPath.string())) {
    return Error::GraphBuildFailed;
  }

  std::cout << "\nDone.\n";
  return 0;
}

static int handleGraphCmd(int argc, char **argv) {
  if (argc < 3) {
    std::cerr << "error: -graph <in.ll> [runtime.log] [out.dot]\n";
    return 1;
  }

  std::string inLl = argv[2];
  std::string rt = (argc >= 4) ? argv[3] : "";
  std::string outDot = (argc >= 5) ? argv[4] : "enhanced_graph.dot";

  return buildGraph(inLl, rt, outDot) ? 0 : 2;
}
