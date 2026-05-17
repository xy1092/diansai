#!/usr/bin/env python3
"""
RTT 数据采集器：不走 UART，只通过 J-Link RTT 从 SWD 读取 telemetry。

用法：
  .venv-pid/bin/python tools/pid/capture_rtt.py
  .venv-pid/bin/python tools/pid/capture_rtt.py --duration 20

注意：
  - 需要先把包含 RTT 输出的固件烧进板子，并让程序运行。
  - RTT Logger 通常会独占 J-Link，运行前请停止 VS Code 调试会话。
  - 这个脚本只收数据；启动/选模式请先用按键/拨码，或在调试器里让程序继续运行。
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

CSV_HEADER = [
    "host_ts", "run_id", "kind", "ts_ms", "ch",
    "sp", "meas", "out", "err", "integ", "deriv",
    "p_term", "i_term", "d_term", "raw_out",
    "line_bias", "contrast", "strength", "on_line",
    "raw0", "raw1", "raw2", "raw3", "raw4", "raw5", "raw6",
    "mission", "state", "loop", "seg", "x", "y", "theta_deg", "seg_cm", "mission_time_ms",
    "param", "param_value", "param_min", "param_max",
    "raw_line",
]


def pad_row(kind: str, ts_ms: str, ch: str, raw_line: str) -> list[str]:
    row = [""] * (len(CSV_HEADER) - 3)
    row[0] = kind
    row[1] = ts_ms
    row[2] = ch
    row[-1] = raw_line
    return row


def parse_pid_line(line: str) -> list[str] | None:
    if not line.startswith("$P,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        parts += [""] * (13 - len(parts))
    row = pad_row("pid", parts[1], parts[2], line)
    row[3:13] = parts[3:13]
    return row


def parse_line_sensor(line: str) -> list[str] | None:
    if not line.startswith("$L,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    row = pad_row("line", parts[1], "LINE_RAW", line)
    row[13:17] = parts[2:6]
    row[17:24] = parts[6:13]
    return row


def parse_app_state(line: str) -> list[str] | None:
    if not line.startswith("$A,"):
        return None
    parts = line.split(",")
    if len(parts) < 11:
        return None
    row = pad_row("app", parts[1], "APP", line)
    row[24:33] = parts[2:11]
    return row


def parse_config_line(line: str) -> list[str] | None:
    if not line.startswith("$C,"):
        return None
    parts = line.split(",")
    if len(parts) < 5:
        return None
    row = pad_row("cfg", "", "CFG", line)
    row[33:37] = parts[1:5]
    return row


def parse_line(line: str) -> list[str] | None:
    return parse_pid_line(line) or parse_line_sensor(line) or parse_app_state(line) or parse_config_line(line)


def run_logger(args: argparse.Namespace, raw_path: Path) -> subprocess.Popen:
    answers = (
        f"{args.device}\n"
        f"{args.interface}\n"
        f"{args.speed}\n"
        "\n"
        f"{args.channel}\n"
        f"{raw_path}\n"
    )
    proc = subprocess.Popen(
        [args.rtt_logger],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert proc.stdin is not None
    proc.stdin.write(answers)
    proc.stdin.flush()
    return proc


def main() -> int:
    project = Path(__file__).resolve().parents[2]
    default_logger = "/home/xy/下载/JLink_Linux_V938_x86_64/JLinkRTTLoggerExe"

    ap = argparse.ArgumentParser()
    ap.add_argument("--rtt-logger", default=default_logger)
    ap.add_argument("--device", default="MSPM0G3507")
    ap.add_argument("--interface", default="SWD")
    ap.add_argument("--speed", default="4000")
    ap.add_argument("--channel", default="0")
    ap.add_argument("--duration", type=float, default=0.0, help="采集秒数，0=直到 Ctrl-C")
    ap.add_argument("--out-dir", default=str(project / "tools" / "pid" / "logs"))
    args = ap.parse_args()

    if not os.path.exists(args.rtt_logger):
        print(f"[rtt] logger not found: {args.rtt_logger}", file=sys.stderr)
        return 1

    os.makedirs(args.out_dir, exist_ok=True)
    ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    raw_path = Path(args.out_dir) / f"rtt_{ts}.raw.log"
    csv_path = Path(args.out_dir) / f"rtt_{ts}.csv"
    run_id = ts

    stop = False

    def on_sigint(*_args):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, on_sigint)

    print(f"[rtt] raw={raw_path}")
    print(f"[rtt] csv={csv_path}")
    print("[rtt] starting JLinkRTTLoggerExe...")
    proc = run_logger(args, raw_path)

    # Give the logger time to create the file and find the RTT block.
    start = time.time()
    last_pos = 0
    pending = ""
    frames = 0
    last_status = 0.0

    with open(csv_path, "w", newline="") as csv_f:
        writer = csv.writer(csv_f)
        writer.writerow(CSV_HEADER)

        try:
            while not stop:
                if args.duration > 0 and time.time() - start >= args.duration:
                    break

                if proc.poll() is not None:
                    print(f"\n[rtt] logger exited with code {proc.returncode}")
                    break

                if raw_path.exists():
                    with open(raw_path, "rb") as raw_f:
                        raw_f.seek(last_pos)
                        data = raw_f.read()
                        last_pos = raw_f.tell()
                    if data:
                        text = data.replace(b"\x00", b"").decode("ascii", errors="ignore")
                        pending += text
                        while "\n" in pending:
                            line, pending = pending.split("\n", 1)
                            line = line.strip()
                            if not line:
                                continue
                            parsed = parse_line(line)
                            if parsed:
                                writer.writerow([f"{time.time():.3f}", run_id, *parsed])
                                frames += 1
                            elif line.startswith("$OK") or line.startswith("$ERR") or line.startswith("$G"):
                                print(f"\n{line}")
                        csv_f.flush()

                now = time.time()
                if now - last_status >= 1.0:
                    last_status = now
                    print(f"\r[rtt] frames={frames} bytes={last_pos}   ", end="")
                    sys.stdout.flush()
                time.sleep(0.05)
        finally:
            try:
                proc.terminate()
                proc.wait(timeout=2)
            except Exception:
                proc.kill()

    print(f"\n[rtt] saved: {csv_path}")
    print(f"[rtt] raw log: {raw_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
