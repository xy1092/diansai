#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import datetime as dt
import os
import queue
import signal
import threading
import time
import tkinter as tk
from collections import deque
from pathlib import Path
from tkinter import messagebox, ttk

os.environ.setdefault("MPLCONFIGDIR", str(Path(__file__).resolve().parent / ".mplconfig"))

import serial
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.figure import Figure


CSV_HEADER = [
    "kind", "ts_ms", "ch", "sp", "meas", "out", "err", "integ", "deriv",
    "p_term", "i_term", "d_term", "raw_out",
    "line_bias", "contrast", "strength", "on_line",
    "raw0", "raw1", "raw2", "raw3", "raw4", "raw5", "raw6",
]
PID_CHANNELS = ("L", "R", "LINE", "ANG")
RAW_SENSOR_COUNT = 7


def parse_pid_line(line: str):
    if not line.startswith("$P,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    try:
        return {
            "kind": "pid",
            "ts_ms": int(parts[1]),
            "ch": parts[2],
            "sp": float(parts[3]),
            "meas": float(parts[4]),
            "out": float(parts[5]),
            "err": float(parts[6]),
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


class SerialWorker:
    def __init__(self, rx_queue: queue.Queue[str]):
        self.rx_queue = rx_queue
        self.ser: serial.Serial | None = None
        self.stop_evt = threading.Event()
        self.thread: threading.Thread | None = None

    def connect(self, port: str, baud: int):
        self.stop_evt.clear()
        self.ser = serial.Serial(port, baud, timeout=0.2)
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def disconnect(self):
        self.stop_evt.set()
        if self.thread is not None:
            self.thread.join(timeout=0.5)
            self.thread = None
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None

    def send(self, cmd: str):
        if self.ser is None:
            raise RuntimeError("serial not connected")
        self.ser.write(cmd.encode("ascii"))
        self.ser.flush()

    def _reader(self):
        assert self.ser is not None
        buf = bytearray()
        while not self.stop_evt.is_set():
            try:
                chunk = self.ser.read(256)
            except Exception as exc:
                self.rx_queue.put(f"[serial-error] {exc}")
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
                        self.rx_queue.put_nowait(text)
                    except queue.Full:
                        pass


class DashboardApp:
    def __init__(self, root: tk.Tk, args):
        self.root = root
        self.args = args
        self.root.title("NUEDC Car Debug Dashboard")
        self.root.geometry("1480x940")

        self.rx_queue: queue.Queue[str] = queue.Queue(maxsize=8000)
        self.worker = SerialWorker(self.rx_queue)
        self.connected = False
        self.csv_file = None
        self.csv_writer = None

        self.time_window = tk.DoubleVar(value=args.window)
        self.port_var = tk.StringVar(value=args.port)
        self.baud_var = tk.IntVar(value=args.baud)
        self.rate_var = tk.IntVar(value=args.rate)
        self.rawline_var = tk.BooleanVar(value=args.line_raw)
        self.focus_channel_var = tk.StringVar(value="LINE")
        self.command_var = tk.StringVar(value="")
        self.status_var = tk.StringVar(value="Disconnected")
        self.log_path_var = tk.StringVar(value=self._default_log_path())
        self.latest_line_bias_var = tk.StringVar(value="bias: --")
        self.latest_line_strength_var = tk.StringVar(value="strength: --")
        self.latest_line_contrast_var = tk.StringVar(value="contrast: --")
        self.latest_line_on_var = tk.StringVar(value="on_line: --")

        self.channel_enabled = {
            ch: tk.BooleanVar(value=(ch in ("L", "R", "LINE")))
            for ch in PID_CHANNELS
        }
        self.channel_stats = {
            ch: {
                "sp": tk.StringVar(value="sp --"),
                "meas": tk.StringVar(value="meas --"),
                "out": tk.StringVar(value="out --"),
                "err": tk.StringVar(value="err --"),
            }
            for ch in PID_CHANNELS
        }
        self.series = {
            ch: {
                "t": deque(maxlen=3000),
                "sp": deque(maxlen=3000),
                "meas": deque(maxlen=3000),
                "out": deque(maxlen=3000),
                "p": deque(maxlen=3000),
                "i": deque(maxlen=3000),
                "d": deque(maxlen=3000),
            }
            for ch in PID_CHANNELS
        }
        self.latest_raw = [0] * RAW_SENSOR_COUNT
        self.t0_ms = None

        self._build_layout()
        self._setup_plot()
        self._update_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _default_log_path(self) -> str:
        here = Path(__file__).resolve().parent
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        return str(here / "logs" / f"dashboard_{ts}.csv")

    def _build_layout(self):
        self.root.columnconfigure(1, weight=1)
        self.root.rowconfigure(0, weight=1)

        left = ttk.Frame(self.root, padding=10)
        left.grid(row=0, column=0, sticky="nsw")
        left.columnconfigure(0, weight=1)

        right = ttk.Frame(self.root, padding=(0, 10, 10, 10))
        right.grid(row=0, column=1, sticky="nsew")
        right.columnconfigure(0, weight=1)
        right.rowconfigure(0, weight=1)

        conn = ttk.LabelFrame(left, text="Connection", padding=8)
        conn.grid(row=0, column=0, sticky="ew")
        conn.columnconfigure(1, weight=1)
        ttk.Label(conn, text="Port").grid(row=0, column=0, sticky="w")
        ttk.Entry(conn, textvariable=self.port_var, width=16).grid(row=0, column=1, sticky="ew")
        ttk.Label(conn, text="Baud").grid(row=1, column=0, sticky="w")
        ttk.Entry(conn, textvariable=self.baud_var, width=16).grid(row=1, column=1, sticky="ew")
        ttk.Label(conn, text="Rate Hz").grid(row=2, column=0, sticky="w")
        ttk.Entry(conn, textvariable=self.rate_var, width=16).grid(row=2, column=1, sticky="ew")
        ttk.Checkbutton(conn, text="Line raw", variable=self.rawline_var).grid(row=3, column=0, columnspan=2, sticky="w")
        ttk.Button(conn, text="Connect", command=self.connect).grid(row=4, column=0, sticky="ew", pady=(6, 0))
        ttk.Button(conn, text="Disconnect", command=self.disconnect).grid(row=4, column=1, sticky="ew", pady=(6, 0))
        ttk.Label(conn, textvariable=self.status_var, foreground="#0b5").grid(row=5, column=0, columnspan=2, sticky="w", pady=(6, 0))

        controls = ttk.LabelFrame(left, text="Controls", padding=8)
        controls.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        controls.columnconfigure(0, weight=1)
        controls.columnconfigure(1, weight=1)
        ttk.Button(controls, text="Resume", command=lambda: self.send_command("$RESUME\r\n")).grid(row=0, column=0, sticky="ew", padx=(0, 4))
        ttk.Button(controls, text="Pause", command=lambda: self.send_command("$PAUSE\r\n")).grid(row=0, column=1, sticky="ew")
        ttk.Button(controls, text="Reset PID", command=lambda: self.send_command("$RST\r\n")).grid(row=1, column=0, sticky="ew", pady=(6, 0), padx=(0, 4))
        ttk.Button(controls, text="Dump Gains", command=lambda: self.send_command("$DUMP\r\n")).grid(row=1, column=1, sticky="ew", pady=(6, 0))
        ttk.Button(controls, text="Apply Rate", command=self.apply_rate).grid(row=2, column=0, sticky="ew", pady=(6, 0), padx=(0, 4))
        ttk.Button(controls, text="Apply RawLine", command=self.apply_rawline).grid(row=2, column=1, sticky="ew", pady=(6, 0))
        ttk.Button(controls, text="Clear Curves", command=self.clear_curves).grid(row=3, column=0, sticky="ew", pady=(6, 0), padx=(0, 4))
        ttk.Button(controls, text="New Log File", command=self.rotate_log_file).grid(row=3, column=1, sticky="ew", pady=(6, 0))

        custom = ttk.LabelFrame(left, text="Custom Command", padding=8)
        custom.grid(row=2, column=0, sticky="ew", pady=(10, 0))
        custom.columnconfigure(0, weight=1)
        ttk.Entry(custom, textvariable=self.command_var).grid(row=0, column=0, sticky="ew")
        ttk.Button(custom, text="Send", command=self.send_custom_command).grid(row=0, column=1, padx=(6, 0))

        view = ttk.LabelFrame(left, text="View", padding=8)
        view.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        view.columnconfigure(1, weight=1)
        ttk.Label(view, text="Window s").grid(row=0, column=0, sticky="w")
        ttk.Entry(view, textvariable=self.time_window, width=10).grid(row=0, column=1, sticky="ew")
        ttk.Label(view, text="Terms").grid(row=1, column=0, sticky="w", pady=(6, 0))
        ttk.Combobox(view, textvariable=self.focus_channel_var, values=list(PID_CHANNELS), state="readonly", width=8).grid(row=1, column=1, sticky="ew", pady=(6, 0))
        for idx, ch in enumerate(PID_CHANNELS, start=2):
            ttk.Checkbutton(view, text=f"Show {ch}", variable=self.channel_enabled[ch]).grid(row=idx, column=0, columnspan=2, sticky="w")

        stats = ttk.LabelFrame(left, text="Latest Values", padding=8)
        stats.grid(row=4, column=0, sticky="ew", pady=(10, 0))
        for row, ch in enumerate(PID_CHANNELS):
            ttk.Label(stats, text=ch, font=("", 10, "bold")).grid(row=row * 2, column=0, sticky="w", pady=(4 if row else 0, 0))
            ttk.Label(stats, textvariable=self.channel_stats[ch]["sp"]).grid(row=row * 2, column=1, sticky="w")
            ttk.Label(stats, textvariable=self.channel_stats[ch]["meas"]).grid(row=row * 2, column=2, sticky="w")
            ttk.Label(stats, textvariable=self.channel_stats[ch]["out"]).grid(row=row * 2 + 1, column=1, sticky="w")
            ttk.Label(stats, textvariable=self.channel_stats[ch]["err"]).grid(row=row * 2 + 1, column=2, sticky="w")

        line_stats = ttk.LabelFrame(left, text="Line Sensors", padding=8)
        line_stats.grid(row=5, column=0, sticky="ew", pady=(10, 0))
        ttk.Label(line_stats, textvariable=self.latest_line_bias_var).grid(row=0, column=0, sticky="w")
        ttk.Label(line_stats, textvariable=self.latest_line_strength_var).grid(row=1, column=0, sticky="w")
        ttk.Label(line_stats, textvariable=self.latest_line_contrast_var).grid(row=2, column=0, sticky="w")
        ttk.Label(line_stats, textvariable=self.latest_line_on_var).grid(row=3, column=0, sticky="w")

        log_frame = ttk.LabelFrame(left, text="Log File", padding=8)
        log_frame.grid(row=6, column=0, sticky="ew", pady=(10, 0))
        log_frame.columnconfigure(0, weight=1)
        ttk.Entry(log_frame, textvariable=self.log_path_var).grid(row=0, column=0, sticky="ew")

        console_frame = ttk.LabelFrame(left, text="Messages", padding=8)
        console_frame.grid(row=7, column=0, sticky="nsew", pady=(10, 0))
        left.rowconfigure(7, weight=1)
        self.console = tk.Text(console_frame, width=42, height=16, wrap="word")
        self.console.grid(row=0, column=0, sticky="nsew")
        console_scroll = ttk.Scrollbar(console_frame, orient="vertical", command=self.console.yview)
        console_scroll.grid(row=0, column=1, sticky="ns")
        self.console.configure(yscrollcommand=console_scroll.set)

        plot_frame = ttk.Frame(right)
        plot_frame.grid(row=0, column=0, sticky="nsew")
        plot_frame.columnconfigure(0, weight=1)
        plot_frame.rowconfigure(0, weight=1)
        self.plot_frame = plot_frame

    def _setup_plot(self):
        self.figure = Figure(figsize=(12.0, 8.2), dpi=100)
        self.ax_track = self.figure.add_subplot(221)
        self.ax_out = self.figure.add_subplot(222)
        self.ax_terms = self.figure.add_subplot(223)
        self.ax_raw = self.figure.add_subplot(224)
        self.figure.tight_layout(pad=2.0)

        self.canvas = FigureCanvasTkAgg(self.figure, master=self.plot_frame)
        self.canvas.get_tk_widget().grid(row=0, column=0, sticky="nsew")
        toolbar = NavigationToolbar2Tk(self.canvas, self.plot_frame, pack_toolbar=False)
        toolbar.update()
        toolbar.grid(row=1, column=0, sticky="ew")

    def log(self, text: str):
        timestamp = dt.datetime.now().strftime("%H:%M:%S")
        self.console.insert("end", f"[{timestamp}] {text}\n")
        self.console.see("end")

    def connect(self):
        if self.connected:
            return
        try:
            self.worker.connect(self.port_var.get().strip(), int(self.baud_var.get()))
        except Exception as exc:
            messagebox.showerror("Connect failed", str(exc))
            return
        self.connected = True
        self.status_var.set("Connected")
        self.rotate_log_file(reset_path=False)
        self.log(f"Connected to {self.port_var.get()} @ {self.baud_var.get()}")
        time.sleep(0.05)
        self.apply_rate()
        self.apply_rawline()
        self.send_command("$RESUME\r\n")
        self.send_command("$DUMP\r\n")

    def disconnect(self):
        if not self.connected:
            return
        self.worker.disconnect()
        self.connected = False
        self.status_var.set("Disconnected")
        self._close_log_file()
        self.log("Disconnected")

    def send_command(self, cmd: str):
        if not self.connected:
            self.log(f"Not connected, skipped: {cmd.strip()}")
            return
        try:
            self.worker.send(cmd)
        except Exception as exc:
            self.log(f"Send failed: {exc}")
            self.disconnect()
            return
        self.log(f">> {cmd.strip()}")

    def apply_rate(self):
        self.send_command(f"$RATE,{int(self.rate_var.get())}\r\n")

    def apply_rawline(self):
        self.send_command(f"$RAWLINE,{1 if self.rawline_var.get() else 0}\r\n")

    def send_custom_command(self):
        text = self.command_var.get().strip()
        if not text:
            return
        if not text.endswith("\n"):
            text += "\r\n"
        self.send_command(text)
        self.command_var.set("")

    def rotate_log_file(self, reset_path: bool = True):
        self._close_log_file()
        if reset_path:
            self.log_path_var.set(self._default_log_path())
        path = self.log_path_var.get().strip()
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        self.csv_file = open(path, "w", newline="")
        self.csv_writer = csv.writer(self.csv_file)
        self.csv_writer.writerow(CSV_HEADER)
        self.log(f"Logging to {path}")

    def _close_log_file(self):
        if self.csv_file is not None:
            try:
                self.csv_file.close()
            except Exception:
                pass
            self.csv_file = None
            self.csv_writer = None

    def clear_curves(self):
        for ch in PID_CHANNELS:
            for key in self.series[ch]:
                self.series[ch][key].clear()
        self.latest_raw = [0] * RAW_SENSOR_COUNT
        self.t0_ms = None
        self.log("Cleared buffered curves")

    def _write_csv(self, row):
        if self.csv_writer is None:
            return
        self.csv_writer.writerow(row)
        if self.csv_file is not None:
            self.csv_file.flush()

    def _consume_queue(self):
        updated = False
        while True:
            try:
                line = self.rx_queue.get_nowait()
            except queue.Empty:
                break
            pid_rec = parse_pid_line(line)
            if pid_rec is not None:
                self._handle_pid(pid_rec)
                self._write_csv(csv_row_from_pid(pid_rec))
                updated = True
                continue
            line_rec = parse_line_sensor(line)
            if line_rec is not None:
                self._handle_line(line_rec)
                self._write_csv(csv_row_from_line(line_rec))
                updated = True
                continue
            self.log(line)
        return updated

    def _handle_pid(self, rec: dict):
        ch = rec["ch"]
        if ch not in self.series:
            return
        if self.t0_ms is None:
            self.t0_ms = rec["ts_ms"]
        t_sec = (rec["ts_ms"] - self.t0_ms) / 1000.0
        self.series[ch]["t"].append(t_sec)
        self.series[ch]["sp"].append(rec["sp"])
        self.series[ch]["meas"].append(rec["meas"])
        self.series[ch]["out"].append(rec["out"])
        self.series[ch]["p"].append(rec["p_term"])
        self.series[ch]["i"].append(rec["i_term"])
        self.series[ch]["d"].append(rec["d_term"])
        self.channel_stats[ch]["sp"].set(f"sp {rec['sp']:.2f}")
        self.channel_stats[ch]["meas"].set(f"meas {rec['meas']:.2f}")
        self.channel_stats[ch]["out"].set(f"out {rec['out']:.2f}")
        self.channel_stats[ch]["err"].set(f"err {rec['err']:.2f}")

    def _handle_line(self, rec: dict):
        self.latest_raw = list(rec["raw"])
        self.latest_line_bias_var.set(f"bias: {rec['line_bias']:.3f}")
        self.latest_line_strength_var.set(f"strength: {rec['strength']}")
        self.latest_line_contrast_var.set(f"contrast: {rec['contrast']}")
        self.latest_line_on_var.set(f"on_line: {rec['on_line']}")

    def _update_plots(self):
        window = max(1.0, float(self.time_window.get()))

        self.ax_track.cla()
        self.ax_out.cla()
        self.ax_terms.cla()
        self.ax_raw.cla()

        self.ax_track.set_title("Setpoint / Measure")
        self.ax_track.grid(True, alpha=0.25)
        self.ax_out.set_title("Output")
        self.ax_out.grid(True, alpha=0.25)
        self.ax_terms.set_title(f"P / I / D Terms ({self.focus_channel_var.get()})")
        self.ax_terms.grid(True, alpha=0.25)
        self.ax_raw.set_title("7-way Line Sensors")
        self.ax_raw.grid(True, axis="y", alpha=0.25)

        latest_t = 0.0
        for ch in PID_CHANNELS:
            if not self.channel_enabled[ch].get():
                continue
            if not self.series[ch]["t"]:
                continue
            latest_t = max(latest_t, self.series[ch]["t"][-1])
            t_vals = list(self.series[ch]["t"])
            self.ax_track.plot(t_vals, list(self.series[ch]["sp"]), label=f"{ch} sp", linewidth=1.5)
            self.ax_track.plot(t_vals, list(self.series[ch]["meas"]), label=f"{ch} meas", linewidth=1.2)
            self.ax_out.plot(t_vals, list(self.series[ch]["out"]), label=ch, linewidth=1.3)

        focus = self.focus_channel_var.get()
        if focus in self.series and self.series[focus]["t"]:
            t_vals = list(self.series[focus]["t"])
            self.ax_terms.plot(t_vals, list(self.series[focus]["p"]), label="P", linewidth=1.4)
            self.ax_terms.plot(t_vals, list(self.series[focus]["i"]), label="I", linewidth=1.4)
            self.ax_terms.plot(t_vals, list(self.series[focus]["d"]), label="D", linewidth=1.4)

        t_min = max(0.0, latest_t - window)
        for ax in (self.ax_track, self.ax_out, self.ax_terms):
            ax.set_xlim(t_min, max(t_min + window, latest_t + 0.1))

        if any(self.channel_enabled[ch].get() and self.series[ch]["t"] for ch in PID_CHANNELS):
            self.ax_track.legend(loc="upper right", fontsize=8, ncol=2)
            self.ax_out.legend(loc="upper right", fontsize=8)
        if focus in self.series and self.series[focus]["t"]:
            self.ax_terms.legend(loc="upper right", fontsize=8)

        x = list(range(RAW_SENSOR_COUNT))
        colors = ["#1f77b4"] * RAW_SENSOR_COUNT
        if any(self.latest_raw):
            colors[self.latest_raw.index(max(self.latest_raw))] = "#d62728"
        self.ax_raw.bar(x, self.latest_raw, color=colors, width=0.72)
        self.ax_raw.set_xticks(x)
        self.ax_raw.set_xticklabels([f"S{i}" for i in x])
        self.ax_raw.set_ylabel("ADC Raw")

        self.canvas.draw_idle()

    def _update_ui(self):
        try:
            if self._consume_queue():
                self._update_plots()
        finally:
            self.root.after(100, self._update_ui)

    def on_close(self):
        self.disconnect()
        self.root.destroy()


def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--rate", type=int, default=100)
    ap.add_argument("--window", type=float, default=8.0)
    ap.add_argument("--line-raw", action="store_true")
    ap.add_argument("--autoconnect", action="store_true")
    return ap.parse_args()


def main():
    args = parse_args()
    root = tk.Tk()
    app = DashboardApp(root, args)
    if args.autoconnect:
        root.after(150, app.connect)
    signal.signal(signal.SIGINT, lambda *_: app.on_close())
    root.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
