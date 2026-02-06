#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v git >/dev/null 2>&1 && git -C "$SCRIPT_DIR" rev-parse --show-toplevel >/dev/null 2>&1; then
  ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
else
  ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
fi

mkdir -p "$ROOT/llvm" "$ROOT/logs" "$ROOT/outputs/simple/static" "$ROOT/outputs/simple/runtime"

CC=${CC:-clang}
OPT=${OPT:-opt}

ANALYZER="$ROOT/bin/defuse-analyzer"

[[ -x "$ANALYZER" ]] || { echo "ERROR: $ANALYZER not found. Run $ROOT/build.sh"; exit 1; }
command -v "$CC"  >/dev/null 2>&1 || { echo "ERROR: clang not found"; exit 1; }
command -v "$OPT" >/dev/null 2>&1 || { echo "ERROR: opt not found"; exit 1; }

SRC="$ROOT/tests/simple/main.c"
LL="$ROOT/llvm/simple.ll"
M2R="$ROOT/llvm/simple_mem2reg.ll"

INS="$ROOT/outputs/simple/instrumented.ll"
PROG="$ROOT/outputs/simple/program"
RLOG="$ROOT/outputs/simple/runtime.log"

"$CC" -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names "$SRC" -o "$LL"
"$OPT" -S -passes=mem2reg "$LL" -o "$M2R"

( cd "$ROOT/outputs/simple/static" && "$ANALYZER" -graph "$M2R" ) \
  > "$ROOT/logs/simple.static.log" 2>&1 || (tail -n 120 "$ROOT/logs/simple.static.log" && exit 1)

"$ANALYZER" -instrument "$M2R" "$INS" > "$ROOT/logs/simple.instrument.log" 2>&1 \
  || (tail -n 120 "$ROOT/logs/simple.instrument.log" && exit 1)

"$CC" -O0 "$ROOT/runtime/core_runtime.c" "$INS" -o "$PROG" > "$ROOT/logs/simple.buildprog.log" 2>&1 \
  || (tail -n 120 "$ROOT/logs/simple.buildprog.log" && exit 1)

"$PROG" > "$RLOG" 2>/dev/null || true

( cd "$ROOT/outputs/simple/runtime" && "$ANALYZER" -graph "$M2R" "$RLOG" ) \
  > "$ROOT/logs/simple.runtime.log" 2>&1 || (tail -n 120 "$ROOT/logs/simple.runtime.log" && exit 1)

echo "[run_simple] ok -> $ROOT/outputs/simple/{static,runtime}"
