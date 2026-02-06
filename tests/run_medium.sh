#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if command -v git >/dev/null 2>&1 && git -C "$SCRIPT_DIR" rev-parse --show-toplevel >/dev/null 2>&1; then
  ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"
else
  ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
fi

mkdir -p "$ROOT/llvm" "$ROOT/logs" "$ROOT/outputs/medium/static" "$ROOT/outputs/medium/runtime"

CC=${CC:-clang}
OPT=${OPT:-opt}

ANALYZER="$ROOT/bin/defuse-analyzer"

[[ -x "$ANALYZER" ]] || { echo "ERROR: $ANALYZER not found. Run $ROOT/build.sh"; exit 1; }
command -v "$CC"  >/dev/null 2>&1 || { echo "ERROR: clang not found"; exit 1; }
command -v "$OPT" >/dev/null 2>&1 || { echo "ERROR: opt not found"; exit 1; }

SRC="$ROOT/tests/medium/main.c"
LL="$ROOT/llvm/medium.ll"
M2R="$ROOT/llvm/medium_mem2reg.ll"

INS="$ROOT/outputs/medium/instrumented.ll"
PROG="$ROOT/outputs/medium/program"
RLOG="$ROOT/outputs/medium/runtime.log"

"$CC" -S -emit-llvm -O0 -Xclang -disable-O0-optnone -fno-discard-value-names "$SRC" -o "$LL"
"$OPT" -S -passes=mem2reg "$LL" -o "$M2R"

( cd "$ROOT/outputs/medium/static" && "$ANALYZER" -graph "$M2R" ) \
  > "$ROOT/logs/medium.static.log" 2>&1 || (tail -n 120 "$ROOT/logs/medium.static.log" && exit 1)

"$ANALYZER" -instrument "$M2R" "$INS" > "$ROOT/logs/medium.instrument.log" 2>&1 \
  || (tail -n 120 "$ROOT/logs/medium.instrument.log" && exit 1)

"$CC" -O0 "$ROOT/runtime/core_runtime.c" "$INS" -o "$PROG" > "$ROOT/logs/medium.buildprog.log" 2>&1 \
  || (tail -n 120 "$ROOT/logs/medium.buildprog.log" && exit 1)

"$PROG" > "$RLOG" 2>/dev/null || true

( cd "$ROOT/outputs/medium/runtime" && "$ANALYZER" -graph "$M2R" "$RLOG" ) \
  > "$ROOT/logs/medium.runtime.log" 2>&1 || (tail -n 120 "$ROOT/logs/medium.runtime.log" && exit 1)

echo "[run_medium] ok -> $ROOT/outputs/medium/{static,runtime}"
