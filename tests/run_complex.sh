#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v git >/dev/null 2>&1 && git -C "$SCRIPT_DIR" rev-parse --show-toplevel >/dev/null 2>&1; then
  ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
else
  ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
fi

CC=${CC:-clang}
OPT=${OPT:-opt}
LINK=${LINK:-llvm-link}

ANALYZER="$ROOT/bin/defuse-analyzer"
TESTDIR="$ROOT/tests/complex"

mkdir -p "$ROOT/llvm" "$ROOT/logs" "$ROOT/outputs/complex/static" "$ROOT/outputs/complex/runtime"

[[ -x "$ANALYZER" ]] || { echo "ERROR: $ANALYZER not found. Run $ROOT/build.sh"; exit 1; }
command -v "$CC"   >/dev/null 2>&1 || { echo "ERROR: clang not found"; exit 1; }
command -v "$OPT"  >/dev/null 2>&1 || { echo "ERROR: opt not found"; exit 1; }
command -v "$LINK" >/dev/null 2>&1 || { echo "ERROR: llvm-link not found"; exit 1; }

"$CC" -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names "$TESTDIR/pool.c" -o "$ROOT/llvm/complex_pool.ll"
"$CC" -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names "$TESTDIR/hash.c" -o "$ROOT/llvm/complex_hash.ll"
"$CC" -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names "$TESTDIR/url.c"  -o "$ROOT/llvm/complex_url.ll"
"$CC" -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names "$TESTDIR/main.c" -o "$ROOT/llvm/complex_main.ll"

"$LINK" -S \
  "$ROOT/llvm/complex_pool.ll" \
  "$ROOT/llvm/complex_hash.ll" \
  "$ROOT/llvm/complex_url.ll" \
  "$ROOT/llvm/complex_main.ll" \
  -o "$ROOT/llvm/complex_linked.ll"

"$OPT" -S -passes=mem2reg "$ROOT/llvm/complex_linked.ll" -o "$ROOT/llvm/complex_mem2reg.ll"

M2R="$ROOT/llvm/complex_mem2reg.ll"
INS="$ROOT/outputs/complex/instrumented.ll"
PROG="$ROOT/outputs/complex/program"
RLOG="$ROOT/outputs/complex/runtime.log"

( cd "$ROOT/outputs/complex/static" && "$ANALYZER" -graph "$M2R" ) \
  > "$ROOT/logs/complex.static.log" 2>&1 || (tail -n 120 "$ROOT/logs/complex.static.log" && exit 1)

"$ANALYZER" -instrument "$M2R" "$INS" > "$ROOT/logs/complex.instrument.log" 2>&1 \
  || (tail -n 120 "$ROOT/logs/complex.instrument.log" && exit 1)

"$CC" -O0 "$ROOT/runtime/core_runtime.c" "$INS" -o "$PROG" > "$ROOT/logs/complex.buildprog.log" 2>&1 \
  || (tail -n 120 "$ROOT/logs/complex.buildprog.log" && exit 1)

"$PROG" > "$RLOG" 2>/dev/null || true

( cd "$ROOT/outputs/complex/runtime" && "$ANALYZER" -graph "$M2R" "$RLOG" ) \
  > "$ROOT/logs/complex.runtime.log" 2>&1 || (tail -n 120 "$ROOT/logs/complex.runtime.log" && exit 1)

echo "[run_complex] ok -> $ROOT/outputs/complex/{static,runtime}"
