# nuedc_car — MSPM0G3507 自动行驶小车

基于 `LP-MSPM0G3507` 的电赛小车工程，当前代码已经补到赛题任务层，包含：

- 任务 `H1/H2/H3/H4` 路径状态机
- 直线段点到点控制
- 半圆弧 7 路灰度循迹
- 经过 `A/B/C/D` 与停车提示

## 最终引脚图

打板前请直接看这份独立文档：

- [MSPM0G3507_PINOUT.md](/home/xy/ti-workspace/projects/nuedc_car/MSPM0G3507_PINOUT.md)

里面包含：

- 带编码器电机 6P 接口
- TB6612 电机驱动接线
- 7 路数字灰度接线
- I2C、串口、蜂鸣器、LED、按键、超声波预留
- MSPM0G3507 数据手册第 6.2 节表 6-1 的已用引脚功能核对

## 当前环境结论

本机已经具备：

- `CCS 20.5`：`/home/xy/ti/ccs2050/ccs`
- `tiarmclang 4.0.4.LTS`
- `gmake`
- `SysConfig CLI`
- `cmake`
- `ninja`
- `arm-none-eabi-gcc / objcopy / size / gdb`
- `openocd`
- `arm-none-eabi-gdb`
- `loadti.sh`
- VS Code TI 扩展

当前仍缺：

- 完整 `MSPM0 SDK`
- 当前工程的 `ti_msp_dl_config.[ch]`
- `SEGGER J-Link Software Pack`（仅在你要用 `J-Link` 时需要）
- 带 `MSPM0` flash driver 的 `OpenOCD`（仅在你要用 `ST-LINK/OpenOCD` 时需要；系统自带 `openocd` 目前不支持 `mspm0` 烧录）
- 板卡连通与 Linux `udev` 权限

## VS Code 入口

项目内已新增：

- [.vscode/tasks.json](/home/xy/ti-workspace/projects/nuedc_car/.vscode/tasks.json)
- [.vscode/launch.json](/home/xy/ti-workspace/projects/nuedc_car/.vscode/launch.json)
- [.vscode/settings.json](/home/xy/ti-workspace/projects/nuedc_car/.vscode/settings.json)
- [tools/mspm0g3507.ccxml](/home/xy/ti-workspace/projects/nuedc_car/tools/mspm0g3507.ccxml)

可直接运行的任务：

- `Check TI Environment`
- `Build Firmware`
- `Flash Firmware`
- `Probe TI Board`
- `Serial Monitor 115200`
- `SDK Install Hint`
- `udev Rules Hint`
- `PID: Web Dashboard`

## Web 控制面板与 AI 调参

当前工程内置新版 Web PID 控制面板：

- 后端：[tools/pid/pid_web_server.py](/home/xy/ti-workspace/projects/nuedc_car/tools/pid/pid_web_server.py)
- 前端：[tools/pid/web/](/home/xy/ti-workspace/projects/nuedc_car/tools/pid/web)
- 启动脚本：[scripts/open_pid_dashboard.sh](/home/xy/ti-workspace/projects/nuedc_car/scripts/open_pid_dashboard.sh)

安装依赖：

```bash
./scripts/setup_pid_env.sh
```

USB 串口启动：

```bash
./scripts/open_pid_dashboard.sh /dev/ttyACM0
```

ESP32 Wi-Fi UART 透传启动：

```bash
./scripts/open_pid_dashboard.sh socket://192.168.4.1:3333
```

启动后默认访问：

```text
http://127.0.0.1:8765/
```

如果要让手机访问电脑上的面板：

```bash
HOST=0.0.0.0 ./scripts/open_pid_dashboard.sh socket://192.168.4.1:3333
```

然后手机打开电脑在同一网络下的地址，例如：

```text
http://192.168.4.2:8765/
```

面板支持：

- `L/R/LINE/ANG` 四路 PID 实时曲线
- PID 写入与读回：`$SET` / `$DUMP`
- 运行参数写入：`$CFGSET`
- 黑匣子导出：`$LOGDUMP`
- AI 调试页：
  - 本地规则调参
  - Claude Code CLI
  - Codex CLI
  - 自动闭环调参，点击开始后循环采样、分析、写入，直到连续稳定、达到最大轮数或手动停止

AI 调参有安全限制：单轮参数变化不超过当前值的 20%，参数不会写成负数，Claude/Codex 输出必须是 JSON；模型不可用、超时或输出不合规时会回退到本地规则建议。

控制面板也已单独拆到独立仓库：

```text
git@github.com:xy1092/diansai-control-ui.git
```

## ESP32 Wi-Fi UART Bridge

ESP32 透传固件位于：

- [platformio.ini](/home/xy/ti-workspace/projects/nuedc_car/platformio.ini)
- [tools/esp32_wifi_uart_bridge/esp32_wifi_uart_bridge.ino](/home/xy/ti-workspace/projects/nuedc_car/tools/esp32_wifi_uart_bridge/esp32_wifi_uart_bridge.ino)

编译：

```bash
platformio run -e esp32devkitv1_uart_bridge
```

默认 AP：

```text
SSID: NUEDC-CAR-UART
PASS: 12345678
TCP : 192.168.4.1:3333
```

Web 面板连接串填写：

```text
socket://192.168.4.1:3333
```

## 脚本说明

- [scripts/check_env.sh](/home/xy/ti-workspace/projects/nuedc_car/scripts/check_env.sh)
  统一检查 SDK、工具链、ccxml、生成文件和板卡连通
- [scripts/build.sh](/home/xy/ti-workspace/projects/nuedc_car/scripts/build.sh)
  统一构建入口；现已接入 `CMake + Ninja + arm-none-eabi-gcc`
- [scripts/flash.sh](/home/xy/ti-workspace/projects/nuedc_car/scripts/flash.sh)
  统一烧录入口；支持 `loadti | openocd | jlink`
- [scripts/flash_openocd.sh](/home/xy/ti-workspace/projects/nuedc_car/scripts/flash_openocd.sh)
  使用 `OpenOCD` 走 `ST-LINK` 或 `J-Link`
- [scripts/flash_jlink.sh](/home/xy/ti-workspace/projects/nuedc_car/scripts/flash_jlink.sh)
  使用 `JLinkExe` 直接烧录 `bin`
- [scripts/probe_board.sh](/home/xy/ti-workspace/projects/nuedc_car/scripts/probe_board.sh)
  检查 `/dev/ttyACM*`、`/dev/ttyUSB*` 和 `XDS110`
- [scripts/serial_monitor.sh](/home/xy/ti-workspace/projects/nuedc_car/scripts/serial_monitor.sh)
  打开串口监视器

## 浏览器下载与命令执行清单

这一节只回答两个问题：

- 哪些东西必须去浏览器下载
- 哪些事情只需要在终端执行命令

### 一、必须去浏览器下载的内容

#### 1. 所有路线都需要

- `MSPM0 SDK`
  这是必装项。没有它，工程无法补齐 `DriverLib / startup / linker / SysConfig` 相关资源，也就不能真正完成 `CMake` 构建。
  官方下载页：
  https://www.ti.com/tool/MSPM0-SDK
  版本说明 / 文档总览：
  https://software-dl.ti.com/msp430/esd/MSPM0-SDK/latest/docs/english/MSPM0_SDK_Documentation_Overview.html
  当前仓库说明基于的版本发布说明：
  https://software-dl.ti.com/msp430/esd/MSPM0-SDK/latest/release_notes_mspm0_sdk_2_10_00_04.html

#### 2. 只有走 J-Link 时才需要

- `SEGGER J-Link Software Pack`
  安装后至少要能找到：
  `JLinkExe`
  `JLinkGDBServerCLExe`
  官方下载页：
  https://www.segger.com/downloads/jlink
  MSPM0 + J-Link 官方说明：
  https://software-dl.ti.com/msp430/esd/MSPM0-SDK/2_10_00_04/docs/english/tools/doc_guide/doc_guide-srcs/segger_jlink.html

#### 3. 只有走 ST-LINK 时才可能需要

- 带 `MSPM0` flash driver 的 `OpenOCD`
  注意：本机虽然已经有 `/usr/bin/openocd`，但它当前不支持 `MSPM0` 烧录，所以如果坚持走 `ST-LINK`，还需要你另外安装一个支持 `mspm0` 的版本。
  OpenOCD 官方获取说明：
  https://openocd.org/pages/getting-openocd.html

### 二、不需要去浏览器下载、只需要执行命令的内容

#### 1. 环境检查

```bash
./scripts/check_env.sh
./scripts/probe_board.sh
```

#### 2. 安装 TI 的 Linux udev 规则

这一步不需要下载新软件，直接用你本机已经有的 CCS 脚本：

`bash
/home/xy/ti/ccs2050/ccs/install_scripts/ti_permissions_install.sh --install
sudo udevadm control --reload-rules
sudo udevadm trigger
```

#### 3. CMake 配置与编译

```bash
./scripts/build.sh --configure-only
./scripts/build.sh
```

或者：

```bash
cmake --preset gcc-debug
cmake --build --preset build
```

#### 4. 烧录命令

板载 `XDS110`：

```bash
FLASH_RUNNER=loadti ./scripts/flash.sh
```

`ST-LINK + OpenOCD`：

```bash
FLASH_RUNNER=openocd \
OPENOCD_INTERFACE_CFG=interface/stlink.cfg \
./scripts/flash.sh
```

`J-Link + OpenOCD`：

```bash
FLASH_RUNNER=openocd \
OPENOCD_INTERFACE_CFG=interface/jlink.cfg \
./scripts/flash.sh
```

`J-Link + JLinkExe`：

```bash
FLASH_RUNNER=jlink ./scripts/flash.sh
```

### 三、哪些不是下载软件，而是你还需要手动在工具里完成的事

- 在 `CCS / SysConfig` 中生成 `ti_msp_dl_config.c`
- 在 `CCS / SysConfig` 中生成 `ti_msp_dl_config.h`
- 如果 `CMake` 没自动找到启动文件和链接脚本，需要手动设置：
  `MSPM0_STARTUP_FILE`
  `MSPM0_LINKER_SCRIPT`

### 四、最短路径建议

如果你只是想先把工程尽快跑起来，建议按下面顺序做：

1. 去浏览器下载并安装 `MSPM0 SDK`
2. 在 `CCS / SysConfig` 中生成 `ti_msp_dl_config.c/.h`
3. 执行 `./scripts/check_env.sh`
4. 执行 `./scripts/build.sh`
5. 优先使用板载 `XDS110` 烧录

## 还差哪几步

### 1. 安装 MSPM0 SDK

官方页面：

- https://www.ti.com/tool/MSPM0-SDK
- 文档总览：
  https://software-dl.ti.com/msp430/esd/MSPM0-SDK/latest/docs/english/MSPM0_SDK_Documentation_Overview.html
- Release Notes：
  https://software-dl.ti.com/msp430/esd/MSPM0-SDK/latest/release_notes_mspm0_sdk_2_10_00_04.html

截至 `2026-04-23`，TI 官方页面显示 Linux 最新版本是：

- `MSPM0 SDK 2.10.00.04`
- 文件名：`mspm0_sdk_2_10_00_04.run`

建议安装到：

- `/home/xy/ti/mspm0_sdk_2_10_00_04`

安装完成后，应至少存在：

- `/home/xy/ti/mspm0_sdk_2_10_00_04/.metadata/product.json`
- `/home/xy/ti/mspm0_sdk_2_10_00_04/source/ti/driverlib/dl_common.h`

### 1.1 如果要用 J-Link，再额外安装什么

根据 MSPM0 SDK 2.10.00.04 的 Tools Guide，`MSPM0` 对 `SEGGER J-Link Software Pack v7.88i` 或更高版本有支持。

官方链接：

- SEGGER 下载页：
  https://www.segger.com/downloads/jlink
- TI 的 MSPM0 + J-Link 使用说明：
  https://software-dl.ti.com/msp430/esd/MSPM0-SDK/2_10_00_04/docs/english/tools/doc_guide/doc_guide-srcs/segger_jlink.html

安装完成后，至少要能找到：

- `JLinkExe`
- `JLinkGDBServerCLExe`

### 1.2 如果要用 ST-LINK，再额外安装什么

本机虽然已经有 `/usr/bin/openocd`，但 `./scripts/check_env.sh` 已验证当前二进制不包含 `mspm0` flash driver，因此现在**不能**直接拿它给 `MSPM0G3507` 烧录。

也就是说，`ST-LINK` 这条路你还需要补下面二选一：

- 安装一个带 `MSPM0` 支持的 `OpenOCD`
- 或者改走 `J-Link` / 板载 `XDS110`

OpenOCD 官方页面：

- https://openocd.org/pages/getting-openocd.html

### 2. 生成 SysConfig 文件

当前工程还没有：

- `ti_msp_dl_config.c`
- `ti_msp_dl_config.h`

因此还不能真正编译。你需要在 CCS 中为 `LP-MSPM0G3507` 新建或导入一个 `.syscfg`，至少补齐：

- `TIMA1` PWM x2
- `TIMG6/TIMG7` QEI
- `ADC0` 4 路灰度 + 电池采样
- `UART0/UART1`
- `I2C1`
- 电机方向 GPIO
- 蜂鸣器 / LED 提示 GPIO

目前这套 `CMake` 方案默认直接消费工程根目录下已有的：

- `ti_msp_dl_config.c`
- `ti_msp_dl_config.h`

也就是说，建议先在 `CCS/SysConfig` 里把这两个文件生成出来，再回到 `CMake` 路线。

### 3. 安装 Linux udev 规则

如果板子插上后仍看不到 `ttyACM` 或 XDS110，需要安装 TI 的权限脚本：

```bash
/home/xy/ti/ccs2050/ccs/install_scripts/ti_permissions_install.sh --install
sudo udevadm control --reload-rules
sudo udevadm trigger
```

然后重新插拔板子。

## 调试配置

[.vscode/launch.json](/home/xy/ti-workspace/projects/nuedc_car/.vscode/launch.json) 已按以下方式配置：

- 调试器：`cortex-debug`
- 接口：`XDS110`
- OpenOCD 脚本：`interface/xds110.cfg + board/ti_mspm0_launchpad.cfg`
- GDB：`/usr/bin/arm-none-eabi-gdb`
- 启动前任务：`Build Firmware`

这部分依赖 TI Embedded Debug 扩展自动安装的 OpenOCD 资源。如果扩展首次启动后提示安装依赖，直接安装即可。

## CMake 路线

项目现在已经补上：

- [CMakeLists.txt](/home/xy/ti-workspace/projects/nuedc_car/CMakeLists.txt)
- [CMakePresets.json](/home/xy/ti-workspace/projects/nuedc_car/CMakePresets.json)
- [cmake/toolchains/arm-none-eabi-gcc.cmake](/home/xy/ti-workspace/projects/nuedc_car/cmake/toolchains/arm-none-eabi-gcc.cmake)
- [tools/openocd/mspm0g3507.cfg](/home/xy/ti-workspace/projects/nuedc_car/tools/openocd/mspm0g3507.cfg)

默认构建器是：

- `arm-none-eabi-gcc`
- `cmake`
- `ninja`

默认输出物在：

- `build/nuedc_car.out`
- `build/nuedc_car.hex`
- `build/nuedc_car.bin`

### CMake 常用命令

先做环境检查：

```bash
./scripts/check_env.sh
```

只做 CMake 配置：

```bash
./scripts/build.sh --configure-only
```

配置并编译：

```bash
./scripts/build.sh
```

也可以直接使用原生命令：

```bash
cmake --preset gcc-debug
cmake --build --preset build
```

### 关于 startup / linker

`CMakeLists.txt` 会优先在 `MSPM0 SDK` 里自动搜索：

- `startup_*mspm0g350*`
- `*mspm0g3507*.ld / *.lds`

如果你的 SDK 布局和默认搜索不一致，就手动指定：

```bash
export MSPM0_STARTUP_FILE=/path/to/startup_mspm0g350x_*.c
export MSPM0_LINKER_SCRIPT=/path/to/mspm0g3507*.ld
./scripts/build.sh
```

## 烧录命令

### 1. 板载 XDS110

```bash
FLASH_RUNNER=loadti ./scripts/flash.sh
```

### 2. ST-LINK 走 OpenOCD

前提：你的 `openocd` 必须带 `mspm0` flash driver。

```bash
FLASH_RUNNER=openocd \
OPENOCD_INTERFACE_CFG=interface/stlink.cfg \
./scripts/flash.sh
```

### 3. J-Link 走 OpenOCD

前提：你的 `openocd` 必须带 `mspm0` flash driver。

```bash
FLASH_RUNNER=openocd \
OPENOCD_INTERFACE_CFG=interface/jlink.cfg \
./scripts/flash.sh
```

### 4. J-Link 走 JLinkExe

前提：系统已安装 `SEGGER J-Link Software Pack`。

```bash
FLASH_RUNNER=jlink ./scripts/flash.sh
```

## 现阶段能直接做什么

已经可以直接运行：

```bash
./scripts/check_env.sh
./scripts/probe_board.sh
```

在补齐 `MSPM0 SDK + ti_msp_dl_config.[ch] + startup/linker + 板卡连通` 之后，就能继续打通：

- 编译
- 烧录
- 断点调试
- 串口 PID 调参
