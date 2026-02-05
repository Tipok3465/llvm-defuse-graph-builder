#!/usr/bin/env bash
set -euo pipefail

# TODO[flops]: This script is meant to test tests/complex, so better move it to tests/complex

mkdir -p llvm logs outputs/complex/static outputs/complex/runtime

CC=${CC:-clang}
OPT=${OPT:-opt}
LINK=${LINK:-llvm-link}

[ -x bin/defuse-analyzer ] || { echo "ERROR: bin/defuse-analyzer not found. Run ./build.sh"; exit 1; }
command -v $CC     >/dev/null 2>&1 || { echo "ERROR: clang not found"; exit 1; }
command -v $OPT       >/dev/null 2>&1 || { echo "ERROR: opt not found"; exit 1; }
command -v $LINK >/dev/null 2>&1 || { echo "ERROR: llvm-link not found"; exit 1; }

$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names tests/complex/pool.c -o llvm/complex_pool.ll
$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names tests/complex/hash.c -o llvm/complex_hash.ll
$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names tests/complex/url.c  -o llvm/complex_url.ll
$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names tests/complex/main.c -o llvm/complex_main.ll

$LINK -S llvm/complex_pool.ll llvm/complex_hash.ll llvm/complex_url.ll llvm/complex_main.ll -o llvm/complex_linked.ll
$OPT -S -passes=mem2reg llvm/complex_linked.ll -o llvm/complex_mem2reg.ll

M2R=llvm/complex_mem2reg.ll
INS=outputs/complex/instrumented.ll
PROG=outputs/complex/program
RLOG=outputs/complex/runtime.log

( cd outputs/complex/static && ../../../bin/defuse-analyzer -graph ../../../"$M2R" ) \
  > logs/complex.static.log 2>&1 || (tail -n 120 logs/complex.static.log && exit 1)

bin/defuse-analyzer -instrument "$M2R" "$INS" > logs/complex.instrument.log 2>&1 \
  || (tail -n 120 logs/complex.instrument.log && exit 1)

$CC -O0 runtime/core_runtime.c "$INS" -o "$PROG" > logs/complex.buildprog.log 2>&1 \
  || (tail -n 120 logs/complex.buildprog.log && exit 1)

"$PROG" > "$RLOG" 2>/dev/null || true

( cd outputs/complex/runtime && ../../../bin/defuse-analyzer -graph ../../../"$M2R" ../runtime.log ) \
  > logs/complex.runtime.log 2>&1 || (tail -n 120 logs/complex.runtime.log && exit 1)

echo "[run_complex] ok -> outputs/complex/{static,runtime}"
