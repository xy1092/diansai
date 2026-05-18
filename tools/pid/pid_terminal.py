#!/usr/bin/env python3
"""
PID 数据采集 + 终端实时显示 + 可选 AI 调参建议。

用法：
    python3 pid_terminal.py --port /dev/ttyACM0
    python3 pid_terminal.py --port /dev/ttyACM0 --ch L R --duration 30
    python3 pid_terminal.py --port /dev/ttyACM0 --ai-tune
    python3 pid_terminal.py --port /dev/ttyACM0 --ai-tune --duration 10
"""
from __future__ import annotations
import argparse
import csv
import datetime as dt
import json
import os
import queue
import signal
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path

import serial

STOP = threading.Event()
RX_Q: queue.Queue = queue.Queue(maxsize=5000)


# ── 串口读取 ──────────────────────────────────────

def reader_thread(ser: serial.Serial):
    buf = bytearray()
    while not STOP.is_set():
        try:
            chunk = ser.read(256)
        except Exception as e:
            print(f"\n[serial] read error: {e}", file=sys.stderr)
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


# ── 解析 ──────────────────────────────────────────

def parse_pid_line(line: str) -> dict | None:
    if not line.startswith("$P,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    try:
        return {
            "kind": "pid", "ts_ms": int(parts[1]), "ch": parts[2],
            "sp": float(parts[3]), "meas": float(parts[4]),
            "out": float(parts[5]), "err": float(parts[6]),
            "integ": float(parts[7]), "deriv": float(parts[8]),
            "p_term": float(parts[9]), "i_term": float(parts[10]),
            "d_term": float(parts[11]), "raw_out": float(parts[12]),
        }
    except ValueError:
        return None


def parse_line_sensor(line: str) -> dict | None:
    if not line.startswith("$L,"):
        return None
    parts = line.split(",")
    if len(parts) < 13:
        return None
    try:
        return {
            "kind": "line", "ts_ms": int(parts[1]),
            "line_bias": float(parts[2]), "contrast": int(parts[3]),
            "strength": int(parts[4]), "on_line": int(parts[5]),
            "raw": [int(v) for v in parts[6:13]],
        }
    except ValueError:
        return None


# ── 终端显示 ──────────────────────────────────────

CLEAR = "\033[2J\033[H"
BOLD = "\033[1m"
DIM = "\033[2m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
CYAN = "\033[36m"
RESET = "\033[0m"


def render_dashboard(latest: dict, channels: list[str], line_info: dict | None,
                     frame_count: int, start_time: float):
    """终端内联渲染当前状态。"""
    lines_out = []
    elapsed = time.time() - start_time
    lines_out.append(f"{BOLD}PID Monitor{CLEAR if frame_count == 0 else ''}{RESET}  "
                     f"{DIM}elapsed {elapsed:.0f}s  frames {frame_count}{RESET}")
    lines_out.append("─" * 64)

    # 每通道一行
    for ch in channels:
        d = latest.get(ch)
        if d is None:
            lines_out.append(f"  {ch}: {DIM}waiting...{RESET}")
        else:
            err_color = YELLOW if abs(d["err"]) < 0.5 else ""
            lines_out.append(
                f"  {BOLD}{ch}{RESET}  "
                f"sp={d['sp']:7.3f}  meas={d['meas']:7.3f}  "
                f"out={d['out']:7.3f}  {err_color}err={d['err']:+7.3f}{RESET}  "
                f"P={d['p_term']:+.3f} I={d['i_term']:+.3f} D={d['d_term']:+.3f}"
            )

    # 循迹传感器
    if line_info:
        raw = line_info.get("raw", [])
        bars = " ".join(f"S{i}{'█' * (v // 200)}" for i, v in enumerate(raw))
        lines_out.append(f"  Line: bias={line_info['line_bias']:.3f}  "
                         f"str={line_info['strength']}  "
                         f"on={line_info['on_line']}  [{bars}]")

    lines_out.append("─" * 64)
    lines_out.append(f"  q=退出  a=AI调参建议  "
                     f"{DIM}输出 CSV→ {Path.cwd() / 'pid_log.csv'}{RESET}")

    # 清屏 + 重绘（第一帧除外）
    if frame_count > 0:
        lines_out.insert(0, "\033[H")
    print("\n".join(lines_out))


# ── AI 调参 ───────────────────────────────────────

def build_ai_prompt(records: list[dict], channels: list[str]) -> str:
    """根据采集数据构造 AI 调参提示词。"""
    stats_lines = []
    for ch in channels:
        ch_recs = [r for r in records if r["ch"] == ch]
        if not ch_recs:
            continue
        errs = [r["err"] for r in ch_recs]
        outs = [r["out"] for r in ch_recs]
        stats_lines.append(
            f"  {ch}: 样本{len(ch_recs)}  "
            f"err均值={statistics.mean(errs):.4f}  err标准差={statistics.stdev(errs):.4f}  "
            f"err范围=[{min(errs):.4f}, {max(errs):.4f}]  "
            f"out均值={statistics.mean(outs):.3f}  out范围=[{min(outs):.3f}, {max(outs):.3f}]"
        )

    # 检测振荡周期
    ch0 = channels[0]
    ch0_recs = [r for r in records if r["ch"] == ch0]
    osc_hint = ""
    if len(ch0_recs) > 10:
        err_seq = [r["err"] for r in ch0_recs]
        crossings = 0
        for i in range(1, len(err_seq)):
            if err_seq[i - 1] * err_seq[i] < 0:
                crossings += 1
        if crossings > 3:
            osc_hint = f"\n  检测到 {crossings} 次过零，可能存在振荡。"

    return f"""你是 PID 调参专家。以下是 MSPM0G3507 电机 PID 实测数据：

通道: {', '.join(channels)}
{chr(10).join(stats_lines)}{osc_hint}

请给出调参建议：
1. 当前可能的问题（过冲/振荡/响应慢/稳态误差大）
2. 每个通道的 Kp/Ki/Kd 调整方向和幅度
3. 建议的新参数值"""


def call_ai_tune(records: list[dict], channels: list[str]):
    """尝试调用 AI 输出调参建议。"""
    prompt = build_ai_prompt(records, channels)

    # 优先用 claude CLI
    if os.environ.get("ANTHROPIC_AUTH_TOKEN"):
        print(f"\n{CYAN}正在询问 AI...{RESET}")
        try:
            proc = subprocess.run(
                ["claude", "-p", prompt, "--output-format", "text"],
                capture_output=True, text=True, timeout=30,
            )
            if proc.returncode == 0 and proc.stdout.strip():
                print(f"\n{CYAN}── AI 调参建议 ──{RESET}\n{proc.stdout}")
                return
        except Exception:
            pass

    # 降级：打印提示词模板
    print(f"\n{YELLOW}无法调用 AI（未配置 API），"
          f"请将以下内容发给 Claude/Codex：{RESET}\n")
    print("─" * 50)
    print(prompt)
    print("─" * 50)


# ── 主程序 ────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--ch", nargs="+", default=["L", "R", "LINE"],
                    help="订阅通道，如 L R LINE")
    ap.add_argument("--duration", type=float, default=0,
                    help="采集时长(秒)，0=持续运行")
    ap.add_argument("--ai-tune", action="store_true",
                    help="采集结束后调用 AI 出调参建议")
    ap.add_argument("--rate", type=int, default=100)
    ap.add_argument("--line-raw", action="store_true")
    ap.add_argument("--csv", default="pid_log.csv")
    args = ap.parse_args()

    print(f"{CLEAR}{BOLD}PID Terminal Monitor{RESET}")
    print(f"  port={args.port}  baud={args.baud}  ch={args.ch}")
    print(f"  rate={args.rate}Hz  line_raw={args.line_raw}")
    print(f"  csv={args.csv}")
    print(f"  {'AI调参模式' if args.ai_tune else '实时显示模式'}")
    print()

    STOP.clear()
    try:
        ser = serial.serial_for_url(args.port, args.baud, timeout=0.2)
    except Exception as e:
        print(f"[serial] 打开失败: {e}", file=sys.stderr)
        return 1

    reader = threading.Thread(target=reader_thread, args=(ser,), daemon=True)
    reader.start()

    # 发送初始化命令
    def send(cmd: str):
        ser.write(cmd.encode("ascii"))
        ser.flush()

    time.sleep(0.05)
    send(f"$RATE,{args.rate}\r\n")
    send(f"$RAWLINE,{1 if args.line_raw else 0}\r\n")
    send("$RESUME\r\n")
    send("$DUMP\r\n")

    # CSV
    csv_f = open(args.csv, "w", newline="")
    csv_w = csv.writer(csv_f)
    csv_w.writerow([
        "kind", "ts_ms", "ch", "sp", "meas", "out", "err", "integ", "deriv",
        "p_term", "i_term", "d_term", "raw_out",
        "line_bias", "contrast", "strength", "on_line",
        "r0", "r1", "r2", "r3", "r4", "r5", "r6",
    ])

    latest: dict[str, dict] = {}
    line_info: dict | None = None
    records: list[dict] = []  # for AI analysis
    frame = 0
    t_start = time.time()
    last_ai_check = t_start

    try:
        while not STOP.is_set():
            # 超时检查
            if args.duration > 0 and (time.time() - t_start) > args.duration:
                STOP.set()
                break

            # 消费串口队列
            updated = False
            while True:
                try:
                    line = RX_Q.get(timeout=0.05)
                except queue.Empty:
                    break
                updated = True

                pr = parse_pid_line(line)
                if pr and pr["ch"] in args.ch:
                    latest[pr["ch"]] = pr
                    records.append(pr)
                    csv_w.writerow([
                        pr["kind"], pr["ts_ms"], pr["ch"],
                        pr["sp"], pr["meas"], pr["out"], pr["err"],
                        pr["integ"], pr["deriv"], pr["p_term"], pr["i_term"],
                        pr["d_term"], pr["raw_out"],
                        "", "", "", "", "", "", "", "", "", "", "",
                    ])
                    continue

                lr = parse_line_sensor(line)
                if lr:
                    line_info = lr
                    records.append(lr)
                    csv_w.writerow([
                        lr["kind"], lr["ts_ms"], "LINE_RAW",
                        "", "", "", "", "", "", "", "", "", "",
                        lr["line_bias"], lr["contrast"], lr["strength"],
                        lr["on_line"],
                        *lr["raw"],
                    ])
                    continue

            if updated:
                frame += 1
                render_dashboard(latest, args.ch, line_info, frame, t_start)
                csv_f.flush()

            # 非交互模式，跳过键盘检测
            if not sys.stdin.isatty():
                time.sleep(0.1)
                continue

    except KeyboardInterrupt:
        pass
    finally:
        STOP.set()
        ser.close()
        csv_f.close()

    print(f"\n{DIM}已保存 {len(records)} 条记录 → {args.csv}{RESET}")

    if args.ai_tune and records:
        call_ai_tune(records, args.ch)

    return 0


if __name__ == "__main__":
    raise SystemExit(main() or 0)
