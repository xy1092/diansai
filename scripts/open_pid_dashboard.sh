#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-$HERE/.venv-pid/bin/python}"
DASHBOARD_PY="$HERE/tools/pid/pid_dashboard.py"
LOG_DIR="$HERE/tools/pid/logs"
PORT="${1:-/dev/ttyACM0}"
BAUD="${2:-115200}"
RATE="${3:-100}"

if [[ ! -x "$PYTHON_BIN" ]]; then
    echo "未找到 Python 环境: $PYTHON_BIN" >&2
    echo "请先运行任务 PID: Install Python deps" >&2
    exit 1
fi

mkdir -p "$LOG_DIR"
STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="$LOG_DIR/dashboard_launch_${STAMP}.log"
PID_FILE="$LOG_DIR/dashboard_launch_${STAMP}.pid"

launch_cmd=(
    "$PYTHON_BIN" "$DASHBOARD_PY"
    --port "$PORT"
    --baud "$BAUD"
    --rate "$RATE"
    --line-raw
    --autoconnect
)

if command -v setsid >/dev/null 2>&1; then
    nohup setsid "${launch_cmd[@]}" </dev/null >"$LOG_FILE" 2>&1 &
else
    nohup "${launch_cmd[@]}" </dev/null >"$LOG_FILE" 2>&1 &
fi

echo $! > "$PID_FILE"

echo "[open_pid_dashboard] launched for $PORT @ $BAUD, pid: $(cat "$PID_FILE"), log: $LOG_FILE"
