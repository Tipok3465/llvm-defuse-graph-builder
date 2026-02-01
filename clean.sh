#!/usr/bin/env bash
set -euo pipefail

rm -rf obj bin llvm logs outputs
rm -rf runtime/*.o
mkdir -p obj bin llvm logs outputs

echo "[clean] done"
