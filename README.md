# nuedc_car

MSPM0G3507 电赛小车工程，包含车端固件、ESP32 Wi-Fi UART 透传固件、串口/无线 Web 控制面板和 PID 调参工具链。

## 当前功能

- MSPM0G3507 车端固件
  - H1/H2/H3/H4 任务状态机
  - 直线段点到点控制
  - 半圆弧 7 路灰度循迹
  - 左右轮速度 PID、循迹 PID、角度遥测通道
  - 串口遥测、参数写入、黑匣子导出
- Web PID 控制面板
  - USB 串口或 ESP32 Wi-Fi 透传连接
  - `L/R/LINE/ANG` 四路实时曲线
  - PID 写入/读回、运行参数调整、黑匣子 CSV 导出
  - 本地规则 / Claude Code / Codex 辅助调参
  - 自动闭环调参
- ESP32 Wi-Fi UART Bridge
  - AP 模式
  - TCP `3333` 透传到 UART2

## 目录

```text
app/                         任务状态机和运动控制
bsp/                         板级驱动
middleware/                  PID、滤波、遥测协议
scripts/                     构建、烧录、调试、面板启动脚本
tools/esp32_wifi_uart_bridge ESP32 透传固件
tools/pid/                   PID 工具链和 Web 面板
platformio.ini               ESP32 PlatformIO 配置
MSPM0G3507_PINOUT.md         引脚表
```

## 环境准备

需要安装：

- TI MSPM0 SDK
- CCS 或 SysConfig CLI
- CMake
- Ninja
- `arm-none-eabi-gcc`
- 可选：SEGGER J-Link 工具
- 可选：PlatformIO，用于编译 ESP32 透传固件

推荐用环境变量指定本机路径：

```bash
export CCS_DIR="$HOME/ti/ccs2050/ccs"
export MSPM0_SDK_ROOT="$HOME/ti/mspm0_sdk_2_10_00_04"
export JLINK_EXE="/opt/SEGGER/JLink/JLinkExe"
export JLINK_GDB_SERVER="/opt/SEGGER/JLink/JLinkGDBServerCLExe"
```

检查环境：

```bash
./scripts/check_env.sh
./scripts/probe_board.sh
```

Linux 下如果板卡权限不足，安装 TI udev 规则：

```bash
$CCS_DIR/install_scripts/ti_permissions_install.sh --install
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## 构建车端固件

```bash
./scripts/build.sh
```

输出：

```text
build/nuedc_car.out
build/nuedc_car.bin
build/nuedc_car.hex
```

如果只想重新配置 CMake：

```bash
./scripts/build.sh --configure-only
```

## 烧录

统一入口：

```bash
./scripts/flash.sh
```

可用 runner：

```bash
FLASH_RUNNER=loadti ./scripts/flash.sh
FLASH_RUNNER=jlink ./scripts/flash.sh
FLASH_RUNNER=openocd OPENOCD_INTERFACE_CFG=interface/jlink.cfg ./scripts/flash.sh
FLASH_RUNNER=openocd OPENOCD_INTERFACE_CFG=interface/stlink.cfg ./scripts/flash.sh
```

## VS Code

常用任务：

- `Build Firmware`
- `Flash Firmware`
- `Probe TI Board`
- `Serial Monitor 115200`
- `PID: Install Python deps`
- `PID: Web Dashboard`

J-Link 调试配置在 `.vscode/launch.json`。J-Link 序列号默认留空，需要时在启动调试时输入。

## Web 控制面板

先安装 Python 依赖：

```bash
./scripts/setup_pid_env.sh
```

推荐用短口令启动：

```bash
./wake up diansai web 0   # 有线 USB，默认 /dev/ttyACM0
./wake up diansai web 1   # ESP32 Wi-Fi 透传，只在电脑本机打开
./wake up diansai web 2   # 手机模式，监听 0.0.0.0:8765
```

也可以直接用脚本：

```bash
./scripts/start.sh 0 [/dev/ttyACM0]
./scripts/start.sh 1
./scripts/start.sh 2
```

底层入口仍然保留：

```bash
./scripts/open_pid_dashboard.sh /dev/ttyACM0
./scripts/open_pid_dashboard.sh socket://192.168.4.1:3333
```

默认电脑访问：

```text
http://127.0.0.1:8765/
```

手机模式下，手机和电脑都连接 `NUEDC-CAR-UART` 后，用手机打开电脑在热点里的地址，例如：

```text
http://<电脑在热点中的IP>:8765/
```

如果手机打不开，优先确认手机 Wi-Fi 代理/VPN/私有 DNS 已关闭，并确认电脑防火墙允许 `wlan0` 入站访问 `8765/tcp`。

## AI 调参

Web 面板的 `AI 调试` 页支持：

- `本地规则`
- `Claude Code`
- `Codex`

Claude/Codex 模式需要本机已安装并登录对应 CLI：

```bash
claude --help
codex --help
```

调参流程：

1. 连接小车
2. 让小车运行并产生遥测
3. 选择通道和 AI 引擎
4. 点击 `分析建议` 查看建议
5. 点击 `应用建议` 写入
6. 或点击 `开始自动` 进入自动闭环调参

安全策略：

- 单轮 PID 变化不超过当前值的 20%
- PID 参数不允许为负
- Claude/Codex 必须返回 JSON
- 模型超时、不可用或输出不合规时自动回退本地规则
- 自动调参可随时手动停止

控制面板也有独立仓库：

```text
git@github.com:<your-github-user>/diansai-control-ui.git
```

## ESP32 Wi-Fi UART Bridge

ESP32 固件位于：

```text
tools/esp32_wifi_uart_bridge/esp32_wifi_uart_bridge.ino
platformio.ini
```

编译：

```bash
platformio run -e esp32devkitv1_uart_bridge
```

上传：

```bash
platformio run -e esp32devkitv1_uart_bridge -t upload
```

默认连接信息：

```text
SSID: NUEDC-CAR-UART
PASS: change-me-1234
TCP : 192.168.4.1:3333
```

可以在 `platformio.ini` 里通过 `build_flags` 覆盖 AP 名称和密码：

```ini
build_flags =
  -D NUEDC_AP_SSID=\"NUEDC-CAR-UART\"
  -D NUEDC_AP_PASS=\"your-password\"
```

## 串口协议摘要

MCU 到 PC：

```text
$P,<ts_ms>,<ch>,<sp>,<meas>,<out>,<err>,<integ>,<deriv>,<p>,<i>,<d>,<raw_out>
$L,<ts_ms>,<bias>,<contrast>,<strength>,<on_line>,<r0>,...,<r6>
$A,<ts_ms>,<mission>,<state>,<loop>,<seg>,<x>,<y>,<theta_deg>,<seg_cm>,<mission_time_ms>
$G,<ch>,<kp>,<ki>,<kd>
$C,<name>,<value>,<min>,<max>
```

PC 到 MCU：

```text
$SET,LINE,KP,82
$SET,LINE,KD,19.5
$CFGSET,spd_max,0.7
$RATE,100
$RAWLINE,1
$LOG,1
$LOGCLR
$LOGDUMP
$RST
$PAUSE
$RESUME
$DUMP
```

## 黑匣子

固件支持 RAM 黑匣子。典型流程：

1. Web 面板连接后点 `Clear Log`
2. 点 `Enable Log`
3. 小车运行
4. 跑完不断电，重新连接面板
5. 点 `Dump Log`
6. 保存 CSV

## 备注

- `build/`、`.pio/`、`.venv-pid/`、日志文件不会提交。
- `.vscode/c_cpp_properties.json` 是 PlatformIO/VS Code 本机生成文件，不建议提交。
- 本仓库不保存 API key、Claude/Codex token 或本机私钥。
