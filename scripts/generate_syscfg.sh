#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

force=0
if [[ "${1:-}" == "--force" ]]; then
  force=1
fi

need_tool() {
  local path="$1"
  local label="$2"
  if [[ -z "$path" || ! -x "$path" ]]; then
    echo "缺少工具: $label"
    exit 2
  fi
}

need_tool "$SYSCFG_CLI" 'SysConfig CLI'

if [[ -z "${MSPM0_SDK_ROOT:-}" ]]; then
  echo 'MSPM0_SDK_ROOT 未设置，且未自动发现 SDK。'
  exit 2
fi

product_json="$MSPM0_SDK_ROOT/.metadata/product.json"
if [[ ! -f "$product_json" ]]; then
  echo "未找到 SDK product.json: $product_json"
  exit 2
fi

syscfg_script="$PROJECT_DIR/nuedc_car.syscfg"
if [[ ! -f "$syscfg_script" ]]; then
  echo "未找到 SysConfig 脚本: $syscfg_script"
  exit 2
fi

generated_files=(
  "$PROJECT_DIR/ti_msp_dl_config.c"
  "$PROJECT_DIR/ti_msp_dl_config.h"
  "$PROJECT_DIR/device.opt"
  "$PROJECT_DIR/device.cmd.genlibs"
  "$PROJECT_DIR/device.lds.genlibs"
  "$PROJECT_DIR/device_linker.cmd"
  "$PROJECT_DIR/device_linker.lds"
)

regen="$force"
if [[ "$regen" -eq 0 ]]; then
  for generated in "${generated_files[@]}"; do
    if [[ ! -f "$generated" || "$syscfg_script" -nt "$generated" ]]; then
      regen=1
      break
    fi
  done
fi

if [[ "$regen" -eq 0 ]]; then
  echo '[syscfg] generated files are up to date'
  exit 0
fi

echo "[syscfg] generating from $syscfg_script"
exec "$SYSCFG_CLI" \
  --product "$product_json" \
  --compiler gcc \
  --script "$syscfg_script" \
  --output "$PROJECT_DIR"
