#!/usr/bin/env bash
set -euo pipefail

mkdir -p obj bin llvm logs outputs

LLVM_CONFIG=${LLVM_CONFIG:-llvm-config}
CXX=${CXX:-clang++}

# [flops]: OPT=${OPT:-opt}
# TODO[flops]: On build stage you also can spot where opt tool is located, abort build & force user to provide correct path and pass it as macro to the app 
# [flops]: (instead of std::system("which opt > /dev/null 2>&1") != 0)) in app runtime.

CXXFLAGS="-std=c++17 -O0 -g -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -Iinclude"
LLVM_CXXFLAGS="$($LLVM_CONFIG --cxxflags | sed 's/-std=c++[^ ]*//g')"
LLVM_LDFLAGS="$($LLVM_CONFIG --ldflags)"
LLVM_LIBS="$($LLVM_CONFIG --libs core irreader support analysis)"
LLVM_SYS="$($LLVM_CONFIG --system-libs)"

# TODO[DKay]: Why not to use incremental build system like Makefile here?
#
rm -f obj/*.o

$CXX $CXXFLAGS $LLVM_CXXFLAGS -Iinclude -c src/main.cpp            -o obj/main.o
$CXX $CXXFLAGS $LLVM_CXXFLAGS -Iinclude -c src/GraphVisualizer.cpp -o obj/GraphVisualizer.o
$CXX $CXXFLAGS $LLVM_CXXFLAGS -Iinclude -c src/Instrumentation.cpp -o obj/Instrumentation.o

$CXX obj/*.o $LLVM_LDFLAGS $LLVM_LIBS $LLVM_SYS -o bin/defuse-analyzer

echo "[build] ok -> bin/defuse-analyzer"
