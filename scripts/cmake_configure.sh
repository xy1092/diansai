#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

need_tool() {
  local path="$1"
  local label="$2"
  if [[ -z "$path" || ! -x "$path" ]]; then
    echo "缺少工具: $label"
    exit 2
  fi
}

need_tool "$CMAKE_BIN" 'cmake'
need_tool "$NINJA_BIN" 'ninja'
need_tool "$ARM_GCC_BIN" 'arm-none-eabi-gcc'
need_tool "$ARM_OBJCOPY_BIN" 'arm-none-eabi-objcopy'

if [[ -z "${MSPM0_SDK_ROOT:-}" ]]; then
  echo 'MSPM0_SDK_ROOT 未设置，且未自动发现 SDK。'
  echo '请先安装 MSPM0 SDK，然后重试。官方页: https://www.ti.com/tool/MSPM0-SDK'
  exit 2
fi

if [[ ! -f "$MSPM0_SDK_ROOT/.metadata/product.json" ]]; then
  echo "未找到 SDK product.json: $MSPM0_SDK_ROOT/.metadata/product.json"
  exit 2
fi

if [[ ! -f "$PROJECT_DIR/ti_msp_dl_config.c" || ! -f "$PROJECT_DIR/ti_msp_dl_config.h" ]]; then
  echo '当前工程缺少 ti_msp_dl_config.[ch]。'
  echo '请先在 CCS/SysConfig 中为 LP-MSPM0G3507 生成工程配置文件。'
  exit 2
fi

mkdir -p "$BUILD_DIR"

cmake_args=(
  -S "$PROJECT_DIR"
  -B "$BUILD_DIR"
  -G Ninja
  -DCMAKE_BUILD_TYPE=Debug
  -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE"
  -DMSPM0_SDK_ROOT="$MSPM0_SDK_ROOT"
  -DNUEDC_MINIMAL_DEBUG="$NUEDC_MINIMAL_DEBUG"
  -DNUEDC_NO_ENCODER="$NUEDC_NO_ENCODER"
)

if [[ -n "${MSPM0_STARTUP_FILE:-}" ]]; then
  cmake_args+=("-DMSPM0_STARTUP_FILE=$MSPM0_STARTUP_FILE")
fi

if [[ -n "${MSPM0_LINKER_SCRIPT:-}" ]]; then
  cmake_args+=("-DMSPM0_LINKER_SCRIPT=$MSPM0_LINKER_SCRIPT")
fi

exec "$CMAKE_BIN" "${cmake_args[@]}"
