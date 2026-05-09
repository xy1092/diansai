#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

configure_only=0

if [[ "${1:-}" == "--configure-only" ]]; then
  configure_only=1
fi

"$SCRIPT_DIR/generate_syscfg.sh"
"$SCRIPT_DIR/cmake_configure.sh"

if [[ "$configure_only" -eq 1 ]]; then
  exit 0
fi

exec "$CMAKE_BIN" --build "$BUILD_DIR"
