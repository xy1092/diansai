#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

if [[ -z "${JLINK_EXE:-}" || ! -x "$JLINK_EXE" ]]; then
  echo "未找到 JLinkExe。"
  exit 2
fi

jlink_dir="$(cd "$(dirname "$JLINK_EXE")" && pwd)"
rtt_client="$jlink_dir/JLinkRTTClientExe"

if [[ ! -x "$rtt_client" ]]; then
  echo "未找到 JLinkRTTClientExe: $rtt_client"
  exit 2
fi

exec "$rtt_client"
