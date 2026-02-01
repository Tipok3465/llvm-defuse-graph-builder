#!/usr/bin/env bash
set -euo pipefail

# запускать из корня проекта: ./run_complex.sh

mkdir -p llvm logs outputs/complex/static outputs/complex/runtime

[ -x bin/defuse-analyzer ] || { echo "ERROR: bin/defuse-analyzer not found. Run ./build.sh"; exit 1; }
command -v clang     >/dev/null 2>&1 || { echo "ERROR: clang not found"; exit 1; }
command -v opt       >/dev/null 2>&1 || { echo "ERROR: opt not found"; exit 1; }
command -v llvm-link >/dev/null 2>&1 || { echo "ERROR: llvm-link not found"; exit 1; }

CC=${CC:-clang}
OPT=${OPT:-opt}
LINK=${LINK:-llvm-link}

# 1) per-module IR
$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names tests/complex/pool.c -o llvm/complex_pool.ll
$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names tests/complex/hash.c -o llvm/complex_hash.ll
$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names tests/complex/url.c  -o llvm/complex_url.ll
$CC -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names tests/complex/main.c -o llvm/complex_main.ll

# 2) link + mem2reg
$LINK -S llvm/complex_pool.ll llvm/complex_hash.ll llvm/complex_url.ll llvm/complex_main.ll -o llvm/complex_linked.ll
$OPT -S -passes=mem2reg llvm/complex_linked.ll -o llvm/complex_mem2reg.ll

M2R=llvm/complex_mem2reg.ll
INS=outputs/complex/instrumented.ll
PROG=outputs/complex/program
RLOG=outputs/complex/runtime.log

# 3) static graph
( cd outputs/complex/static && ../../../bin/defuse-analyzer -graph ../../../"$M2R" ) \
  > logs/complex.static.log 2>&1 || (tail -n 120 logs/complex.static.log && exit 1)

# 4) instrument + build + run
bin/defuse-analyzer -instrument "$M2R" "$INS" > logs/complex.instrument.log 2>&1 \
  || (tail -n 120 logs/complex.instrument.log && exit 1)

$CC -O0 runtime/core_runtime.c "$INS" -o "$PROG" > logs/complex.buildprog.log 2>&1 \
  || (tail -n 120 logs/complex.buildprog.log && exit 1)

"$PROG" > "$RLOG" 2>/dev/null || true

# 5) runtime graph
( cd outputs/complex/runtime && ../../../bin/defuse-analyzer -graph ../../../"$M2R" ../runtime.log ) \
  > logs/complex.runtime.log 2>&1 || (tail -n 120 logs/complex.runtime.log && exit 1)

echo "[run_complex] ok -> outputs/complex/{static,runtime}"
