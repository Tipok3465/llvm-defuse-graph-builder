#!/usr/bin/env bash
set -euo pipefail

mkdir -p obj bin llvm logs outputs

LLVM_CONFIG=${LLVM_CONFIG:-llvm-config}
CXX=${CXX:-clang++}

OPT=${OPT:-opt}

if ! OPT_PATH="$(command -v "$OPT" 2>/dev/null)"; then
  echo "[build] error: opt not found. install llvm or set OPT=/path/to/opt" >&2
  exit 1
fi

CXXFLAGS="-std=c++17 -O0 -g -Wall -Wextra -Wpedantic -fno-exceptions -fno-rtti -Iinclude"
LLVM_CXXFLAGS="$($LLVM_CONFIG --cxxflags | sed 's/-std=c++[^ ]*//g')"
LLVM_LDFLAGS="$($LLVM_CONFIG --ldflags)"
LLVM_LIBS="$($LLVM_CONFIG --libs core irreader support analysis)"
LLVM_SYS="$($LLVM_CONFIG --system-libs)"

APP_DEFS="-DDEFUSE_OPT_PATH=\"${OPT_PATH}\""

rm -f obj/*.o

$CXX $CXXFLAGS $LLVM_CXXFLAGS $APP_DEFS -c src/main.cpp            -o obj/main.o
$CXX $CXXFLAGS $LLVM_CXXFLAGS $APP_DEFS -c src/GraphVisualizer.cpp -o obj/GraphVisualizer.o
$CXX $CXXFLAGS $LLVM_CXXFLAGS $APP_DEFS -c src/Instrumentation.cpp -o obj/Instrumentation.o

$CXX obj/*.o $LLVM_LDFLAGS $LLVM_LIBS $LLVM_SYS -o bin/defuse-analyzer

echo "[build] ok -> bin/defuse-analyzer (opt: ${OPT_PATH})"
