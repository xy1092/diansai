#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

interface_cfg="${1:-$OPENOCD_INTERFACE_CFG}"
target_cfg="${2:-$OPENOCD_TARGET_CFG}"
image_file="${3:-$ELF_FILE}"

if [[ -z "$OPENOCD_BIN" || ! -x "$OPENOCD_BIN" ]]; then
  echo '未找到 openocd。'
  exit 2
fi

if [[ ! -f "$target_cfg" ]]; then
  echo "未找到 OpenOCD target cfg: $target_cfg"
  exit 2
fi

if [[ ! -f "$image_file" ]]; then
  echo "未找到待烧录镜像: $image_file"
  echo '请先完成 Build。'
  exit 2
fi

probe_output="$("$OPENOCD_BIN" -f "$interface_cfg" -f "$target_cfg" -c shutdown 2>&1 || true)"
if grep -Fq "flash driver 'mspm0' not found" <<<"$probe_output"; then
  echo '当前 openocd 不包含 MSPM0 flash driver，无法给 MSPM0G3507 烧录。'
  echo '请安装带 MSPM0 支持的 OpenOCD，或改用 JLinkExe / TI XDS110(loadti) 路线。'
  exit 2
fi

exec "$OPENOCD_BIN" \
  -f "$interface_cfg" \
  -f "$target_cfg" \
  -c "adapter speed $OPENOCD_SPEED_KHZ" \
  -c "transport select swd" \
  -c "program \"$image_file\" verify reset exit"
