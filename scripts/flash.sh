#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

if [[ ! -f "$ELF_FILE" ]]; then
  echo "未找到待烧录固件: $ELF_FILE"
  echo '请先完成 Build。'
  exit 2
fi

case "$FLASH_RUNNER" in
  loadti)
    if [[ ! -f "$CCXML_FILE" ]]; then
      echo "未找到 ccxml: $CCXML_FILE"
      exit 2
    fi
    exec "$LOADTI_SH" -c "$CCXML_FILE" "$ELF_FILE"
    ;;
  openocd)
    exec "$SCRIPT_DIR/flash_openocd.sh" "$OPENOCD_INTERFACE_CFG" "$OPENOCD_TARGET_CFG" "$ELF_FILE"
    ;;
  jlink)
    exec "$SCRIPT_DIR/flash_jlink.sh" "$BIN_FILE"
    ;;
  *)
    echo "不支持的 FLASH_RUNNER: $FLASH_RUNNER"
    echo '可选值: loadti | openocd | jlink'
    exit 2
    ;;
esac
