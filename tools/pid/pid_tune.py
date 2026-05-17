#!/usr/bin/env python3
"""
交互式 REPL，向 MCU 实时发 PID 命令。

常用：
    set L kp 2.5           → "$SET,L,KP,2.5"
    set line kd 0.1
    rate 200
    rst                    → 重置积分
    pause / resume
    dump                   → 打印当前增益
    quit / Ctrl-D

用法：
    python3 pid_tune.py --port /dev/ttyACM0
"""
from __future__ import annotations
import argparse
import sys
import threading
import time
import serial


HELP = """\
命令参考：
  set <ch> <kp|ki|kd> <val>   修改增益（ch: L | R | line | ang）
  rate <hz>                   修改遥测速率（1-500）
  rawline <on|off|1|0>        开/关 7 路灰度原始流
  rst                         重置所有积分项
  pause | resume              暂停/恢复遥测发送
  dump                        让 MCU 回传当前增益
  raw <text>                  原样发送（需自己加 $ 和 \\r\\n）
  help                        显示本帮助
  quit                        退出
"""


def reader_thread(ser: serial.Serial, stop_evt: threading.Event):
    buf = bytearray()
    while not stop_evt.is_set():
        try:
            chunk = ser.read(256)
        except Exception:
            return
        if not chunk:
            continue
        buf.extend(chunk)
        while b"\n" in buf:
            line, _, rest = buf.partition(b"\n")
            buf = bytearray(rest)
            s = line.decode("ascii", errors="ignore").strip()
            if not s:
                continue
            # 只回显非遥测流的消息，避免刷屏
            if s.startswith("$P,"):
                continue
            print(f"\r<< {s}")
            sys.stdout.write(">> ")
            sys.stdout.flush()


def translate(cmd: str) -> str | None:
    c = cmd.strip()
    if not c:
        return None
    if c in ("help", "?"):
        print(HELP)
        return None
    if c in ("quit", "exit", "q"):
        raise SystemExit(0)

    parts = c.split()
    head = parts[0].lower()

    if head == "set" and len(parts) == 4:
        ch    = parts[1].upper()
        gain  = parts[2].upper()
        try:
            val = float(parts[3])
        except ValueError:
            print("  ! 第 3 个参数不是数字")
            return None
        return f"$SET,{ch},{gain},{val}\r\n"
    if head == "rate" and len(parts) == 2:
        return f"$RATE,{parts[1]}\r\n"
    if head == "rawline" and len(parts) == 2:
        val = parts[1].lower()
        if val in ("on", "1", "true"):
            return "$RAWLINE,1\r\n"
        if val in ("off", "0", "false"):
            return "$RAWLINE,0\r\n"
        print("  ! rawline 只接受 on/off/1/0")
        return None
    if head == "rst":
        return "$RST\r\n"
    if head == "pause":
        return "$PAUSE\r\n"
    if head == "resume":
        return "$RESUME\r\n"
    if head == "dump":
        return "$DUMP\r\n"
    if head == "raw":
        rest = c[3:].lstrip()
        return rest if rest.endswith("\n") else rest + "\r\n"

    print(f"  ! 无法识别: {c}  (输入 help)")
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("--baud", type=int, default=115200)
    args = ap.parse_args()

    ser = serial.serial_for_url(args.port, args.baud, timeout=0.2)
    print(f"[tune] 连接 {args.port} @ {args.baud}。输入 help 查看命令。")

    stop_evt = threading.Event()
    t = threading.Thread(target=reader_thread, args=(ser, stop_evt), daemon=True)
    t.start()

    try:
        while True:
            try:
                cmd = input(">> ")
            except EOFError:
                print()
                break
            out = translate(cmd)
            if out is None:
                continue
            ser.write(out.encode("ascii"))
    except SystemExit:
        pass
    finally:
        stop_evt.set()
        ser.close()
        print("[tune] bye")


if __name__ == "__main__":
    main()
