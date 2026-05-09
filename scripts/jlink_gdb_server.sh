#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

if [[ -z "${JLINK_GDB_SERVER:-}" || ! -x "$JLINK_GDB_SERVER" ]]; then
  echo '未找到 JLinkGDBServerCLExe。'
  echo '请先安装 SEGGER J-Link Software Pack。'
  exit 2
fi

exec "$JLINK_GDB_SERVER" "$@"
