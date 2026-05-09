#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")/.." && pwd)"
VENV_DIR="$HERE/.venv-pid"
REQ_FILE="$HERE/tools/pid/requirements.txt"

if [[ ! -d "$VENV_DIR" ]]; then
    python3 -m venv "$VENV_DIR"
fi

"$VENV_DIR/bin/pip" install -r "$REQ_FILE"

echo "[setup_pid_env] ready: $VENV_DIR"
