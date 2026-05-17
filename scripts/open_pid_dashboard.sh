#!/usr/bin/env bash
# 启动新的网页版 PID 控制面板（FastAPI + 浏览器）
#
# 用法：
#   ./scripts/open_pid_dashboard.sh                    # 默认 /dev/ttyACM0
#   ./scripts/open_pid_dashboard.sh /dev/ttyACM1
#   ./scripts/open_pid_dashboard.sh socket://192.168.1.20:3333   # ESP32 透传
#
# 启动后浏览器会自动打开 http://127.0.0.1:8765
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-$HERE/.venv-pid/bin/python}"
SERVER_PY="$HERE/tools/pid/pid_web_server.py"
LOG_DIR="$HERE/tools/pid/logs"
PORT="${1:-/dev/ttyACM0}"
BAUD="${2:-115200}"
RATE="${3:-100}"
HOST="${HOST:-127.0.0.1}"
WEB_PORT="${WEB_PORT:-8765}"

if [[ ! -x "$PYTHON_BIN" ]]; then
    echo "未找到 Python 环境: $PYTHON_BIN" >&2
    echo "请先运行任务 PID: Install Python deps（会装 fastapi/uvicorn）" >&2
    exit 1
fi

mkdir -p "$LOG_DIR"
STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="$LOG_DIR/dashboard_launch_${STAMP}.log"
PID_FILE="$LOG_DIR/dashboard_launch_${STAMP}.pid"

launch_cmd=(
    "$PYTHON_BIN" "$SERVER_PY"
    --host "$HOST"
    --web-port "$WEB_PORT"
    --port "$PORT"
    --baud "$BAUD"
    --rate "$RATE"
    --open
    --autoconnect
)

if command -v setsid >/dev/null 2>&1; then
    nohup setsid "${launch_cmd[@]}" </dev/null >"$LOG_FILE" 2>&1 &
else
    nohup "${launch_cmd[@]}" </dev/null >"$LOG_FILE" 2>&1 &
fi

echo $! > "$PID_FILE"

echo "[open_pid_dashboard] http://$HOST:$WEB_PORT/  PORT=$PORT  BAUD=$BAUD  pid=$(cat "$PID_FILE")"
echo "[open_pid_dashboard] log: $LOG_FILE"
