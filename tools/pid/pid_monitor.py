#!/usr/bin/env python3
"""
实时采集 MCU 端 PID 遥测数据，落盘 CSV + 实时绘图。

串口格式约定见 middleware/telemetry.h：
    $P,<ts_ms>,<ch>,<sp>,<meas>,<out>,<err>,<integ>,<deriv>,<p>,<i>,<d>,<raw_out>
    $L,<ts_ms>,<bias>,<contrast>,<strength>,<on_line>,<r0>,...,<r6>

用法：
    python3 pid_monitor.py --port /dev/ttyACM0
    python3 pid_monitor.py --port /dev/ttyACM0 --csv logs/run_20260423.csv
    python3 pid_monitor.py --port /dev/ttyACM0 --channels L R LINE --window 10
    python3 pid_monitor.py --port /dev/ttyACM0 --line-raw
"""
from __future__ import annotations
import argparse
import csv
import datetime as dt
import os
import queue
import signal
import sys
import threading
import time
from collections import deque
from pathlib import Path

os.environ.setdefault("MPLCONFIGDIR", str(Path(__file__).resolve().parent / ".mplconfig"))

import serial

# ---------- 串口读取线程 ----------

STOP = threading.Event()
RX_Q: queue.Queue = queue.Queue(maxsize=5000)


def reader_thread(ser: serial.Serial):
    buf = bytearray()
    while not STOP.is_set():
        try:
            chunk = ser.read(256)
        except Exception as e:
            print(f"[serial] read error: {e}", file=sys.stderr)
            break
        if not chunk:
            continue
        buf.extend(chunk)
        while b"\n" in buf:
            line, _, rest = buf.partition(b"\n")
            buf = bytearray(rest)
            s = line.decode("ascii", errors="ignore").strip()
            if s:
                try:
                    RX_Q.put_nowait(s)
                except queue.Full:
                    pass


def parse_pid_line(line: str):
    """返回 dict 或 None；只处理 $P 数据行。"""
    if not line.startswith("$P,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    try:
        return {
            "kind":  "pid",
            "ts_ms": int(parts[1]),
            "ch":    parts[2],
            "sp":    float(parts[3]),
            "meas":  float(parts[4]),
            "out":   float(parts[5]),
            "err":   float(parts[6]),
            "integ": float(parts[7]),
            "deriv": float(parts[8]),
            "p_term": float(parts[9]),
            "i_term": float(parts[10]),
            "d_term": float(parts[11]),
            "raw_out": float(parts[12]),
        }
    except ValueError:
        return None


def parse_line_sensor(line: str):
    """返回 dict 或 None；只处理 $L 原始循迹数据。"""
    if not line.startswith("$L,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    try:
        return {
            "kind": "line",
            "ts_ms": int(parts[1]),
            "line_bias": float(parts[2]),
            "contrast": int(parts[3]),
            "strength": int(parts[4]),
            "on_line": int(parts[5]),
            "raw": [int(v) for v in parts[6:13]],
        }
    except ValueError:
        return None


def csv_row_from_pid(rec: dict):
    return [
        rec["kind"], rec["ts_ms"], rec["ch"], rec["sp"], rec["meas"], rec["out"],
        rec["err"], rec["integ"], rec["deriv"], rec["p_term"], rec["i_term"],
        rec["d_term"], rec["raw_out"], "", "", "", "", "", "", "", "", "", "", "",
    ]


def csv_row_from_line(rec: dict):
    return [
        rec["kind"], rec["ts_ms"], "LINE_RAW", "", "", "",
        "", "", "", "", "", "", "",
        rec["line_bias"], rec["contrast"], rec["strength"], rec["on_line"],
        rec["raw"][0], rec["raw"][1], rec["raw"][2], rec["raw"][3],
        rec["raw"][4], rec["raw"][5], rec["raw"][6],
    ]


# ---------- 主程序 ----------

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port",    default="/dev/ttyACM0")
    ap.add_argument("--baud",    type=int, default=115200)
    ap.add_argument("--csv",     default=None, help="落盘 CSV 路径；默认 logs/run_YYYYMMDD_HHMMSS.csv")
    ap.add_argument("--channels", nargs="+", default=["L", "R", "LINE"],
                    help="订阅的通道，空格分隔，如 L R LINE ANG")
    ap.add_argument("--window",  type=float, default=8.0, help="实时图显示窗口长度（秒）")
    ap.add_argument("--rate",    type=int, default=100, help="启动时配置 MCU 遥测速率（Hz）")
    ap.add_argument("--line-raw", action="store_true", help="同时采集 7 路灰度原始值")
    ap.add_argument("--no-init-cmds", action="store_true", help="不在启动时向 MCU 发送 RATE/RAWLINE/DUMP/RESUME")
    ap.add_argument("--no-plot", action="store_true", help="只落盘不绘图")
    args = ap.parse_args()

    here = Path(__file__).resolve().parent
    if args.csv is None:
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        args.csv = str(here / "logs" / f"run_{ts}.csv")
    os.makedirs(os.path.dirname(args.csv) or ".", exist_ok=True)

    print(f"[monitor] port={args.port}  baud={args.baud}")
    print(f"[monitor] csv ={args.csv}")
    print(f"[monitor] ch  ={args.channels}  window={args.window}s")
    print(f"[monitor] rate={args.rate}Hz  line_raw={args.line_raw}")

    STOP.clear()
    signal.signal(signal.SIGINT, lambda *_: STOP.set())
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as e:
        print(f"[serial] open failed: {e}", file=sys.stderr)
        return 1

    t = threading.Thread(target=reader_thread, args=(ser,), daemon=True)
    t.start()

    f = open(args.csv, "w", newline="")
    w = csv.writer(f)
    w.writerow([
        "kind", "ts_ms", "ch", "sp", "meas", "out", "err", "integ", "deriv",
        "p_term", "i_term", "d_term", "raw_out",
        "line_bias", "contrast", "strength", "on_line",
        "raw0", "raw1", "raw2", "raw3", "raw4", "raw5", "raw6"
    ])

    def send_cmd(cmd: str):
        ser.write(cmd.encode("ascii"))
        ser.flush()

    if not args.no_init_cmds:
        time.sleep(0.05)
        send_cmd(f"$RATE,{args.rate}\r\n")
        send_cmd(f"$RAWLINE,{1 if args.line_raw else 0}\r\n")
        send_cmd("$RESUME\r\n")
        send_cmd("$DUMP\r\n")

    if args.no_plot:
        while not STOP.is_set():
            try:
                line = RX_Q.get(timeout=0.5)
            except queue.Empty:
                continue
            rec_pid = parse_pid_line(line)
            if rec_pid and rec_pid["ch"] in args.channels:
                w.writerow(csv_row_from_pid(rec_pid))
                continue
            rec_line = parse_line_sensor(line)
            if rec_line and args.line_raw:
                w.writerow(csv_row_from_line(rec_line))
                continue
            print(line)
        STOP.set()
        ser.close()
        f.close()
        return 0

    # 延迟导入 matplotlib，避免 --no-plot 模式也得装
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    # 每通道两子图：(sp & meas)、(out)
    n_ch = len(args.channels)
    fig, axes = plt.subplots(n_ch, 2, figsize=(11, 2.6 * n_ch), squeeze=False)
    fig.canvas.manager.set_window_title(f"PID Monitor — {args.port}")

    N = 2000
    data = {ch: {"t": deque(maxlen=N), "sp": deque(maxlen=N),
                 "meas": deque(maxlen=N), "out": deque(maxlen=N)}
            for ch in args.channels}

    lines = {}
    for i, ch in enumerate(args.channels):
        ax1, ax2 = axes[i, 0], axes[i, 1]
        ax1.set_title(f"{ch}: setpoint vs measure")
        ax1.grid(True, alpha=0.3)
        lines[(ch, "sp")],   = ax1.plot([], [], label="sp",   lw=1.4)
        lines[(ch, "meas")], = ax1.plot([], [], label="meas", lw=1.2)
        ax1.legend(loc="upper right", fontsize=8)

        ax2.set_title(f"{ch}: output")
        ax2.grid(True, alpha=0.3)
        lines[(ch, "out")], = ax2.plot([], [], lw=1.2, color="tab:orange")

    t0 = None

    def on_data():
        nonlocal t0
        pulled = 0
        while True:
            try:
                line = RX_Q.get_nowait()
            except queue.Empty:
                break
            pulled += 1
            rec = parse_pid_line(line)
            if rec:
                if rec["ch"] in data:
                    if t0 is None:
                        t0 = rec["ts_ms"]
                    t_sec = (rec["ts_ms"] - t0) / 1000.0
                    d = data[rec["ch"]]
                    d["t"].append(t_sec)
                    d["sp"].append(rec["sp"])
                    d["meas"].append(rec["meas"])
                    d["out"].append(rec["out"])
                w.writerow(csv_row_from_pid(rec))
                continue
            rec_line = parse_line_sensor(line)
            if rec_line:
                if args.line_raw:
                    w.writerow(csv_row_from_line(rec_line))
                continue
            print(line)    # 其他消息原样打印
        if pulled:
            f.flush()

    def animate(_):
        on_data()
        latest_t = 0.0
        for ch in args.channels:
            d = data[ch]
            if not d["t"]:
                continue
            latest_t = max(latest_t, d["t"][-1])
        tmin = max(0.0, latest_t - args.window)
        for i, ch in enumerate(args.channels):
            d = data[ch]
            ax1, ax2 = axes[i, 0], axes[i, 1]
            lines[(ch, "sp")].set_data(d["t"],   d["sp"])
            lines[(ch, "meas")].set_data(d["t"], d["meas"])
            lines[(ch, "out")].set_data(d["t"],  d["out"])
            ax1.set_xlim(tmin, max(tmin + args.window, latest_t + 0.1))
            ax2.set_xlim(tmin, max(tmin + args.window, latest_t + 0.1))
            ax1.relim(); ax1.autoscale_view(scalex=False)
            ax2.relim(); ax2.autoscale_view(scalex=False)
        return []

    ani = FuncAnimation(fig, animate, interval=80, blit=False, cache_frame_data=False)
    try:
        plt.tight_layout()
        plt.show()
    finally:
        STOP.set()
        ser.close()
        f.close()
        print(f"[monitor] saved → {args.csv}")


if __name__ == "__main__":
    raise SystemExit(main() or 0)
