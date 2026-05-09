#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

status=0

check_file() {
  local path="$1"
  local label="$2"
  if [[ -e "$path" ]]; then
    printf '[OK]   %s: %s\n' "$label" "$path"
  else
    printf '[MISS] %s: %s\n' "$label" "$path"
    status=1
  fi
}

check_dir() {
  local path="$1"
  local label="$2"
  if [[ -d "$path" ]]; then
    printf '[OK]   %s: %s\n' "$label" "$path"
  else
    printf '[MISS] %s: %s\n' "$label" "$path"
    status=1
  fi
}

echo '[tools]'
check_dir  "$CCS_DIR" 'CCS'
check_file "$TI_ARM_CLANG_BIN/tiarmclang" 'TI ARM Clang'
check_file "$GMAKE_BIN" 'gmake'
check_file "$SYSCFG_CLI" 'SysConfig CLI'
check_file "$LOADTI_SH" 'loadti.sh'
check_file "$CCXML_FILE" 'ccxml'
check_file "$CMAKE_BIN" 'cmake'
check_file "$NINJA_BIN" 'ninja'
check_file "$ARM_GCC_BIN" 'arm-none-eabi-gcc'
check_file "$ARM_OBJCOPY_BIN" 'arm-none-eabi-objcopy'
check_file "$ARM_SIZE_BIN" 'arm-none-eabi-size'
check_file "$OPENOCD_BIN" 'openocd'
if [[ -n "${JLINK_EXE:-}" ]]; then
  check_file "$JLINK_EXE" 'JLinkExe'
else
  echo '[MISS] JLinkExe: 未安装 SEGGER J-Link Software Pack'
  status=1
fi
if [[ -n "${JLINK_GDB_SERVER:-}" ]]; then
  check_file "$JLINK_GDB_SERVER" 'JLinkGDBServerCLExe'
else
  echo '[MISS] JLinkGDBServerCLExe: 未安装 SEGGER J-Link Software Pack'
  status=1
fi
if [[ -n "${OPENOCD_BIN:-}" && -x "$OPENOCD_BIN" && -f "$OPENOCD_TARGET_CFG" ]]; then
  probe_output="$("$OPENOCD_BIN" -f interface/stlink.cfg -f "$OPENOCD_TARGET_CFG" -c shutdown 2>&1 || true)"
  if grep -Fq "flash driver 'mspm0' not found" <<<"$probe_output"; then
    echo '[MISS] OpenOCD MSPM0 flash driver: 当前 openocd 不支持 MSPM0 烧录'
    status=1
  else
    echo '[OK]   OpenOCD MSPM0 flash driver: 已检测到可用支持'
  fi
fi

echo
echo '[sdk]'
if [[ -n "${MSPM0_SDK_ROOT:-}" ]]; then
  check_dir "$MSPM0_SDK_ROOT" 'MSPM0 SDK'
  check_file "$MSPM0_SDK_ROOT/.metadata/product.json" 'MSPM0 product.json'
  check_file "$MSPM0_SDK_ROOT/source/ti/driverlib/dl_common.h" 'DriverLib headers'
else
  echo '[MISS] MSPM0 SDK: MSPM0_SDK_ROOT 未设置，且常见目录下未找到 SDK'
  echo '       官方下载页: https://www.ti.com/tool/MSPM0-SDK'
  status=1
fi

echo
echo '[project]'
check_file "$PROJECT_DIR/main.c" 'main.c'
check_file "$PROJECT_DIR/pin_map.h" 'pin_map.h'
check_file "$PROJECT_DIR/CMakeLists.txt" 'CMakeLists.txt'
check_file "$OPENOCD_TARGET_CFG" 'OpenOCD target cfg'
if [[ -e "$PROJECT_DIR/ti_msp_dl_config.c" || -e "$PROJECT_DIR/ti_msp_dl_config.h" ]]; then
  echo '[OK]   SysConfig generated files: 已存在'
else
  echo '[MISS] SysConfig generated files: 当前工程还没有 ti_msp_dl_config.[ch]'
  status=1
fi

echo
echo '[board]'
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo 'No ttyACM/ttyUSB device'
lsusb 2>/dev/null | grep -Ei 'texas instruments|xds110|mspm0|ti ' || echo 'No TI/XDS110 probe detected'
id

exit "$status"
