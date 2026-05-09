#!/usr/bin/env bash
# pid_session.sh — 一键打开 PID 调参会话
#
# 做的事：
#   1. 用 socat 把真实串口 /dev/ttyACM0 分叉成 /tmp/ttyV_mon 和 /tmp/ttyV_tune
#   2. 后台启动 pid_monitor.py 连 /tmp/ttyV_mon（带实时绘图）
#   3. 前台启动 pid_tune.py    连 /tmp/ttyV_tune
#   4. 退出时自动清理子进程
#
# 用法：
#   ./scripts/pid_session.sh                    # 默认 /dev/ttyACM0
#   ./scripts/pid_session.sh /dev/ttyACM1
#   ./scripts/pid_session.sh /dev/ttyACM0 921600
#
# 依赖：socat、python3、tools/pid/requirements.txt 已装好

set -euo pipefail

PORT="${1:-/dev/ttyACM0}"
BAUD="${2:-115200}"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
TOOLS="$HERE/tools/pid"
PYTHON_BIN="${PYTHON_BIN:-$HERE/.venv-pid/bin/python}"

if [[ ! -x "$PYTHON_BIN" ]]; then
    PYTHON_BIN="${PYTHON_FALLBACK:-python3}"
fi

V_BUS="/tmp/ttyV_bus"
V_MON="/tmp/ttyV_mon"
V_TUNE="/tmp/ttyV_tune"

PIDS=()

cleanup() {
    echo
    echo "[pid_session] cleaning up..."
    for p in "${PIDS[@]}"; do
        kill "$p" 2>/dev/null || true
    done
    rm -f "$V_BUS" "$V_MON" "$V_TUNE" 2>/dev/null || true
    exit 0
}
trap cleanup INT TERM EXIT

if ! command -v socat >/dev/null 2>&1; then
    echo "需要先安装 socat: sudo pacman -S socat" >&2
    exit 1
fi
if [[ ! -e "$PORT" ]]; then
    echo "串口设备不存在: $PORT" >&2
    exit 1
fi

echo "[pid_session] PORT=$PORT  BAUD=$BAUD"
echo "[pid_session] 启动 socat 分叉..."

# 真实串口 → /tmp/ttyV_bus（PTY 1）
socat -d -d -lf /tmp/socat_bus.log \
    "$PORT,raw,echo=0,b$BAUD" \
    "pty,raw,echo=0,link=$V_BUS" &
PIDS+=($!)
sleep 0.5

# /tmp/ttyV_bus → 同时给 mon 和 tune（双向汇流）
socat -d -d -lf /tmp/socat_fanout.log \
    "$V_BUS,raw,echo=0" \
    "pty,raw,echo=0,link=$V_MON" &
PIDS+=($!)

socat -d -d -lf /tmp/socat_tune.log \
    "$V_BUS,raw,echo=0" \
    "pty,raw,echo=0,link=$V_TUNE" &
PIDS+=($!)

sleep 1
[[ -e "$V_MON" ]]  || { echo "socat 分叉失败 (mon)";  exit 1; }
[[ -e "$V_TUNE" ]] || { echo "socat 分叉失败 (tune)"; exit 1; }

echo "[pid_session] 后台启动 monitor..."
"$PYTHON_BIN" "$TOOLS/pid_monitor.py" --port "$V_MON" --baud "$BAUD" \
    --channels L R LINE &
PIDS+=($!)
sleep 1

echo "[pid_session] 前台启动 tune REPL（Ctrl-D 退出会清理一切）"
"$PYTHON_BIN" "$TOOLS/pid_tune.py" --port "$V_TUNE" --baud "$BAUD"
