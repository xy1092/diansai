#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
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
    "mission", "state", "loop", "seg", "x", "y", "theta_deg", "seg_cm", "mission_time_ms",
    "param", "param_value", "param_min", "param_max", "raw_line",
]
BLACKBOX_HEADER = [
    "idx", "mission_time_ms", "mission", "state", "loop", "seg",
    "x", "y", "theta_deg", "seg_cm",
    "sp_l", "meas_l", "out_l", "sp_r", "meas_r", "out_r",
    "line_bias", "contrast", "strength", "on_line",
]
PID_CHANNELS = ("L", "R", "LINE", "ANG")
RAW_SENSOR_COUNT = 7

DEFAULT_PID_VALUES = {
    ("L", "KP"): "580", ("L", "KI"): "8", ("L", "KD"): "35",
    ("R", "KP"): "580", ("R", "KI"): "8", ("R", "KD"): "35",
    ("LINE", "KP"): "82", ("LINE", "KI"): "0", ("LINE", "KD"): "18",
}
DEFAULT_CFG_VALUES = {
    "spd_min": "0.18", "spd_max": "0.65", "arc_spd": "0.42",
    "seek_spd": "0.24", "seek_diff": "0.18",
    "pose_kd": "0.018", "pose_kh": "0.42", "pose_kf": "0.22",
    "arc_min": "95", "arc_tol": "10", "imu_hold": "0.20",
}
CFG_GROUPS = [
    ("Speed Targets", (
        ("spd_min", "Minimum pulse/ms"),
        ("spd_max", "Straight max pulse/ms"),
        ("arc_spd", "Arc cruise pulse/ms"),
        ("seek_spd", "Lost-line seek pulse/ms"),
    )),
    ("Line Recovery", (
        ("seek_diff", "Lost-line turn diff"),
        ("arc_min", "Arc min distance cm"),
        ("arc_tol", "Arc done tolerance cm"),
    )),
    ("Pose / Heading", (
        ("pose_kd", "Distance gain"),
        ("pose_kh", "Heading gain"),
        ("pose_kf", "Final heading gain"),
        ("imu_hold", "IMU hold gain"),
    )),
]
STATE_NAME = {0: "IDLE", 1: "READY", 2: "RUN", 3: "STOP", 4: "ERROR"}


def parse_pid_line(line: str):
    if not line.startswith("$P,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    try:
        return {
            "kind": "pid", "ts_ms": int(parts[1]), "ch": parts[2],
            "sp": float(parts[3]), "meas": float(parts[4]), "out": float(parts[5]),
            "err": float(parts[6]), "integ": float(parts[7]), "deriv": float(parts[8]),
            "p_term": float(parts[9]), "i_term": float(parts[10]),
            "d_term": float(parts[11]), "raw_out": float(parts[12]), "raw_line": line,
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
            "kind": "line", "ts_ms": int(parts[1]), "line_bias": float(parts[2]),
            "contrast": int(parts[3]), "strength": int(parts[4]), "on_line": int(parts[5]),
            "raw": [int(v) for v in parts[6:13]], "raw_line": line,
        }
    except ValueError:
        return None


def parse_app_state(line: str):
    if not line.startswith("$A,"):
        return None
    parts = line.split(",")
    if len(parts) < 11:
        return None
    try:
        return {
            "kind": "app", "ts_ms": int(parts[1]), "mission": int(parts[2]),
            "state": int(parts[3]), "loop": int(parts[4]), "seg": int(parts[5]),
            "x": float(parts[6]), "y": float(parts[7]), "theta_deg": float(parts[8]),
            "seg_cm": float(parts[9]), "mission_time_ms": int(parts[10]), "raw_line": line,
        }
    except ValueError:
        return None


def parse_gain(line: str):
    if not line.startswith("$G,"):
        return None
    parts = line.split(",")
    if len(parts) < 5:
        return None
    try:
        return {"ch": parts[1], "kp": float(parts[2]), "ki": float(parts[3]), "kd": float(parts[4])}
    except ValueError:
        return None


def parse_config(line: str):
    if not line.startswith("$C,"):
        return None
    parts = line.split(",")
    if len(parts) < 5:
        return None
    try:
        return {
            "kind": "cfg", "param": parts[1], "param_value": float(parts[2]),
            "param_min": float(parts[3]), "param_max": float(parts[4]), "raw_line": line,
        }
    except ValueError:
        return None


def parse_blackbox_line(line: str):
    if not line.startswith("$B,"):
        return None
    parts = line.split(",")
    if len(parts) < 21:
        return None
    try:
        values = [
            int(parts[1]), int(parts[2]), int(parts[3]), int(parts[4]), int(parts[5]), int(parts[6]),
            float(parts[7]), float(parts[8]), float(parts[9]), float(parts[10]),
            float(parts[11]), float(parts[12]), int(parts[13]), float(parts[14]), float(parts[15]), int(parts[16]),
            float(parts[17]), int(parts[18]), int(parts[19]), int(parts[20]),
        ]
        return dict(zip(BLACKBOX_HEADER, values))
    except ValueError:
        return None


def empty_row(kind: str, ts_ms="", ch="", raw_line=""):
    return {key: "" for key in CSV_HEADER} | {"kind": kind, "ts_ms": ts_ms, "ch": ch, "raw_line": raw_line}


def csv_row_from_pid(rec: dict):
    row = empty_row("pid", rec["ts_ms"], rec["ch"], rec["raw_line"])
    for key in ("sp", "meas", "out", "err", "integ", "deriv", "p_term", "i_term", "d_term", "raw_out"):
        row[key] = rec[key]
    return [row[k] for k in CSV_HEADER]


def csv_row_from_line(rec: dict):
    row = empty_row("line", rec["ts_ms"], "LINE_RAW", rec["raw_line"])
    row.update({"line_bias": rec["line_bias"], "contrast": rec["contrast"], "strength": rec["strength"], "on_line": rec["on_line"]})
    for idx, val in enumerate(rec["raw"]):
        row[f"raw{idx}"] = val
    return [row[k] for k in CSV_HEADER]


def csv_row_from_app(rec: dict):
    row = empty_row("app", rec["ts_ms"], "APP", rec["raw_line"])
    for key in ("mission", "state", "loop", "seg", "x", "y", "theta_deg", "seg_cm", "mission_time_ms"):
        row[key] = rec[key]
    return [row[k] for k in CSV_HEADER]


def csv_row_from_cfg(rec: dict):
    row = empty_row("cfg", "", "CFG", rec["raw_line"])
    for key in ("param", "param_value", "param_min", "param_max"):
        row[key] = rec[key]
    return [row[k] for k in CSV_HEADER]


def blackbox_csv_row(rec: dict):
    return [rec[k] for k in BLACKBOX_HEADER]


class SerialWorker:
    def __init__(self, rx_queue: queue.Queue[str]):
        self.rx_queue = rx_queue
        self.ser: serial.Serial | None = None
        self.stop_evt = threading.Event()
        self.thread: threading.Thread | None = None

    def connect(self, port: str, baud: int):
        self.stop_evt.clear()
        self.ser = serial.serial_for_url(port, baud, timeout=0.2)
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
        self.root.title("NUEDC Car Control Lab")
        self.root.geometry("1620x980")
        self.root.minsize(1260, 760)
        self._setup_style()

        self.rx_queue: queue.Queue[str] = queue.Queue(maxsize=10000)
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
        self.latest_line_bias_var = tk.StringVar(value="bias --")
        self.latest_line_strength_var = tk.StringVar(value="strength --")
        self.latest_line_contrast_var = tk.StringVar(value="contrast --")
        self.latest_line_on_var = tk.StringVar(value="on_line --")
        self.latest_app_var = tk.StringVar(value="No app frame yet")
        self.blackbox_status_var = tk.StringVar(value="No dump loaded")
        self.mission_card_var = tk.StringVar(value="H-  READY")
        self.pose_card_var = tk.StringVar(value="x --  y --  head --")
        self.time_card_var = tk.StringVar(value="0.0 s")

        self.pid_vars = {key: tk.StringVar(value=value) for key, value in DEFAULT_PID_VALUES.items()}
        self.cfg_vars = {key: tk.StringVar(value=value) for key, value in DEFAULT_CFG_VALUES.items()}
        self.channel_enabled = {ch: tk.BooleanVar(value=(ch in ("L", "R", "LINE"))) for ch in PID_CHANNELS}
        self.channel_stats = {
            ch: {"sp": tk.StringVar(value="sp --"), "meas": tk.StringVar(value="meas --"),
                 "out": tk.StringVar(value="out --"), "err": tk.StringVar(value="err --")}
            for ch in PID_CHANNELS
        }
        self.series = {
            ch: {key: deque(maxlen=3000) for key in ("t", "sp", "meas", "out", "p", "i", "d")}
            for ch in PID_CHANNELS
        }
        self.latest_raw = [0] * RAW_SENSOR_COUNT
        self.t0_ms = None
        self.latest_app = None
        self.latest_params: dict[str, dict] = {}
        self.blackbox_records: list[dict] = []
        self.blackbox_active = False

        self._build_layout()
        self._setup_plot()
        self._update_ui()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _setup_style(self):
        style = ttk.Style(self.root)
        try:
            style.theme_use("clam")
        except tk.TclError:
            pass
        self.root.configure(bg="#edf1f5")
        style.configure("TFrame", background="#edf1f5")
        style.configure("Panel.TFrame", background="#ffffff")
        style.configure("TLabel", background="#edf1f5", foreground="#1f2937", font=("Inter", 10))
        style.configure("Panel.TLabel", background="#ffffff", foreground="#1f2937", font=("Inter", 10))
        style.configure("Muted.TLabel", background="#ffffff", foreground="#64748b", font=("Inter", 9))
        style.configure("Title.TLabel", background="#edf1f5", foreground="#0f172a", font=("Inter", 18, "bold"))
        style.configure("CardTitle.TLabel", background="#ffffff", foreground="#334155", font=("Inter", 9))
        style.configure("CardValue.TLabel", background="#ffffff", foreground="#0f172a", font=("Inter", 15, "bold"))
        style.configure("TButton", padding=(10, 6), font=("Inter", 10))
        style.configure("Accent.TButton", padding=(12, 7), font=("Inter", 10, "bold"))
        style.configure("TEntry", padding=(5, 4))
        style.configure("TLabelframe", background="#ffffff", bordercolor="#d8dee9", relief="solid")
        style.configure("TLabelframe.Label", background="#ffffff", foreground="#334155", font=("Inter", 10, "bold"))
        style.configure("TNotebook", background="#edf1f5", borderwidth=0)
        style.configure("TNotebook.Tab", padding=(12, 7), font=("Inter", 10))

    def _default_log_path(self) -> str:
        here = Path(__file__).resolve().parent
        ts = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        return str(here / "logs" / f"dashboard_{ts}.csv")

    def _build_layout(self):
        self.root.columnconfigure(0, weight=0)
        self.root.columnconfigure(1, weight=1)
        self.root.rowconfigure(1, weight=1)

        header = ttk.Frame(self.root, padding=(18, 14, 18, 8))
        header.grid(row=0, column=0, columnspan=2, sticky="ew")
        header.columnconfigure(0, weight=1)
        ttk.Label(header, text="NUEDC Car Control Lab", style="Title.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(header, text="PID tuning, route telemetry, line sensors, and blackbox export", foreground="#64748b").grid(row=1, column=0, sticky="w")
        ttk.Label(header, textvariable=self.status_var, foreground="#0f766e", font=("Inter", 11, "bold")).grid(row=0, column=1, sticky="e")

        left = ttk.Frame(self.root, padding=(14, 0, 8, 14))
        left.grid(row=1, column=0, sticky="nsw")
        left.columnconfigure(0, weight=1)

        right = ttk.Frame(self.root, padding=(8, 0, 14, 14))
        right.grid(row=1, column=1, sticky="nsew")
        right.columnconfigure(0, weight=1)
        right.rowconfigure(2, weight=1)

        self._build_top_cards(right)
        self._build_plot_area(right)
        self._build_console(right)
        self._build_left_panel(left)

    def _panel(self, parent, title: str, row: int):
        frame = ttk.LabelFrame(parent, text=title, padding=10)
        frame.grid(row=row, column=0, sticky="ew", pady=(0, 10))
        frame.columnconfigure(0, weight=1)
        return frame

    def _build_top_cards(self, parent):
        cards = ttk.Frame(parent)
        cards.grid(row=0, column=0, sticky="ew", pady=(0, 10))
        for col in range(4):
            cards.columnconfigure(col, weight=1)
        self._card(cards, 0, "Mission", self.mission_card_var)
        self._card(cards, 1, "Pose", self.pose_card_var)
        self._card(cards, 2, "Run Time", self.time_card_var)
        self._card(cards, 3, "Blackbox", self.blackbox_status_var)

    def _card(self, parent, col: int, title: str, var: tk.StringVar):
        card = ttk.Frame(parent, style="Panel.TFrame", padding=(14, 10))
        card.grid(row=0, column=col, sticky="ew", padx=(0 if col == 0 else 8, 0))
        ttk.Label(card, text=title, style="CardTitle.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(card, textvariable=var, style="CardValue.TLabel").grid(row=1, column=0, sticky="w", pady=(3, 0))

    def _build_plot_area(self, parent):
        plot_panel = ttk.Frame(parent, style="Panel.TFrame", padding=8)
        plot_panel.grid(row=1, column=0, sticky="nsew")
        plot_panel.columnconfigure(0, weight=1)
        plot_panel.rowconfigure(0, weight=1)
        self.plot_frame = plot_panel

    def _build_console(self, parent):
        bottom = ttk.Frame(parent)
        bottom.grid(row=2, column=0, sticky="nsew", pady=(10, 0))
        bottom.columnconfigure(0, weight=1)
        bottom.rowconfigure(0, weight=1)
        console_frame = ttk.LabelFrame(bottom, text="Messages", padding=8)
        console_frame.grid(row=0, column=0, sticky="nsew")
        console_frame.columnconfigure(0, weight=1)
        console_frame.rowconfigure(0, weight=1)
        self.console = tk.Text(console_frame, height=7, wrap="word", bg="#0f172a", fg="#dbeafe", insertbackground="#dbeafe", relief="flat")
        self.console.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(console_frame, orient="vertical", command=self.console.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.console.configure(yscrollcommand=scroll.set)

    def _build_left_panel(self, parent):
        canvas = tk.Canvas(parent, width=420, bg="#edf1f5", highlightthickness=0)
        scroll = ttk.Scrollbar(parent, orient="vertical", command=canvas.yview)
        canvas.grid(row=0, column=0, sticky="nsw")
        scroll.grid(row=0, column=1, sticky="ns")
        body = ttk.Frame(canvas)
        canvas.create_window((0, 0), window=body, anchor="nw")
        canvas.configure(yscrollcommand=scroll.set)
        body.bind("<Configure>", lambda _e: canvas.configure(scrollregion=canvas.bbox("all")))
        body.columnconfigure(0, weight=1)

        conn = self._panel(body, "Connection", 0)
        self._labeled_entry(conn, 0, "Port", self.port_var)
        self._labeled_entry(conn, 1, "Baud", self.baud_var)
        self._labeled_entry(conn, 2, "Rate Hz", self.rate_var)
        ttk.Checkbutton(conn, text="Line raw", variable=self.rawline_var).grid(row=3, column=0, columnspan=2, sticky="w", pady=(4, 0))
        btns = ttk.Frame(conn, style="Panel.TFrame")
        btns.grid(row=4, column=0, columnspan=2, sticky="ew", pady=(8, 0))
        btns.columnconfigure(0, weight=1); btns.columnconfigure(1, weight=1)
        ttk.Button(btns, text="Connect", command=self.connect, style="Accent.TButton").grid(row=0, column=0, sticky="ew", padx=(0, 5))
        ttk.Button(btns, text="Disconnect", command=self.disconnect).grid(row=0, column=1, sticky="ew")

        actions = self._panel(body, "Run Control", 1)
        for idx, (text, cmd) in enumerate([
            ("Resume Telemetry", lambda: self.send_command("$RESUME\r\n")),
            ("Pause Telemetry", lambda: self.send_command("$PAUSE\r\n")),
            ("Reset PID", lambda: self.send_command("$RST\r\n")),
            ("Read Current", lambda: self.send_command("$DUMP\r\n")),
            ("Apply Rate", self.apply_rate),
            ("Apply RawLine", self.apply_rawline),
        ]):
            r, c = divmod(idx, 2)
            actions.columnconfigure(c, weight=1)
            ttk.Button(actions, text=text, command=cmd).grid(row=r, column=c, sticky="ew", pady=(0 if r == 0 else 6, 0), padx=(0, 5 if c == 0 else 0))

        notebook = ttk.Notebook(body)
        notebook.grid(row=2, column=0, sticky="ew", pady=(0, 10))
        notebook.add(self._pid_tab(notebook), text="PID")
        notebook.add(self._params_tab(notebook), text="Params")
        notebook.add(self._log_tab(notebook), text="Logs")
        notebook.add(self._view_tab(notebook), text="View")

        latest = self._panel(body, "Live State", 3)
        for row, var in enumerate((self.latest_app_var, self.latest_line_bias_var, self.latest_line_strength_var, self.latest_line_contrast_var, self.latest_line_on_var)):
            ttk.Label(latest, textvariable=var, style="Panel.TLabel").grid(row=row, column=0, sticky="w", pady=(0 if row == 0 else 3, 0))

        custom = self._panel(body, "Custom Command", 4)
        custom.columnconfigure(0, weight=1)
        ttk.Entry(custom, textvariable=self.command_var).grid(row=0, column=0, sticky="ew")
        ttk.Button(custom, text="Send", command=self.send_custom_command).grid(row=0, column=1, padx=(6, 0))

    def _labeled_entry(self, parent, row: int, label: str, var):
        ttk.Label(parent, text=label, style="Panel.TLabel").grid(row=row, column=0, sticky="w", pady=(0, 5))
        ttk.Entry(parent, textvariable=var).grid(row=row, column=1, sticky="ew", pady=(0, 5))
        parent.columnconfigure(1, weight=1)

    def _pid_tab(self, parent):
        tab = ttk.Frame(parent, padding=10)
        tab.columnconfigure(0, weight=1)
        speed = ttk.LabelFrame(tab, text="Wheel Speed PID", padding=8)
        speed.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        self._pid_grid(speed, ("L", "R"))
        line = ttk.LabelFrame(tab, text="Line Tracking PID", padding=8)
        line.grid(row=1, column=0, sticky="ew", pady=(0, 8))
        self._pid_grid(line, ("LINE",))
        ttk.Button(tab, text="Apply PID", command=self.apply_pid_params, style="Accent.TButton").grid(row=2, column=0, sticky="ew")
        return tab

    def _pid_grid(self, parent, channels):
        for col, text in enumerate(("Loop", "KP", "KI", "KD")):
            ttk.Label(parent, text=text, style="Panel.TLabel").grid(row=0, column=col, sticky="w", padx=(0, 6))
            parent.columnconfigure(col, weight=1 if col else 0)
        for row, ch in enumerate(channels, start=1):
            ttk.Label(parent, text=ch, style="Panel.TLabel").grid(row=row, column=0, sticky="w", pady=(4, 0))
            for col, gain in enumerate(("KP", "KI", "KD"), start=1):
                ttk.Entry(parent, textvariable=self.pid_vars[(ch, gain)], width=9).grid(row=row, column=col, sticky="ew", padx=(0, 6), pady=(4, 0))

    def _params_tab(self, parent):
        tab = ttk.Frame(parent, padding=10)
        tab.columnconfigure(0, weight=1)
        row = 0
        for title, items in CFG_GROUPS:
            group = ttk.LabelFrame(tab, text=title, padding=8)
            group.grid(row=row, column=0, sticky="ew", pady=(0, 8))
            group.columnconfigure(1, weight=1)
            for item_row, (name, label) in enumerate(items):
                ttk.Label(group, text=label, style="Panel.TLabel").grid(row=item_row, column=0, sticky="w", pady=(0, 5))
                ttk.Entry(group, textvariable=self.cfg_vars[name], width=12).grid(row=item_row, column=1, sticky="ew", pady=(0, 5))
            row += 1
        ttk.Button(tab, text="Apply Run Params", command=self.apply_cfg_params, style="Accent.TButton").grid(row=row, column=0, sticky="ew")
        return tab

    def _log_tab(self, parent):
        tab = ttk.Frame(parent, padding=10)
        tab.columnconfigure(0, weight=1)
        blackbox = ttk.LabelFrame(tab, text="On-Car RAM Blackbox", padding=8)
        blackbox.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        for col in range(2):
            blackbox.columnconfigure(col, weight=1)
        for idx, (text, cmd) in enumerate([
            ("Enable Log", lambda: self.send_command("$LOG,1\r\n")),
            ("Disable Log", lambda: self.send_command("$LOG,0\r\n")),
            ("Clear Log", lambda: self.send_command("$LOGCLR\r\n")),
            ("Dump Log", self.dump_blackbox),
            ("Save AI Pack", self.save_ai_pack),
        ]):
            r, c = divmod(idx, 2)
            span = 2 if text == "Save AI Pack" else 1
            ttk.Button(blackbox, text=text, command=cmd).grid(row=r, column=c, columnspan=span, sticky="ew", pady=(0 if r == 0 else 6, 0), padx=(0, 5 if c == 0 and span == 1 else 0))
        ttk.Label(blackbox, textvariable=self.blackbox_status_var, style="Muted.TLabel").grid(row=3, column=0, columnspan=2, sticky="w", pady=(6, 0))
        filebox = ttk.LabelFrame(tab, text="Live CSV", padding=8)
        filebox.grid(row=1, column=0, sticky="ew")
        filebox.columnconfigure(0, weight=1)
        ttk.Entry(filebox, textvariable=self.log_path_var).grid(row=0, column=0, sticky="ew")
        ttk.Button(filebox, text="New File", command=self.rotate_log_file).grid(row=1, column=0, sticky="ew", pady=(6, 0))
        return tab

    def _view_tab(self, parent):
        tab = ttk.Frame(parent, padding=10)
        tab.columnconfigure(1, weight=1)
        ttk.Label(tab, text="Time window", style="Panel.TLabel").grid(row=0, column=0, sticky="w", pady=(0, 5))
        ttk.Entry(tab, textvariable=self.time_window, width=10).grid(row=0, column=1, sticky="ew", pady=(0, 5))
        ttk.Label(tab, text="PID terms", style="Panel.TLabel").grid(row=1, column=0, sticky="w", pady=(0, 5))
        ttk.Combobox(tab, textvariable=self.focus_channel_var, values=list(PID_CHANNELS), state="readonly", width=10).grid(row=1, column=1, sticky="ew", pady=(0, 5))
        for idx, ch in enumerate(PID_CHANNELS, start=2):
            ttk.Checkbutton(tab, text=f"Show {ch}", variable=self.channel_enabled[ch]).grid(row=idx, column=0, columnspan=2, sticky="w")
        ttk.Button(tab, text="Clear Curves", command=self.clear_curves).grid(row=idx + 1, column=0, columnspan=2, sticky="ew", pady=(8, 0))
        return tab

    def _setup_plot(self):
        self.figure = Figure(figsize=(12.0, 7.6), dpi=100, facecolor="#ffffff")
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
        self.send_command("$LOG,1\r\n")
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

    def apply_pid_params(self):
        for (ch, gain), var in self.pid_vars.items():
            text = var.get().strip()
            if text:
                self.send_command(f"$SET,{ch},{gain},{float(text):.6g}\r\n")

    def apply_cfg_params(self):
        for name, var in self.cfg_vars.items():
            text = var.get().strip()
            if text:
                self.send_command(f"$CFGSET,{name},{float(text):.6g}\r\n")

    def dump_blackbox(self):
        self.blackbox_records = []
        self.blackbox_active = True
        self.blackbox_status_var.set("dumping...")
        self.send_command("$LOGDUMP\r\n")

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

    def _blackbox_path(self, suffix: str) -> Path:
        base = Path(self.log_path_var.get().strip())
        if not base.name:
            base = Path(self._default_log_path())
        return base.with_name(base.stem + suffix)

    def save_blackbox_csv(self) -> Path | None:
        if not self.blackbox_records:
            self.log("No blackbox records to save")
            return None
        path = self._blackbox_path("_blackbox.csv")
        with open(path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(BLACKBOX_HEADER)
            for rec in self.blackbox_records:
                writer.writerow(blackbox_csv_row(rec))
        self.log(f"Saved blackbox CSV: {path}")
        return path

    def save_ai_pack(self):
        blackbox_csv = self.save_blackbox_csv()
        path = self._blackbox_path("_ai_pack.json")
        pack = {
            "created_at": dt.datetime.now().isoformat(timespec="seconds"),
            "live_log_csv": self.log_path_var.get().strip(),
            "blackbox_csv": str(blackbox_csv) if blackbox_csv else None,
            "latest_app": self.latest_app,
            "latest_params": self.latest_params,
            "dashboard_pid_entries": {f"{ch}_{gain}": var.get() for (ch, gain), var in self.pid_vars.items()},
            "dashboard_run_params": {name: var.get() for name, var in self.cfg_vars.items()},
            "blackbox_records": self.blackbox_records,
            "notes_for_ai": [
                "Speed setpoint unit is encoder pulses per 1 ms tick; 0.50 is about 25 cm/s when ODOM_PULSES_PER_CM is 20.",
                "Watch L/R meas tracking sp, output saturation, line_bias edge cases, and on_line dropouts.",
                "If no wireless link is present, blackbox_records are the most useful post-run trace.",
            ],
        }
        with open(path, "w") as f:
            json.dump(pack, f, ensure_ascii=False, indent=2)
        self.log(f"Saved AI pack: {path}")
        messagebox.showinfo("AI Pack", f"Saved:\n{path}")

    def _consume_queue(self):
        updated = False
        while True:
            try:
                line = self.rx_queue.get_nowait()
            except queue.Empty:
                break
            pid_rec = parse_pid_line(line)
            if pid_rec is not None:
                self._handle_pid(pid_rec); self._write_csv(csv_row_from_pid(pid_rec)); updated = True; continue
            line_rec = parse_line_sensor(line)
            if line_rec is not None:
                self._handle_line(line_rec); self._write_csv(csv_row_from_line(line_rec)); updated = True; continue
            app_rec = parse_app_state(line)
            if app_rec is not None:
                self._handle_app(app_rec); self._write_csv(csv_row_from_app(app_rec)); updated = True; continue
            cfg_rec = parse_config(line)
            if cfg_rec is not None:
                self._handle_cfg(cfg_rec); self._write_csv(csv_row_from_cfg(cfg_rec)); continue
            gain_rec = parse_gain(line)
            if gain_rec is not None:
                self._handle_gain(gain_rec); self.log(line); continue
            b_rec = parse_blackbox_line(line)
            if b_rec is not None:
                self.blackbox_records.append(b_rec)
                if len(self.blackbox_records) % 25 == 0:
                    self.blackbox_status_var.set(f"{len(self.blackbox_records)} frames")
                continue
            if line.startswith("$BHEAD"):
                self.blackbox_records = []
                self.blackbox_active = True
                self.blackbox_status_var.set(line.replace("$BHEAD,", "head: "))
                self.log(line); continue
            if line.startswith("$BEND"):
                self.blackbox_active = False
                self.blackbox_status_var.set(f"{len(self.blackbox_records)} frames ready")
                self.save_blackbox_csv()
                self.log(line); continue
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
        self.channel_stats[ch]["sp"].set(f"sp {rec['sp']:.3f}")
        self.channel_stats[ch]["meas"].set(f"meas {rec['meas']:.3f}")
        self.channel_stats[ch]["out"].set(f"out {rec['out']:.1f}")
        self.channel_stats[ch]["err"].set(f"err {rec['err']:.3f}")

    def _handle_line(self, rec: dict):
        self.latest_raw = list(rec["raw"])
        self.latest_line_bias_var.set(f"bias {rec['line_bias']:.3f}")
        self.latest_line_strength_var.set(f"strength {rec['strength']}")
        self.latest_line_contrast_var.set(f"contrast {rec['contrast']}")
        self.latest_line_on_var.set(f"on_line {rec['on_line']}")

    def _handle_app(self, rec: dict):
        self.latest_app = rec
        state = STATE_NAME.get(rec["state"], str(rec["state"]))
        self.mission_card_var.set(f"H{rec['mission']}  {state}")
        self.pose_card_var.set(f"x {rec['x']:.1f}  y {rec['y']:.1f}  h {rec['theta_deg']:.0f} deg")
        self.time_card_var.set(f"{rec['mission_time_ms'] / 1000:.1f} s")
        self.latest_app_var.set(
            f"H{rec['mission']} {state}  loop {rec['loop'] + 1}  seg {rec['seg']}  seg_cm {rec['seg_cm']:.1f}"
        )

    def _handle_cfg(self, rec: dict):
        self.latest_params[rec["param"]] = rec
        if rec["param"] in self.cfg_vars:
            self.cfg_vars[rec["param"]].set(f"{rec['param_value']:.6g}")
        self.log(rec["raw_line"])

    def _handle_gain(self, rec: dict):
        ch = rec["ch"]
        for gain, key in (("KP", "kp"), ("KI", "ki"), ("KD", "kd")):
            if (ch, gain) in self.pid_vars:
                self.pid_vars[(ch, gain)].set(f"{rec[key]:.6g}")

    def _update_plots(self):
        window = max(1.0, float(self.time_window.get()))
        self.ax_track.cla(); self.ax_out.cla(); self.ax_terms.cla(); self.ax_raw.cla()
        for ax in (self.ax_track, self.ax_out, self.ax_terms, self.ax_raw):
            ax.set_facecolor("#fbfdff")
            ax.grid(True, alpha=0.22)
        self.ax_track.set_title("Setpoint / Measure")
        self.ax_out.set_title("Output PWM")
        self.ax_terms.set_title(f"P / I / D Terms ({self.focus_channel_var.get()})")
        self.ax_raw.set_title("7-way Line Sensors")

        latest_t = 0.0
        for ch in PID_CHANNELS:
            if not self.channel_enabled[ch].get() or not self.series[ch]["t"]:
                continue
            latest_t = max(latest_t, self.series[ch]["t"][-1])
            t_vals = list(self.series[ch]["t"])
            self.ax_track.plot(t_vals, list(self.series[ch]["sp"]), label=f"{ch} sp", linewidth=1.6)
            self.ax_track.plot(t_vals, list(self.series[ch]["meas"]), label=f"{ch} meas", linewidth=1.2)
            self.ax_out.plot(t_vals, list(self.series[ch]["out"]), label=ch, linewidth=1.4)
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
        colors = ["#2563eb"] * RAW_SENSOR_COUNT
        if any(self.latest_raw):
            colors[self.latest_raw.index(max(self.latest_raw))] = "#ef4444"
        self.ax_raw.bar(x, self.latest_raw, color=colors, width=0.72)
        self.ax_raw.set_xticks(x)
        self.ax_raw.set_xticklabels([f"S{i}" for i in x])
        self.ax_raw.set_ylabel("Raw")
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
