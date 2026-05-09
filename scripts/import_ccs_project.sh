#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/env.sh"

CCS_SERVER_CLI="${CCS_SERVER_CLI:-$CCS_DIR/eclipse/ccs-server-cli.sh}"
CCS_WORKSPACE_DIR="${CCS_WORKSPACE_DIR:-$HOME/ti/ccs_ws}"
CCS_PROJECT_NAME="${CCS_PROJECT_NAME:-nuedc_car_ccs}"
CCS_PROJECTSPEC="${CCS_PROJECTSPEC:-$PROJECT_DIR/ccs/nuedc_car_gcc.projectspec}"

if [[ ! -x "$CCS_SERVER_CLI" ]]; then
  echo "[error] CCS Server CLI not found: $CCS_SERVER_CLI" >&2
  exit 1
fi

if [[ ! -f "$CCS_PROJECTSPEC" ]]; then
  echo "[error] projectspec not found: $CCS_PROJECTSPEC" >&2
  exit 1
fi

mkdir -p "$CCS_WORKSPACE_DIR"

echo "[ccs] workspace: $CCS_WORKSPACE_DIR"
echo "[ccs] projectspec: $CCS_PROJECTSPEC"
echo "[ccs] project name: $CCS_PROJECT_NAME"

if [[ -z "${DISPLAY:-}" && -z "${WAYLAND_DISPLAY:-}" ]]; then
  echo "[warn] DISPLAY/WAYLAND_DISPLAY 未设置，CCS CLI 在部分 Linux 环境下可能无法启动。" >&2
fi

"$CCS_SERVER_CLI" \
  -workspace "$CCS_WORKSPACE_DIR" \
  -application importProject \
  -ccs.location "$CCS_PROJECTSPEC" \
  -ccs.device MSPM0G3507 \
  -ccs.renameTo "$CCS_PROJECT_NAME"

echo "[done] CCS project imported into: $CCS_WORKSPACE_DIR/$CCS_PROJECT_NAME"
