#!/usr/bin/env python3
"""Web-based PID control panel for NUEDC car.

Backend: FastAPI + WebSocket. Frontend: tools/pid/web (static files).

Transport is anything pyserial.serial_for_url accepts:
  /dev/ttyACM0          USB CDC
  COM3                  Windows
  socket://1.2.3.4:3333 ESP32 Wi-Fi UART bridge
  rfc2217://host:port   RFC2217 server
"""
from __future__ import annotations

import argparse
import asyncio
import csv
import datetime as dt
import json
import os
import queue
import threading
import time
import webbrowser
from collections import deque
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Any

import serial
import uvicorn
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse, RedirectResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

HERE = Path(__file__).resolve().parent
WEB_DIR = HERE / "web"
LOG_DIR = HERE / "logs"
LOG_DIR.mkdir(exist_ok=True)

PID_CHANNELS = ("L", "R", "LINE", "ANG")
RAW_SENSOR_COUNT = 7
HISTORY_LEN = 600  # ~6 s @ 100 Hz

STATE_NAME = {0: "IDLE", 1: "READY", 2: "RUN", 3: "STOP", 4: "ERROR"}

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


def _f(s: str) -> float:
    """Tolerant float: empty / 'nan' / 'inf' become 0.0.
    Firmware built with newlib-nano without -u _printf_float prints
    nothing for %f, leaving empty fields between commas."""
    s = s.strip()
    if not s:
        return 0.0
    try:
        v = float(s)
        if v != v or v in (float("inf"), float("-inf")):
            return 0.0
        return v
    except ValueError:
        return 0.0


def _i(s: str) -> int:
    s = s.strip()
    if not s:
        return 0
    try:
        return int(s)
    except ValueError:
        try:
            return int(float(s))
        except ValueError:
            return 0


def parse_pid(line: str):
    if not line.startswith("$P,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    try:
        return {
            "kind": "pid", "ts_ms": _i(parts[1]), "ch": parts[2],
            "sp": _f(parts[3]), "meas": _f(parts[4]), "out": _f(parts[5]),
            "err": _f(parts[6]), "integ": _f(parts[7]), "deriv": _f(parts[8]),
            "p": _f(parts[9]), "i": _f(parts[10]),
            "d": _f(parts[11]), "raw_out": _f(parts[12]),
        }
    except Exception:
        return None


def parse_line(line: str):
    if not line.startswith("$L,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    try:
        return {
            "kind": "line", "ts_ms": _i(parts[1]), "bias": _f(parts[2]),
            "contrast": _i(parts[3]), "strength": _i(parts[4]), "on_line": _i(parts[5]),
            "raw": [_i(v) for v in parts[6:13]],
        }
    except Exception:
        return None


def parse_app(line: str):
    if not line.startswith("$A,"):
        return None
    parts = line.split(",")
    if len(parts) < 11:
        return None
    try:
        return {
            "kind": "app", "ts_ms": _i(parts[1]), "mission": _i(parts[2]),
            "state": _i(parts[3]), "loop": _i(parts[4]), "seg": _i(parts[5]),
            "x": _f(parts[6]), "y": _f(parts[7]), "theta_deg": _f(parts[8]),
            "seg_cm": _f(parts[9]), "mission_time_ms": _i(parts[10]),
            "state_name": STATE_NAME.get(_i(parts[3]), str(parts[3])),
        }
    except Exception:
        return None


def parse_gain(line: str):
    if not line.startswith("$G,"):
        return None
    parts = line.split(",")
    if len(parts) < 5:
        return None
    try:
        return {"kind": "gain", "ch": parts[1], "kp": _f(parts[2]),
                "ki": _f(parts[3]), "kd": _f(parts[4])}
    except Exception:
        return None


def parse_cfg(line: str):
    if not line.startswith("$C,"):
        return None
    parts = line.split(",")
    if len(parts) < 5:
        return None
    try:
        return {"kind": "cfg", "param": parts[1], "value": _f(parts[2]),
                "min": _f(parts[3]), "max": _f(parts[4])}
    except Exception:
        return None


def parse_blackbox(line: str):
    if not line.startswith("$B,"):
        return None
    parts = line.split(",")
    if len(parts) < 21:
        return None
    try:
        vals = [
            _i(parts[1]), _i(parts[2]), _i(parts[3]), _i(parts[4]), _i(parts[5]), _i(parts[6]),
            _f(parts[7]), _f(parts[8]), _f(parts[9]), _f(parts[10]),
            _f(parts[11]), _f(parts[12]), _i(parts[13]),
            _f(parts[14]), _f(parts[15]), _i(parts[16]),
            _f(parts[17]), _i(parts[18]), _i(parts[19]), _i(parts[20]),
        ]
        return dict(zip(BLACKBOX_HEADER, vals))
    except Exception:
        return None


class SerialBus:
    """Read/write a pyserial-compatible URL on a background thread."""

    def __init__(self, on_line, on_status):
        self.on_line = on_line
        self.on_status = on_status
        self.ser: serial.Serial | None = None
        self.thread: threading.Thread | None = None
        self.stop_evt = threading.Event()
        self.url: str | None = None
        self.baud: int = 115200
        self._lock = threading.Lock()

    @property
    def connected(self) -> bool:
        return self.ser is not None

    def connect(self, url: str, baud: int):
        with self._lock:
            if self.ser is not None:
                self.disconnect_locked()
            self.url = url
            self.baud = baud
            self.stop_evt.clear()
            self.ser = serial.serial_for_url(url, baud, timeout=0.2)
            self.thread = threading.Thread(target=self._reader, daemon=True)
            self.thread.start()
        self.on_status(f"connected to {url} @ {baud}")

    def disconnect_locked(self):
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

    def disconnect(self):
        with self._lock:
            was = self.ser is not None
            self.disconnect_locked()
        if was:
            self.on_status("disconnected")

    def send(self, text: str):
        with self._lock:
            ser = self.ser
            if ser is None:
                raise RuntimeError("not connected")
            if not text.endswith("\n"):
                text += "\r\n"
            ser.write(text.encode("ascii", errors="ignore"))
            ser.flush()

    def _reader(self):
        assert self.ser is not None
        buf = bytearray()
        ser = self.ser
        while not self.stop_evt.is_set():
            try:
                chunk = ser.read(256)
            except Exception as exc:
                self.on_status(f"serial-error {exc}")
                break
            if not chunk:
                continue
            buf.extend(chunk)
            while b"\n" in buf:
                line, _, rest = buf.partition(b"\n")
                buf = bytearray(rest)
                txt = line.decode("ascii", errors="ignore").strip()
                if txt:
                    self.on_line(txt)


class State:
    def __init__(self):
        self.t0_ms: int | None = None
        self.channels: dict[str, dict[str, deque]] = {
            ch: {k: deque(maxlen=HISTORY_LEN) for k in ("t", "sp", "meas", "out", "p", "i", "d", "err")}
            for ch in PID_CHANNELS
        }
        self.latest_pid: dict[str, dict] = {}
        self.latest_line: dict | None = None
        self.latest_app: dict | None = None
        self.gains: dict[str, dict] = {ch: {"kp": 0.0, "ki": 0.0, "kd": 0.0} for ch in PID_CHANNELS}
        self.params: dict[str, dict] = {}
        self.blackbox: list[dict] = []
        self.blackbox_active: bool = False
        self.frame_count: int = 0
        self.start_time: float = time.time()
        self.lock = threading.Lock()

    def push_pid(self, rec: dict):
        with self.lock:
            ch = rec["ch"]
            ser = self.channels.get(ch)
            if ser is None:
                return
            if self.t0_ms is None:
                self.t0_ms = rec["ts_ms"]
            t = (rec["ts_ms"] - self.t0_ms) / 1000.0
            ser["t"].append(t)
            for k in ("sp", "meas", "out", "p", "i", "d", "err"):
                ser[k].append(rec[k])
            self.latest_pid[ch] = rec
            self.frame_count += 1

    def push_line(self, rec: dict):
        with self.lock:
            self.latest_line = rec

    def push_app(self, rec: dict):
        with self.lock:
            self.latest_app = rec

    def push_gain(self, rec: dict):
        with self.lock:
            ch = rec["ch"].upper()
            if ch in self.gains:
                self.gains[ch] = {"kp": rec["kp"], "ki": rec["ki"], "kd": rec["kd"]}

    def push_cfg(self, rec: dict):
        with self.lock:
            self.params[rec["param"]] = rec

    def snapshot(self) -> dict[str, Any]:
        with self.lock:
            out = {
                "connected": False,
                "frame_count": self.frame_count,
                "uptime": time.time() - self.start_time,
                "channels": {},
                "latest_pid": self.latest_pid.copy(),
                "latest_line": self.latest_line,
                "latest_app": self.latest_app,
                "gains": {ch: g.copy() for ch, g in self.gains.items()},
                "params": {k: v.copy() for k, v in self.params.items()},
                "blackbox_count": len(self.blackbox),
                "blackbox_active": self.blackbox_active,
            }
            for ch, s in self.channels.items():
                out["channels"][ch] = {k: list(v) for k, v in s.items()}
            return out


class CsvLogger:
    def __init__(self):
        self.fh = None
        self.writer = None
        self.path: Path | None = None
        self.lock = threading.Lock()

    def open(self, path: Path):
        self.close()
        path.parent.mkdir(parents=True, exist_ok=True)
        self.fh = open(path, "w", newline="")
        self.writer = csv.writer(self.fh)
        self.writer.writerow(CSV_HEADER)
        self.path = path

    def write(self, kind: str, row: dict, raw_line: str = ""):
        with self.lock:
            if self.writer is None:
                return
            base = {h: "" for h in CSV_HEADER}
            base["kind"] = kind
            base["raw_line"] = raw_line
            base.update({k: v for k, v in row.items() if k in base})
            self.writer.writerow([base[h] for h in CSV_HEADER])
            if self.fh:
                self.fh.flush()

    def close(self):
        with self.lock:
            if self.fh is not None:
                try:
                    self.fh.close()
                except Exception:
                    pass
            self.fh = None
            self.writer = None


# ---------------- FastAPI app ----------------

class ConnectReq(BaseModel):
    port: str
    baud: int = 115200


class CmdReq(BaseModel):
    text: str


class PidSetReq(BaseModel):
    ch: str
    kp: float | None = None
    ki: float | None = None
    kd: float | None = None


class CfgSetReq(BaseModel):
    name: str
    value: float


class RateReq(BaseModel):
    hz: int


class RawlineReq(BaseModel):
    on: bool


def make_app(args) -> FastAPI:
    event_queue: queue.Queue[dict] = queue.Queue(maxsize=20000)
    state = State()
    csv_logger = CsvLogger()
    ws_clients: set[WebSocket] = set()
    main_loop: asyncio.AbstractEventLoop | None = None

    def push_event(ev: dict):
        try:
            event_queue.put_nowait(ev)
        except queue.Full:
            pass

    def on_line(text: str):
        # parse and update state in worker thread, enqueue for ws broadcast
        rec = parse_pid(text)
        if rec is not None:
            state.push_pid(rec)
            csv_logger.write("pid", rec, raw_line=text)
            push_event({"type": "pid", "data": rec})
            return
        rec = parse_line(text)
        if rec is not None:
            state.push_line(rec)
            row = {**rec, **{f"raw{i}": v for i, v in enumerate(rec["raw"])}}
            row.pop("raw", None)
            csv_logger.write("line", row, raw_line=text)
            push_event({"type": "line", "data": rec})
            return
        rec = parse_app(text)
        if rec is not None:
            state.push_app(rec)
            csv_logger.write("app", rec, raw_line=text)
            push_event({"type": "app", "data": rec})
            return
        rec = parse_gain(text)
        if rec is not None:
            state.push_gain(rec)
            push_event({"type": "gain", "data": rec})
            push_event({"type": "log", "data": {"text": text}})
            return
        rec = parse_cfg(text)
        if rec is not None:
            state.push_cfg(rec)
            csv_logger.write("cfg", rec, raw_line=text)
            push_event({"type": "cfg", "data": rec})
            push_event({"type": "log", "data": {"text": text}})
            return
        rec = parse_blackbox(text)
        if rec is not None:
            with state.lock:
                state.blackbox.append(rec)
                count = len(state.blackbox)
            push_event({"type": "blackbox", "data": {"count": count}})
            return
        if text.startswith("$BHEAD"):
            with state.lock:
                state.blackbox = []
                state.blackbox_active = True
            push_event({"type": "log", "data": {"text": text}})
            push_event({"type": "blackbox", "data": {"count": 0, "head": text}})
            return
        if text.startswith("$BEND"):
            with state.lock:
                state.blackbox_active = False
                count = len(state.blackbox)
            push_event({"type": "log", "data": {"text": text}})
            push_event({"type": "blackbox", "data": {"count": count, "done": True}})
            return
        push_event({"type": "log", "data": {"text": text}})

    def on_status(msg: str):
        push_event({"type": "status", "data": {"text": msg, "connected": bus.connected}})

    bus = SerialBus(on_line, on_status)

    async def broadcaster():
        nonlocal main_loop
        main_loop = asyncio.get_running_loop()
        while True:
            try:
                ev = await asyncio.to_thread(event_queue.get, True, 1.0)
            except queue.Empty:
                continue
            dead = []
            data = json.dumps(ev, ensure_ascii=False)
            for ws in list(ws_clients):
                try:
                    await ws.send_text(data)
                except Exception:
                    dead.append(ws)
            for ws in dead:
                ws_clients.discard(ws)

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        task = asyncio.create_task(broadcaster())
        try:
            yield
        finally:
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass
            bus.disconnect()
            csv_logger.close()

    app = FastAPI(lifespan=lifespan, title="NUEDC PID Web Dashboard")

    @app.get("/", include_in_schema=False)
    async def root():
        return RedirectResponse("/static/index.html")

    @app.get("/api/snapshot")
    async def get_snapshot():
        snap = state.snapshot()
        snap["connected"] = bus.connected
        snap["port"] = bus.url
        snap["baud"] = bus.baud
        snap["csv_path"] = str(csv_logger.path) if csv_logger.path else None
        return snap

    @app.post("/api/connect")
    async def api_connect(req: ConnectReq):
        try:
            bus.connect(req.port, req.baud)
        except Exception as exc:
            return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)
        # auto open CSV log
        stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
        csv_logger.open(LOG_DIR / f"dashboard_{stamp}.csv")
        # send default startup commands, spaced out so the MCU UART RX
        # parser does not drop commands while still processing the
        # previous one. $CFGDUMP is not implemented on firmware side.
        await asyncio.to_thread(time.sleep, 0.05)
        for cmd in (f"$RATE,{args.rate}\r\n", "$RAWLINE,1\r\n", "$LOG,1\r\n",
                    "$RESUME\r\n", "$DUMP\r\n"):
            try:
                bus.send(cmd)
            except Exception:
                break
            await asyncio.to_thread(time.sleep, 0.05)
        return {"ok": True, "csv_path": str(csv_logger.path)}

    @app.post("/api/disconnect")
    async def api_disconnect():
        bus.disconnect()
        csv_logger.close()
        return {"ok": True}

    @app.post("/api/cmd")
    async def api_cmd(req: CmdReq):
        try:
            bus.send(req.text)
        except Exception as exc:
            return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)
        push_event({"type": "log", "data": {"text": f">> {req.text.strip()}"}})
        return {"ok": True}

    @app.post("/api/pid/set")
    async def api_pid_set(req: PidSetReq):
        ch = req.ch.upper()
        if ch not in PID_CHANNELS:
            return JSONResponse({"ok": False, "error": f"bad channel {ch}"}, status_code=400)
        sent = []
        for gain, val in (("KP", req.kp), ("KI", req.ki), ("KD", req.kd)):
            if val is None:
                continue
            cmd = f"$SET,{ch},{gain},{val:.6g}\r\n"
            try:
                bus.send(cmd)
                sent.append(cmd.strip())
                push_event({"type": "log", "data": {"text": f">> {cmd.strip()}"}})
            except Exception as exc:
                return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)
        return {"ok": True, "sent": sent}

    @app.post("/api/cfg/set")
    async def api_cfg_set(req: CfgSetReq):
        cmd = f"$CFGSET,{req.name},{req.value:.6g}\r\n"
        try:
            bus.send(cmd)
        except Exception as exc:
            return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)
        push_event({"type": "log", "data": {"text": f">> {cmd.strip()}"}})
        return {"ok": True}

    @app.post("/api/rate")
    async def api_rate(req: RateReq):
        cmd = f"$RATE,{int(req.hz)}\r\n"
        bus.send(cmd)
        push_event({"type": "log", "data": {"text": f">> {cmd.strip()}"}})
        return {"ok": True}

    @app.post("/api/rawline")
    async def api_rawline(req: RawlineReq):
        cmd = f"$RAWLINE,{1 if req.on else 0}\r\n"
        bus.send(cmd)
        push_event({"type": "log", "data": {"text": f">> {cmd.strip()}"}})
        return {"ok": True}

    @app.post("/api/log/{action}")
    async def api_log(action: str):
        mapping = {
            "enable": "$LOG,1\r\n",
            "disable": "$LOG,0\r\n",
            "clear": "$LOGCLR\r\n",
            "dump": "$LOGDUMP\r\n",
            "reset_pid": "$RST\r\n",
            "pause": "$PAUSE\r\n",
            "resume": "$RESUME\r\n",
            "dump_gains": "$DUMP\r\n",
            "dump_cfg": "$DUMP\r\n",  # firmware $DUMP also emits $C,... cfg lines
        }
        if action not in mapping:
            return JSONResponse({"ok": False, "error": f"unknown action {action}"}, status_code=400)
        try:
            bus.send(mapping[action])
        except Exception as exc:
            return JSONResponse({"ok": False, "error": str(exc)}, status_code=400)
        push_event({"type": "log", "data": {"text": f">> {mapping[action].strip()}"}})
        return {"ok": True}

    @app.post("/api/blackbox/save")
    async def api_blackbox_save():
        with state.lock:
            recs = list(state.blackbox)
        if not recs:
            return JSONResponse({"ok": False, "error": "no records"}, status_code=400)
        base = csv_logger.path or (LOG_DIR / f"dashboard_{dt.datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
        out_path = base.with_name(base.stem + "_blackbox.csv")
        with open(out_path, "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(BLACKBOX_HEADER)
            for r in recs:
                w.writerow([r[k] for k in BLACKBOX_HEADER])
        return {"ok": True, "path": str(out_path), "rows": len(recs)}

    @app.websocket("/ws")
    async def ws_endpoint(ws: WebSocket):
        await ws.accept()
        ws_clients.add(ws)
        try:
            # send initial snapshot
            snap = state.snapshot()
            snap["connected"] = bus.connected
            snap["port"] = bus.url
            snap["baud"] = bus.baud
            await ws.send_text(json.dumps({"type": "snapshot", "data": snap}))
            while True:
                # keep open; ignore client messages
                await ws.receive_text()
        except WebSocketDisconnect:
            pass
        finally:
            ws_clients.discard(ws)

    if not WEB_DIR.exists():
        WEB_DIR.mkdir(parents=True, exist_ok=True)
    app.mount("/static", StaticFiles(directory=str(WEB_DIR), html=True), name="static")

    return app


def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--web-port", type=int, default=8765)
    ap.add_argument("--port", default="/dev/ttyACM0", help="default serial URL shown in UI")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--rate", type=int, default=100)
    ap.add_argument("--open", action="store_true", help="open browser after start")
    ap.add_argument("--autoconnect", action="store_true")
    return ap.parse_args()


def main():
    args = parse_args()
    app = make_app(args)
    # expose defaults to frontend via a tiny endpoint
    @app.get("/api/defaults")
    async def defaults():
        return {"port": args.port, "baud": args.baud, "rate": args.rate,
                "autoconnect": args.autoconnect}

    if args.open:
        threading.Timer(0.8, lambda: webbrowser.open(f"http://{args.host}:{args.web_port}/")).start()

    uvicorn.run(app, host=args.host, port=args.web_port, log_level="warning")


if __name__ == "__main__":
    main()
