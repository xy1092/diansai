#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export CCS_DIR="${CCS_DIR:-$HOME/ti/ccs2050/ccs}"
export TI_ARM_CLANG_DIR="${TI_ARM_CLANG_DIR:-$CCS_DIR/tools/compiler/ti-cgt-armllvm_4.0.4.LTS}"
export TI_ARM_CLANG_BIN="${TI_ARM_CLANG_BIN:-$TI_ARM_CLANG_DIR/bin}"
export SYSCFG_DIR="${SYSCFG_DIR:-$CCS_DIR/utils/sysconfig_1.27.0}"
export SYSCFG_CLI="${SYSCFG_CLI:-$SYSCFG_DIR/sysconfig_cli.sh}"
export GMAKE_BIN="${GMAKE_BIN:-$CCS_DIR/utils/bin/gmake}"
export LOADTI_SH="${LOADTI_SH:-$CCS_DIR/ccs_base/scripting/examples/loadti/loadti.sh}"
export CCXML_FILE="${CCXML_FILE:-$PROJECT_DIR/tools/mspm0g3507.ccxml}"
export CMAKE_BIN="${CMAKE_BIN:-$(command -v cmake || true)}"
export NINJA_BIN="${NINJA_BIN:-$(command -v ninja || true)}"
export ARM_GCC_BIN="${ARM_GCC_BIN:-$(command -v arm-none-eabi-gcc || true)}"
export ARM_OBJCOPY_BIN="${ARM_OBJCOPY_BIN:-$(command -v arm-none-eabi-objcopy || true)}"
export ARM_SIZE_BIN="${ARM_SIZE_BIN:-$(command -v arm-none-eabi-size || true)}"
export OPENOCD_BIN="${OPENOCD_BIN:-}"
export JLINK_EXE="${JLINK_EXE:-$(command -v JLinkExe || true)}"
export JLINK_GDB_SERVER="${JLINK_GDB_SERVER:-$(command -v JLinkGDBServerCLExe || true)}"

if [[ -z "${MSPM0_SDK_ROOT:-}" ]]; then
  for sdk_dir in \
    "/opt/ti/mspm0_sdk" \
    "$HOME/ti/mspm0_sdk_2_10_00_04/mspm0_sdk_2_10_00_04" \
    "$HOME/ti/mspm0_sdk_2_10_00_04" \
    "$HOME/ti/mspm0_sdk_2_07_00_05/mspm0_sdk_2_07_00_05" \
    "$HOME/ti/mspm0_sdk_2_07_00_05"
  do
    if [[ -f "$sdk_dir/.metadata/product.json" ]]; then
      export MSPM0_SDK_ROOT="$sdk_dir"
      break
    fi
  done
fi

if [[ -z "${JLINK_EXE:-}" ]]; then
  for jlink_dir in \
    "$HOME/Downloads/JLink_Linux_V938_x86_64" \
    "$HOME/下载/JLink_Linux_V938_x86_64" \
    "$HOME/JLink_Linux_V938_x86_64" \
    "/opt/SEGGER/JLink"
  do
    if [[ -x "$jlink_dir/JLinkExe" ]]; then
      export JLINK_EXE="$jlink_dir/JLinkExe"
      break
    fi
  done
fi

if [[ -z "${JLINK_GDB_SERVER:-}" && -n "${JLINK_EXE:-}" ]]; then
  jlink_dir="$(cd "$(dirname "$JLINK_EXE")" && pwd)"
  for gdb_server in \
    "$jlink_dir/JLinkGDBServerCLExe" \
    "$jlink_dir/JLinkGDBServerExe" \
    "$jlink_dir/JLinkGDBServer"
  do
    if [[ -x "$gdb_server" ]]; then
      export JLINK_GDB_SERVER="$gdb_server"
      break
    fi
  done
fi

if [[ -z "${OPENOCD_BIN:-}" ]]; then
  for openocd_bin in \
    "$HOME/.config/Texas Instruments/ti-embedded-debug/openocd/1.3.1.50/bin/openocd" \
    "$HOME/.config/Texas Instruments/ti-embedded-debug/openocd/1.3.1.49/bin/openocd" \
    "/usr/bin/openocd"
  do
    if [[ -x "$openocd_bin" ]]; then
      export OPENOCD_BIN="$openocd_bin"
      break
    fi
  done
fi

export BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build}"
export ELF_FILE="${ELF_FILE:-$BUILD_DIR/nuedc_car.out}"
export BIN_FILE="${BIN_FILE:-$BUILD_DIR/nuedc_car.bin}"
export HEX_FILE="${HEX_FILE:-$BUILD_DIR/nuedc_car.hex}"
export CMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE:-$PROJECT_DIR/cmake/toolchains/arm-none-eabi-gcc.cmake}"
export NUEDC_MINIMAL_DEBUG="${NUEDC_MINIMAL_DEBUG:-OFF}"
export NUEDC_NO_ENCODER="${NUEDC_NO_ENCODER:-OFF}"
export NUEDC_USE_ULTRASONIC="${NUEDC_USE_ULTRASONIC:-OFF}"
export NUEDC_USE_K230="${NUEDC_USE_K230:-OFF}"
export FLASH_RUNNER="${FLASH_RUNNER:-loadti}"
export OPENOCD_INTERFACE_CFG="${OPENOCD_INTERFACE_CFG:-interface/stlink.cfg}"
export OPENOCD_TARGET_CFG="${OPENOCD_TARGET_CFG:-$PROJECT_DIR/tools/openocd/mspm0g3507.cfg}"
export OPENOCD_SPEED_KHZ="${OPENOCD_SPEED_KHZ:-4000}"
export JLINK_DEVICE="${JLINK_DEVICE:-MSPM0G3507}"
export JLINK_SPEED_KHZ="${JLINK_SPEED_KHZ:-4000}"
