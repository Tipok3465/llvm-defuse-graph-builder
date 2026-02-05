#!/usr/bin/env bash
set -euo pipefail

# TODO[flops]: This script is meant to test tests/simple, so better move it to tests/simple

mkdir -p llvm logs outputs/simple/static outputs/simple/runtime

CC=${CC:-clang}
OPT=${OPT:-opt}

[ -x bin/defuse-analyzer ] || { echo "ERROR: bin/defuse-analyzer not found. Run ./build.sh"; exit 1; }
command -v $CC >/dev/null 2>&1 || { echo "ERROR: clang not found"; exit 1; }
command -v $OPT   >/dev/null 2>&1 || { echo "ERROR: opt not found"; exit 1; }

SRC=tests/simple/main.c
LL=llvm/simple.ll
M2R=llvm/simple_mem2reg.ll

INS=outputs/simple/instrumented.ll
PROG=outputs/simple/program
RLOG=outputs/simple/runtime.log

$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names "$SRC" -o "$LL"
$OPT -S -passes=mem2reg "$LL" -o "$M2R"

( cd outputs/simple/static && ../../../bin/defuse-analyzer -graph ../../../"$M2R" ) \
  > logs/simple.static.log 2>&1 || (tail -n 120 logs/simple.static.log && exit 1)

bin/defuse-analyzer -instrument "$M2R" "$INS" > logs/simple.instrument.log 2>&1 \
  || (tail -n 120 logs/simple.instrument.log && exit 1)

$CC -O0 runtime/core_runtime.c "$INS" -o "$PROG" > logs/simple.buildprog.log 2>&1 \
  || (tail -n 120 logs/simple.buildprog.log && exit 1)

"$PROG" > "$RLOG" 2>/dev/null || true

( cd outputs/simple/runtime && ../../../bin/defuse-analyzer -graph ../../../"$M2R" ../runtime.log ) \
  > logs/simple.runtime.log 2>&1 || (tail -n 120 logs/simple.runtime.log && exit 1)

echo "[run_simple] ok -> outputs/simple/{static,runtime}"
