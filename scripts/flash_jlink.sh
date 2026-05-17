#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

image_file="${1:-$BIN_FILE}"

if [[ -z "$JLINK_EXE" || ! -x "$JLINK_EXE" ]]; then
  echo '未找到 JLinkExe。'
  echo '请先安装 SEGGER J-Link Software Pack。'
  exit 2
fi

if [[ ! -f "$image_file" ]]; then
  echo "未找到待烧录二进制: $image_file"
  echo '请先完成 Build，或确认 build/nuedc_car.bin 已生成。'
  exit 2
fi

mkdir -p "$BUILD_DIR/tmp"
cmd_file="$(mktemp "$BUILD_DIR/tmp/jlink_cmd.XXXXXX")"
trap 'rm -f "$cmd_file"' EXIT

cat >"$cmd_file" <<EOF
r
h
loadfile $image_file, 0x00000000
r
g
q
EOF

exec "$JLINK_EXE" \
  -device "$JLINK_DEVICE" \
  -if SWD \
  -speed "$JLINK_SPEED_KHZ" \
  -autoconnect 1 \
  -NoGui 1 \
  -CommandFile "$cmd_file"
