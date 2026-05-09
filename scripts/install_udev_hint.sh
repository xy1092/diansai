#!/usr/bin/env bash
set -euo pipefail

cat <<'EOF'
为让 XDS110 / MSPM0 LaunchPad 在 Linux 下免 root 访问，建议安装 TI udev 规则：

  /home/xy/ti/ccs2050/ccs/install_scripts/ti_permissions_install.sh --install

安装后执行：
  sudo udevadm control --reload-rules
  sudo udevadm trigger

然后重新插拔板子，再运行：
  ./scripts/probe_board.sh
EOF
