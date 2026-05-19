# PID 调参工具链

MCU 串口遥测 + PC 端实时采集 / 调参 / 离线分析。

## 一、串口协议

波特率 `115200 8N1`，走调试串口（XDS110 → `/dev/ttyACM0`）。

**MCU → PC（每 10 ms 一行）**
```
$P,<ts_ms>,<ch>,<sp>,<meas>,<out>,<err>,<integ>,<deriv>,<p>,<i>,<d>,<raw_out>
$L,<ts_ms>,<bias>,<contrast>,<strength>,<on_line>,<r0>,...,<r6>
```
`ch ∈ {L, R, LINE, ANG}`。
CSV 里会带 `kind` 列：`pid` 表示 PID 遥测，`line` 表示 7 路灰度原始采样。

**PC → MCU**
```
$SET,L,KP,2.50     改左轮速度环 Kp
$SET,line,KD,0.10  改循迹环 Kd  （通道名大小写不敏感）
$RATE,200          遥测 200 Hz
$RAWLINE,1         打开 7 路灰度原始流
$RST               所有积分归零
$PAUSE / $RESUME   停/续发送
$DUMP              让 MCU 回传当前增益
```

## 二、一次性准备

```bash
../../scripts/setup_pid_env.sh
```

默认会在项目根目录创建本地虚拟环境 `.venv-pid`。
在 VS Code 里也可以直接运行任务 `PID: Install Python deps`。

## Web Dashboard

推荐现场调参优先使用新版 Web 面板：

```bash
../../scripts/open_pid_dashboard.sh /dev/ttyACM0
```

ESP32 Wi-Fi UART 透传：

```bash
../../scripts/open_pid_dashboard.sh socket://192.168.4.1:3333
```

启动后打开：

```text
http://127.0.0.1:8765/
```

手机访问时用：

```bash
HOST=0.0.0.0 ../../scripts/open_pid_dashboard.sh socket://192.168.4.1:3333
```

Web 面板功能：

- 实时显示 `L/R/LINE/ANG` 四路 PID 曲线
- 写入/读回 PID 参数
- 调运行参数和黑匣子
- 死区扫描、死区前馈、段间 `theta snap`
- AI 调试页支持 `本地规则 / Claude Code / Codex`
- 自动闭环调参：点击开始后自动采样、分析、写入，直到稳定或手动停止

Claude/Codex 调参需要本机已安装并登录对应 CLI：

```bash
claude --help
codex --help
```

模型建议会先经过后端安全校验：单轮变化不超过 20%，参数非负，输出格式必须是 JSON；失败时自动回退本地规则。

## 三、典型工作流

1. **先只采集不调参**（实时图）

   ```bash
   python3 pid_monitor.py --port /dev/ttyACM0 --channels L R LINE
   ```
   关掉窗口即停止，数据已落在 `logs/run_YYYYMMDD_HHMMSS.csv`。

   若要把 7 路灰度原始值一起自动记录进 CSV：

   ```bash
   python3 pid_monitor.py --port /dev/ttyACM0 --line-raw --no-plot
   ```
   `pid_monitor.py` 会在启动时自动向 MCU 发送 `RATE / RAWLINE / RESUME / DUMP`。

   若想要一个更适合现场调试的可视化面板，可以直接开：

   ```bash
   python3 pid_dashboard.py --port /dev/ttyACM0 --line-raw
   ```
   它会显示：
   - `Setpoint / Measure` 曲线
   - `Output` 曲线
   - 指定通道的 `P / I / D` 三项曲线
   - 7 路灰度柱状图
   - 最新值面板和消息窗口

   面板上自带这些按键：
   - `Resume / Pause`
   - `Reset PID`
   - `Dump Gains`
   - `Apply Rate`
   - `Apply RawLine`
   - `Clear Curves`
   - `New Log File`

   在 VS Code 里也可以直接运行任务 `PID: Debug Dashboard`。

2. **一边跑车一边改增益**（分两个终端）

   ```bash
   # 终端 A
   python3 pid_monitor.py --port /dev/ttyACM0 --channels L R LINE

   # 终端 B
   python3 pid_tune.py --port /dev/ttyACM0
   >> set L kp 2.8
   >> set L ki 0.6
   >> rawline on
   >> rst
   ```
   > 注意：**同一串口不能被两个进程同时独占**。推荐做法是 `pid_monitor.py`
   > 专注读，`pid_tune.py` 只写；或者用 `socat` 做串口分叉。最省事：
   > 在 monitor 里 Ctrl-C 中断后再开 tune，交替做。

3. **事后对比 / 出图**

   在 VS Code 里打开 `pid_analyze.ipynb`，改掉 `CSV_PATH`，逐段运行。

## 四、socat 同端口双进程（Linux）

若想 **同时** 跑 monitor 和 tune，用 socat 把 `/dev/ttyACM0` 分叉成两条虚拟口：

```bash
socat -d -d pty,raw,echo=0,link=/tmp/ttyV0 pty,raw,echo=0,link=/tmp/ttyV1 &
# 然后把实际设备用自己写的小脚本 bridge 进 /tmp/ttyV0，
# monitor 和 tune 都连 /tmp/ttyV1。
```
这种用法不常用，一般"交替开"就够了。

## 五、和 VS Code 断点调试一起用

推荐现场流程：

1. 先运行任务 `PID: Debug Dashboard`
2. 再按 `F5`，选 `F5 Debug MSPM0G3507 (J-Link GDB Server)`
3. 在 VS Code 里下断点、单步、看变量
4. 在 Dashboard 里看实时曲线，并用按钮发 `$PAUSE / $RESUME / $RST / $DUMP`

说明：
- Dashboard 里的 `Pause / Resume` 控制的是 **遥测流**，方便你暂停界面刷新或继续观察。
- 真正的断点暂停、继续执行，还是通过 VS Code 调试器控制。

## 六、赛题对应的默认策略

当前车端默认值是按 H 题首跑保守值设置的：

- 循迹 bias 已归一化到 `[-1, 1]`
- 直线/弧线/姿态控制都限制为前进输出
- `H1/H2/H3/H4` 路由完整
- 段间 `theta snap` 默认开启
- 死区前馈默认关闭，先扫再填

## 七、典型调参 checklist

- [ ] **速度环 Kp**：给阶跃（固定 target_speed），观察 rise time；目标 < 200 ms
- [ ] **速度环 Ki**：等稳态误差出来后加，注意积分限幅别打满
- [ ] **循迹环 Kd**：只在有明显抖动时加，起步设 0
- [ ] **采样率**：建议内环 1 kHz，外环 200 Hz；遥测默认 100 Hz 够用

## 现场无电脑跑车 + 回来导出日志

固件现在带 RAM 黑匣子，默认每 100 ms 记录一帧关键数据：任务、状态、圈数、段号、位姿、左右轮目标/实测/输出、循迹偏差、灰度强度。断电会丢，但只要跑完不断电，回来插串口就能导出。

推荐流程：

1. 插线打开 Dashboard，点 `Clear Log`，确认黑匣子清空。
2. 点 `Enable Log`，然后可以断开串口，把车放到场地跑。
3. 跑完不要断电，重新插线连接 Dashboard。
4. 点 `Dump Log`，会生成 `*_blackbox.csv`。
5. 点 `Save AI Pack`，会生成 `*_ai_pack.json`，把这个 JSON 和 CSV 给 AI 分析最方便。

常用命令也可以手动发：

```text
$LOG,1      开启黑匣子
$LOGCLR     清空黑匣子
$LOGDUMP    导出黑匣子
```

实时串口遥测默认关闭，Dashboard 连接后会自动发送 `$RESUME`。这样现场不连电脑时，小车不会一直往串口刷日志。

## 预留无线 J-Link / RTT

`bsp_uart.c` 会把同一份遥测同时写入 SEGGER RTT 上行缓冲。以后有无线 J-Link 后，可以继续用：

```bash
python3 tools/pid/capture_rtt.py --duration 30
```

RTT 采集适合无线实时记录；串口黑匣子适合现在这种“先跑，回来插线导出”的方式。
