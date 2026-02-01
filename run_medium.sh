#!/usr/bin/env bash
set -euo pipefail

# запускать из корня проекта: ./run_medium.sh

mkdir -p llvm logs outputs/medium/static outputs/medium/runtime

[ -x bin/defuse-analyzer ] || { echo "ERROR: bin/defuse-analyzer not found. Run ./build.sh"; exit 1; }
command -v clang >/dev/null 2>&1 || { echo "ERROR: clang not found"; exit 1; }
command -v opt   >/dev/null 2>&1 || { echo "ERROR: opt not found"; exit 1; }

CC=${CC:-clang}
OPT=${OPT:-opt}

SRC=tests/medium/main.c
LL=llvm/medium.ll
M2R=llvm/medium_mem2reg.ll

INS=outputs/medium/instrumented.ll
PROG=outputs/medium/program
RLOG=outputs/medium/runtime.log

$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names "$SRC" -o "$LL"
$OPT -S -passes=mem2reg "$LL" -o "$M2R"

( cd outputs/medium/static && ../../../bin/defuse-analyzer -graph ../../../"$M2R" ) \
  > logs/medium.static.log 2>&1 || (tail -n 120 logs/medium.static.log && exit 1)

bin/defuse-analyzer -instrument "$M2R" "$INS" > logs/medium.instrument.log 2>&1 \
  || (tail -n 120 logs/medium.instrument.log && exit 1)

$CC -O0 runtime/core_runtime.c "$INS" -o "$PROG" > logs/medium.buildprog.log 2>&1 \
  || (tail -n 120 logs/medium.buildprog.log && exit 1)

"$PROG" > "$RLOG" 2>/dev/null || true

( cd outputs/medium/runtime && ../../../bin/defuse-analyzer -graph ../../../"$M2R" ../runtime.log ) \
  > logs/medium.runtime.log 2>&1 || (tail -n 120 logs/medium.runtime.log && exit 1)

echo "[run_medium] ok -> outputs/medium/{static,runtime}"
