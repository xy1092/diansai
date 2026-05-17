#!/usr/bin/env python3
"""
现场调试采集器：连接调试串口，手动开始/停止记录遥测数据。

按键：
  r  开始/停止记录
  1~4 选择 H1~H4
  s  发车
  x  停车
  l  开/关 7 路灰度原始流
  d  读取当前 PID 参数
  q  退出
"""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import os
import queue
import select
import signal
import sys
import termios
import threading
import time
import tty
from pathlib import Path

import serial

STOP = threading.Event()
RX_Q: queue.Queue[str] = queue.Queue(maxsize=10000)

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


def reader_thread(ser: serial.Serial) -> None:
    buf = bytearray()
    while not STOP.is_set():
        try:
            chunk = ser.read(256)
        except Exception as exc:
            print(f"\n[serial] read error: {exc}", file=sys.stderr)
            break
        if not chunk:
            continue
        buf.extend(chunk)
        while b"\n" in buf:
            line, _, rest = buf.partition(b"\n")
            buf = bytearray(rest)
            text = line.decode("ascii", errors="ignore").strip()
            if text:
                try:
                    RX_Q.put_nowait(text)
                except queue.Full:
                    pass


def parse_pid_line(line: str) -> list | None:
    if not line.startswith("$P,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    return [
        "pid", parts[1], parts[2],
        parts[3], parts[4], parts[5], parts[6], parts[7], parts[8],
        parts[9], parts[10], parts[11], parts[12],
        "", "", "", "", "", "", "", "", "", "", "",
        "", "", "", "", "", "", "", "", "",
        "", "", "", "",
        line,
    ]


def parse_line_sensor(line: str) -> list | None:
    if not line.startswith("$L,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    return [
        "line", parts[1], "LINE_RAW",
        "", "", "", "", "", "", "", "", "", "",
        parts[2], parts[3], parts[4], parts[5],
        parts[6], parts[7], parts[8], parts[9], parts[10], parts[11], parts[12],
        "", "", "", "", "", "", "", "", "",
        "", "", "", "",
        line,
    ]


def parse_app_state(line: str) -> list | None:
    if not line.startswith("$A,"):
        return None
    parts = line.split(",")
    if len(parts) < 11:
        return None
    return [
        "app", parts[1], "APP",
        "", "", "", "", "", "", "", "", "", "",
        "", "", "", "",
        "", "", "", "", "", "", "",
        parts[2], parts[3], parts[4], parts[5], parts[6], parts[7], parts[8], parts[9], parts[10],
        "", "", "", "",
        line,
    ]


def parse_config_line(line: str) -> list | None:
    if not line.startswith("$C,"):
        return None
    parts = line.split(",")
    if len(parts) < 5:
        return None
    return [
        "cfg", "", "CFG",
        "", "", "", "", "", "", "", "", "", "",
        "", "", "", "",
        "", "", "", "", "", "", "",
        "", "", "", "", "", "", "", "", "",
        parts[1], parts[2], parts[3], parts[4],
        line,
    ]


def csv_row(run_id: str, parsed: list) -> list:
    return [f"{time.time():.3f}", run_id, *parsed]


def send(ser: serial.Serial, text: str) -> None:
    ser.write(text.encode("ascii"))
    ser.flush()


def print_help() -> None:
    print()
    print("keys: r record  1-4 mission  s start  x stop  l rawline  d dump  q quit")
    print()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--rate", type=int, default=100)
    ap.add_argument("--out-dir", default=str(Path(__file__).resolve().parent / "logs"))
    ap.add_argument("--line-raw", action="store_true", help="启动时打开 7 路灰度原始流")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = Path(args.out_dir) / f"capture_{ts}.csv"
    raw_path = Path(args.out_dir) / f"capture_{ts}.raw.log"

    try:
        ser = serial.serial_for_url(args.port, args.baud, timeout=0.1)
    except Exception as exc:
        print(f"[serial] open failed: {exc}", file=sys.stderr)
        return 1

    STOP.clear()
    signal.signal(signal.SIGINT, lambda *_: STOP.set())
    t = threading.Thread(target=reader_thread, args=(ser,), daemon=True)
    t.start()

    send(ser, f"$RATE,{args.rate}\r\n")
    send(ser, f"$RAWLINE,{1 if args.line_raw else 0}\r\n")
    send(ser, "$RESUME\r\n")
    send(ser, "$DUMP\r\n")

    recording = False
    line_raw = args.line_raw
    frames = 0
    run_id = ""
    last_status = 0.0

    old_term = termios.tcgetattr(sys.stdin)
    tty.setcbreak(sys.stdin.fileno())

    with open(csv_path, "w", newline="") as csv_f, open(raw_path, "w") as raw_f:
        writer = csv.writer(csv_f)
        writer.writerow(CSV_HEADER)

        print(f"[capture] port={args.port} baud={args.baud} rate={args.rate}Hz")
        print(f"[capture] csv={csv_path}")
        print(f"[capture] raw={raw_path}")
        print_help()

        try:
            while not STOP.is_set():
                if select.select([sys.stdin], [], [], 0.02)[0]:
                    key = sys.stdin.read(1)
                    if key == "q":
                        STOP.set()
                    elif key == "r":
                        recording = not recording
                        if recording:
                            run_id = dt.datetime.now().strftime("%H%M%S")
                            frames = 0
                            print(f"\n[record] START run={run_id}")
                        else:
                            print(f"\n[record] STOP frames={frames}")
                    elif key in "1234":
                        send(ser, key)
                        print(f"\n[cmd] mission H{key}")
                    elif key in "sS":
                        send(ser, "s")
                        print("\n[cmd] start")
                    elif key in "xX":
                        send(ser, "x")
                        print("\n[cmd] stop")
                    elif key == "l":
                        line_raw = not line_raw
                        send(ser, f"$RAWLINE,{1 if line_raw else 0}\r\n")
                        print(f"\n[cmd] rawline={int(line_raw)}")
                    elif key == "d":
                        send(ser, "$DUMP\r\n")
                        print("\n[cmd] dump")
                    elif key in ("h", "?"):
                        print_help()

                pulled = 0
                while True:
                    try:
                        line = RX_Q.get_nowait()
                    except queue.Empty:
                        break
                    pulled += 1
                    raw_f.write(line + "\n")

                    parsed = (parse_pid_line(line) or parse_line_sensor(line) or
                              parse_app_state(line) or parse_config_line(line))
                    if recording and parsed:
                        writer.writerow(csv_row(run_id, parsed))
                        frames += 1
                    elif line.startswith("$OK") or line.startswith("$ERR") or line.startswith("$G") or line.startswith("$C"):
                        print(f"\n{line}")

                if pulled:
                    csv_f.flush()
                    raw_f.flush()

                now = time.time()
                if now - last_status >= 1.0:
                    last_status = now
                    state = "REC" if recording else "idle"
                    print(f"\r[{state}] frames={frames} rawline={int(line_raw)} queue={RX_Q.qsize()}   ", end="")
                    sys.stdout.flush()
        finally:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_term)
            STOP.set()
            ser.close()

    print(f"\n[capture] saved: {csv_path}")
    print(f"[capture] raw log: {raw_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
