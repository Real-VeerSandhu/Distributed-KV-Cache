#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

# Extend this list when src/ and apps/ are added.
DIRS=("${ROOT_DIR}/tests")

find "${DIRS[@]}" \
  -type f \( -name "*.cc" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) \
  -print0 \
  | xargs -0 clang-format -i --style=file

echo "Formatting complete."
