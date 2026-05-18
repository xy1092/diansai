#!/usr/bin/env bash
# Short launcher for the NUEDC PID web dashboard.
#
# Modes:
#   0 / usb       wired USB, local browser only
#   1 / esp32     ESP32 Wi-Fi bridge, local browser only
#   2 / phone     ESP32 Wi-Fi bridge, phone can open the panel
#
# Common usage:
#   ./scripts/start.sh 0                # wired USB: /dev/ttyACM0
#   ./scripts/start.sh 0 /dev/ttyACM1
#   ./scripts/start.sh 1                # ESP32 bridge: socket://192.168.4.1:3333
#   ./scripts/start.sh 2                # phone-friendly, listens on 0.0.0.0
#   ./scripts/start.sh socket://192.168.4.1:3333
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mode="${1:-2}"
port=""
host="${HOST:-}"

case "$mode" in
  0|usb|wired|wire)
    port="${2:-/dev/ttyACM0}"
    host="${host:-127.0.0.1}"
    ;;
  1|esp32|wifi)
    port="socket://192.168.4.1:3333"
    host="${host:-127.0.0.1}"
    ;;
  2|phone|mobile)
    port="socket://192.168.4.1:3333"
    host="${host:-0.0.0.0}"
    ;;
  127|local)
    port="${2:-socket://192.168.4.1:3333}"
    host="${host:-127.0.0.1}"
    ;;
  socket://*|/dev/*)
    port="$mode"
    host="${host:-127.0.0.1}"
    ;;
  *)
    echo "用法:"
    echo "  ./scripts/start.sh 0 [/dev/ttyACM0] # 有线 USB"
    echo "  ./scripts/start.sh 1                # ESP32 无线，电脑本机打开"
    echo "  ./scripts/start.sh 2                # 手机模式，手机可打开电脑面板"
    echo "  ./scripts/start.sh socket://192.168.4.1:3333"
    exit 2
    ;;
esac

case "$mode" in
  0|usb|wired|wire)
    echo "[diansai-web] mode 0: wired USB -> $port"
    ;;
  1|esp32|wifi|127|local)
    echo "[diansai-web] mode 1: ESP32 bridge -> $port"
    ;;
  2|phone|mobile)
    echo "[diansai-web] mode 2: phone mode -> $port"
    ;;
esac

HOST="$host" exec "$SCRIPT_DIR/open_pid_dashboard.sh" "$port"
