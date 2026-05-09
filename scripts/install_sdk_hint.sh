#!/usr/bin/env bash
set -euo pipefail

cat <<'EOF'
当前工程需要完整的 MSPM0 SDK 才能编译并生成 ti_msp_dl_config.[ch]。

官方页面：
  https://www.ti.com/tool/MSPM0-SDK

截至 2026-04-22，TI 官方页面显示 Linux 最新版为：
  MSPM0 SDK 2.10.00.04
  文件名: mspm0_sdk_2_10_00_04.run

建议安装到：
  /home/xy/ti/mspm0_sdk_2_10_00_04

安装完成后，确认以下文件存在：
  /home/xy/ti/mspm0_sdk_2_10_00_04/.metadata/product.json
  /home/xy/ti/mspm0_sdk_2_10_00_04/source/ti/driverlib/dl_common.h

然后重新运行：
  ./scripts/check_env.sh
EOF
