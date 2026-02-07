#!/usr/bin/env bash
set -euo pipefail

# Resolve repo root (scripts/..)
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "🔍 Running clang-format..."

find vr180 util -type f \( \
  -name "*.cpp" -o \
  -name "*.h"   -o \
  -name "*.cu"  \
\) -print0 | xargs -0 clang-format -i

echo "✅ clang-format complete"

# ---------------------------------------
# Optional clang-tidy
# Enable with: LINT_TIDY=1 ./scripts/lint.sh
# ---------------------------------------
if [[ "${LINT_TIDY:-0}" == "1" ]]; then
  echo "🔍 Running clang-tidy (best-effort)..."

  if [[ ! -f compile_commands.json ]]; then
    echo "⚠️ compile_commands.json not found — clang-tidy may be approximate"
  fi

  find vr180 util -type f -name "*.cpp" | while read -r file; do
    echo "  tidy $file"
    clang-tidy "$file" -p . || true
  done

  echo "✅ clang-tidy pass complete"
fi

echo "🔥 Finished linting 🚀"