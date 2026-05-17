# PID 调参工具链

MCU 串口遥测 + PC 端网页仪表盘 + 离线分析。

## 一、串口协议

波特率 `115200 8N1`，走调试串口（XDS110 → `/dev/ttyACM0`）。

**MCU → PC**

```
$P,<ts_ms>,<ch>,<sp>,<meas>,<out>,<err>,<integ>,<deriv>,<p>,<i>,<d>,<raw_out>
$L,<ts_ms>,<bias>,<contrast>,<strength>,<on_line>,<r0>,...,<r6>
$A,<ts_ms>,<mission>,<state>,<loop>,<seg>,<x>,<y>,<theta_deg>,<seg_cm>,<mission_time_ms>
$G,<ch>,<kp>,<ki>,<kd>                     # 让 MCU 回传当前增益
$C,<param>,<value>,<min>,<max>             # 运行参数
$B,...                                     # RAM 黑匣子帧
```
`ch ∈ {L, R, LINE, ANG}`。

**PC → MCU**

```
$SET,L,KP,2.50                改左轮速度环 Kp
$SET,line,KD,0.10             改循迹环 Kd（通道大小写不敏感）
$CFGSET,spd_max,0.7           改运行参数
$RATE,200                     遥测 200 Hz
$RAWLINE,1                    打开 7 路灰度原始流
$RST                          所有积分归零
$PAUSE / $RESUME              停/续发送
$DUMP                         让 MCU 回传当前增益
$CFGDUMP                      让 MCU 回传所有运行参数
$LOG,1 / $LOG,0 / $LOGCLR / $LOGDUMP   黑匣子控制
```

## 二、一次性准备

```bash
../../scripts/setup_pid_env.sh
```

会在项目根创建 `.venv-pid` 并装好 `pyserial / fastapi / uvicorn / matplotlib / pandas / prompt_toolkit`。
VS Code 里运行任务 `PID: Install Python deps` 也一样。

## 三、推荐工作流：网页仪表盘

```bash
# USB 直连
./scripts/open_pid_dashboard.sh /dev/ttyACM0

# ESP32 透传（Wi-Fi）—— 端口写 socket:// 即可
./scripts/open_pid_dashboard.sh socket://192.168.1.20:3333
```

启动后浏览器自动打开 `http://127.0.0.1:8765`，看到的内容：

| 区域 | 内容 |
|---|---|
| 顶部状态栏 | 左轮 meas / 右轮 meas / 循迹 bias / heading / 任务+状态 / 运行时间 |
| 概览页 | 四个 PID 模块卡片：左轮 / 右轮 / 循迹 / 角度 |
| 每个 PID 卡片 | 圆环仪表盘（实时 meas，sp 指针）+ sp/meas 曲线 + KP/KI/KD 调参 |
| 循迹卡片 | bias 指针仪表 + 7 路灰度柱状图 |
| 运行参数页 | 自动从 `$CFGDUMP` 拉取，可直接改写 `$CFGSET` |
| 黑匣子页 | `开启 / 停止 / 清空 / 导出` 按钮 + 自动落 CSV |
| 控制台页 | 原始消息 + 自定义命令 |

后端独占串口，前端通过 WebSocket 推送；同一个浏览器多开标签、或在手机上访问 `http://<电脑IP>:8765` 都可以。

### 直接调用 Python（不通过 shell 脚本）

```bash
python3 tools/pid/pid_web_server.py --port /dev/ttyACM0 --open --autoconnect
python3 tools/pid/pid_web_server.py --port socket://192.168.1.20:3333 --host 0.0.0.0 --web-port 8765
```

## 四、Wi-Fi 透传适配

`tools/esp32_wifi_uart_bridge/esp32_wifi_uart_bridge.ino` 把 ESP32 配成 TCP 透传：

- ESP32 加入 Wi-Fi 后，在 `TCP_PORT = 3333` 上等连接
- 把 ESP32 的 UART2 接到 MCU 的调试串口 TX/RX（共地）
- 在仪表盘的"端口"栏写 `socket://<esp32-ip>:3333`，点连接即可

后端用 `serial.serial_for_url()`，所以 `socket://` / `rfc2217://` / `/dev/ttyACM0` 都走同一套代码。

## 五、命令行 REPL（备用）

```bash
python3 tools/pid/pid_tune.py --port /dev/ttyACM0
>> set L kp 2.8
>> set L ki 0.6
>> rawline on
>> rst
```

> 同一物理串口不能同时被网页仪表盘和 REPL 独占。要并行使用，请用 ESP32 透传或 socat 把串口分叉。

## 六、事后离线分析

`tools/pid/logs/` 里会自动落 CSV：

- `dashboard_YYYYMMDD_HHMMSS.csv`：实时遥测全量
- `dashboard_YYYYMMDD_HHMMSS_blackbox.csv`：每次 `$LOGDUMP` 后的 RAM 黑匣子

打开 `pid_analyze.ipynb`，改掉 `CSV_PATH`，逐段运行画图。

## 七、典型调参 checklist

- [ ] **速度环 Kp**：给阶跃（固定 target_speed），观察 rise time；目标 < 200 ms
- [ ] **速度环 Ki**：等稳态误差出来后加，注意积分限幅别打满
- [ ] **循迹环 Kd**：只在有明显抖动时加，起步设 0
- [ ] **采样率**：建议内环 1 kHz，外环 200 Hz；遥测默认 100 Hz 够用

## 八、现场无电脑跑车 + 回来导出黑匣子

固件带 RAM 黑匣子，默认每 100 ms 记录一帧。断电会丢，只要跑完不断电就能导出。

1. 插线打开仪表盘，"黑匣子页"点 `清空`
2. 点 `开启 LOG`，断开串口，把车放到场地跑
3. 跑完不要断电，重新插线连接仪表盘
4. 点 `导出` → 等 `$BEND` → 点 `保存 CSV`
