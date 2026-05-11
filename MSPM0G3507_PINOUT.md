# MSPM0G3507 自动行驶小车最终引脚图

适用对象：地猛星 MSPM0G3507 开发板、带 AB 相编码器直流减速电机、TB6612 电机驱动、7 路数字灰度传感器。

数据手册来源：`/home/xy/下载/mspm0g3507数据手册.pdf`，第 6.2 节表 6-1 `引脚属性`，封装按 `48 LQFP / PT` 核对。

## 0. PCB 交接版完整引脚对照表

这张表给 PCB 外设组直接使用。所有进入 MSPM0G3507 的信号默认按 3.3V 电平设计；5V 外设输出进 MCU 前必须确认是否已经降到 3.3V。

### 0.1 MCU 引脚到外设总表

| 模块 | PCB 信号名 | MSPM0G3507 引脚 | 方向 | 外设连接 | 电平/备注 |
| --- | --- | --- | --- | --- | --- |
| 左电机 PWM | `PWMA` | `PA8` | MCU -> 外设 | TB6612 `PWMA` | 3.3V PWM，`TIMA0_C0` |
| 左电机方向 1 | `AIN1` | `PB2` | MCU -> 外设 | TB6612 `AIN1` | 3.3V GPIO |
| 左电机方向 2 | `AIN2` | `PB3` | MCU -> 外设 | TB6612 `AIN2` | 3.3V GPIO |
| 右电机 PWM | `PWMB` | `PA9` | MCU -> 外设 | TB6612 `PWMB` | 3.3V PWM，`TIMA0_C1` |
| 右电机方向 1 | `BIN1` | `PB8` | MCU -> 外设 | TB6612 `BIN1` | 3.3V GPIO |
| 右电机方向 2 | `BIN2` | `PB9` | MCU -> 外设 | TB6612 `BIN2` | 3.3V GPIO |
| 电机驱动使能 | `STBY` | `PB6` | MCU -> 外设 | TB6612 `STBY` | 3.3V GPIO，高电平使能；建议 10k 下拉到 GND |
| 左编码器 A | `LM_ENC_A` | `PA0` | 外设 -> MCU | 左电机编码器 A | GPIO 输入；优先 3.3V |
| 左编码器 B | `LM_ENC_B` | `PA1` | 外设 -> MCU | 左电机编码器 B | GPIO 输入；优先 3.3V |
| 右编码器 A | `RM_ENC_A` | `PB20` | 外设 -> MCU | 右电机编码器 A | GPIO 输入；必须 3.3V 兼容 |
| 右编码器 B | `RM_ENC_B` | `PB24` | 外设 -> MCU | 右电机编码器 B | GPIO 输入；必须 3.3V 兼容 |
| 数字灰度 0 | `CCD0` | `PA27` | 外设 -> MCU | 灰度 D0 | GPIO 输入；必须 3.3V 兼容 |
| 数字灰度 1 | `CCD1` | `PA26` | 外设 -> MCU | 灰度 D1 | GPIO 输入；必须 3.3V 兼容 |
| 数字灰度 2 | `CCD2` | `PA25` | 外设 -> MCU | 灰度 D2 | GPIO 输入；必须 3.3V 兼容 |
| 数字灰度 3 | `CCD3` | `PA24` | 外设 -> MCU | 灰度 D3 | GPIO 输入；必须 3.3V 兼容 |
| 数字灰度 4 | `CCD4` | `PA7` | 外设 -> MCU | 灰度 D4 | GPIO 输入；必须 3.3V 兼容 |
| 数字灰度 5 | `CCD5` | `PB18` | 外设 -> MCU | 灰度 D5 | GPIO 输入；必须 3.3V 兼容 |
| 数字灰度 6 | `CCD6` | `PB19` | 外设 -> MCU | 灰度 D6 | GPIO 输入；必须 3.3V 兼容 |
| I2C 数据 | `I2C1_SDA` | `PA16` | 双向 | MPU6050/OLED SDA | 上拉到 3.3V，不能上拉到 5V |
| I2C 时钟 | `I2C1_SCL` | `PA17` | MCU -> 外设 | MPU6050/OLED SCL | 上拉到 3.3V，不能上拉到 5V |
| MPU6050 中断 | `MPU_INT` | `PA15` | 外设 -> MCU | MPU6050 INT | GPIO 输入，3.3V |
| 蜂鸣器 | `BUZZ` | `PB7` | MCU -> 外设 | 蜂鸣器驱动管 | 用三极管/MOS 驱动，不建议 MCU 直推 |
| 板载/外接 LED | `LED` | `PA14` | MCU -> 外设 | LED | GPIO 输出，串限流电阻 |
| 启动键 | `START` | `PA18` | 外设 -> MCU | 按键到 GND | GPIO 输入，内部/外部上拉到 3.3V |
| 任务选择 0 | `MODE0` | `PA21` | 外设 -> MCU | 拨码到 GND | GPIO 输入，上拉到 3.3V |
| 任务选择 1 | `MODE1` | `PA22` | 外设 -> MCU | 拨码到 GND | GPIO 输入，上拉到 3.3V |
| 停止/备用键 | `STOP` | `PA28` | 外设 -> MCU | 按键到 GND | GPIO 输入，上拉到 3.3V |
| 调试串口 TX | `UART0_TX` | `PA10` | MCU -> CH340 | CH340 `RXD` | 板载串口，交叉连接 |
| 调试串口 RX | `UART0_RX` | `PA11` | CH340 -> MCU | CH340 `TXD` | 板载串口，交叉连接 |
| 超声波触发 | `US_TRIG` | `PA12` | MCU -> 外设 | HC-SR04 `TRIG` | 3.3V 输出通常可触发 |
| 超声波回波 | `US_ECHO` | `PA13` | 外设 -> MCU | HC-SR04 `ECHO` | 5V ECHO 必须分压/电平转换 |

### 0.2 建议外设连接器

| 连接器 | 引脚/丝印 | 连接目标 | 备注 |
| --- | --- | --- | --- |
| 左电机 6P | `LM_M+` | TB6612 `AOUT1` | 电机动力线 |
| 左电机 6P | `LM_M-` | TB6612 `AOUT2` | 电机动力线 |
| 左电机 6P | `LM_ENC_VCC` | 3.3V | 编码器电源；若必须 5V，A/B 要确认电平 |
| 左电机 6P | `LM_ENC_GND` | GND | 共地 |
| 左电机 6P | `LM_ENC_A` | `PA0` | 编码器 A |
| 左电机 6P | `LM_ENC_B` | `PA1` | 编码器 B |
| 右电机 6P | `RM_M+` | TB6612 `BOUT1` | 电机动力线 |
| 右电机 6P | `RM_M-` | TB6612 `BOUT2` | 电机动力线 |
| 右电机 6P | `RM_ENC_VCC` | 3.3V | 编码器电源；若必须 5V，A/B 要确认电平 |
| 右电机 6P | `RM_ENC_GND` | GND | 共地 |
| 右电机 6P | `RM_ENC_A` | `PB20` | 编码器 A |
| 右电机 6P | `RM_ENC_B` | `PB24` | 编码器 B |
| 灰度 9P | `3V3` | 3.3V | 推荐灰度模块 3.3V 供电 |
| 灰度 9P | `GND` | GND | 共地 |
| 灰度 9P | `D0` | `PA27` | 数字灰度 |
| 灰度 9P | `D1` | `PA26` | 数字灰度 |
| 灰度 9P | `D2` | `PA25` | 数字灰度 |
| 灰度 9P | `D3` | `PA24` | 数字灰度 |
| 灰度 9P | `D4` | `PA7` | 数字灰度 |
| 灰度 9P | `D5` | `PB18` | 数字灰度 |
| 灰度 9P | `D6` | `PB19` | 数字灰度 |
| I2C 4P | `3V3` | 3.3V | MPU6050/OLED |
| I2C 4P | `GND` | GND | 共地 |
| I2C 4P | `SDA` | `PA16` | 上拉到 3.3V |
| I2C 4P | `SCL` | `PA17` | 上拉到 3.3V |
| 按键/拨码 | `START` | `PA18` | 按下接 GND |
| 按键/拨码 | `MODE0` | `PA21` | 拨到 ON 接 GND |
| 按键/拨码 | `MODE1` | `PA22` | 拨到 ON 接 GND |
| 按键/拨码 | `STOP` | `PA28` | 可选 |
| 超声波 4P | `5V` | 5V | HC-SR04 电源 |
| 超声波 4P | `GND` | GND | 共地 |
| 超声波 4P | `TRIG` | `PA12` | MCU 输出 |
| 超声波 4P | `ECHO` | `PA13` | 必须降到 3.3V |

### 0.3 电源与保护要求

| 网络 | 去向 | PCB 要求 |
| --- | --- | --- |
| `VBAT` | 电池输入 | 按电池接口和电机电流加粗走线 |
| `VMOTOR` | TB6612 `VM` | 接电机电源，靠近 TB6612 放大电容 |
| `5V` | 可选 5V 模块 | 由降压模块输出；不要进 MSPM0 GPIO |
| `3V3` | MSPM0、灰度、编码器、I2C、TB6612 `VCC` | MCU 逻辑电源；传感器优先使用 3.3V |
| `GND` | 所有模块 | 电池、降压、MSPM0、TB6612、传感器必须共地 |
| `US_ECHO` | `PA13` | 5V 回波加分压或电平转换 |
| 灰度 `D0-D6` | MCU GPIO | 若灰度模块只能 5V 输出，每路加电平转换 |
| 编码器 `A/B` | MCU GPIO | 若编码器只能 5V 输出，加电平转换；`PA0/PA1` 虽 5V 容限，也建议按 3.3V 设计 |

## 1. 打板用完整接口表

### 1.1 MCU 到 TB6612

| 功能 | 信号名 | MSPM0G3507 引脚 | 连接到 | 说明 |
| --- | --- | --- | --- | --- |
| 左电机 PWM | `PWMA` | `PA8` | TB6612 `PWMA` | 硬件 PWM，`TIMA0_C0` |
| 左电机方向 1 | `AIN1` | `PB2` | TB6612 `AIN1` | GPIO 输出 |
| 左电机方向 2 | `AIN2` | `PB3` | TB6612 `AIN2` | GPIO 输出 |
| 右电机 PWM | `PWMB` | `PA9` | TB6612 `PWMB` | 硬件 PWM，`TIMA0_C1` |
| 右电机方向 1 | `BIN1` | `PB8` | TB6612 `BIN1` | GPIO 输出 |
| 右电机方向 2 | `BIN2` | `PB9` | TB6612 `BIN2` | GPIO 输出 |
| 电机驱动使能 | `STBY` | `PB6` | TB6612 `STBY` | 高电平使能 |

### 1.2 带编码器电机接口

电机动力线不接 MCU，必须接 TB6612 输出；编码器 A/B 才接 MSPM0 GPIO。

| 左电机 6P 接口丝印 | 连接 |
| --- | --- |
| `LM_M+` | TB6612 `AOUT1` |
| `LM_M-` | TB6612 `AOUT2` |
| `LM_ENC_VCC` | 3.3V，除非你的编码器明确要求 5V |
| `LM_ENC_GND` | GND |
| `LM_ENC_A` | `PA0` |
| `LM_ENC_B` | `PA1` |

| 右电机 6P 接口丝印 | 连接 |
| --- | --- |
| `RM_M+` | TB6612 `BOUT1` |
| `RM_M-` | TB6612 `BOUT2` |
| `RM_ENC_VCC` | 3.3V，除非你的编码器明确要求 5V |
| `RM_ENC_GND` | GND |
| `RM_ENC_A` | `PB20` |
| `RM_ENC_B` | `PB24` |

如果编码器必须 5V 供电，而且 A/B 输出也是 5V，进 `PA0/PA1/PB20/PB24` 前必须加电平转换或电阻分压。

说明：`PA0/PA1` 在数据手册里标为 `5V 容限开漏 IO`，这不表示它们不能用。它的重点限制是：不推荐拿 `PA0/PA1` 当普通推挽输出去直接驱动外设；本方案只把它们作为编码器 A/B 输入使用，所以是合适的。若你的编码器 A/B 是 5V 输出，`PA0/PA1` 相比普通 IO 更安全，但最终仍建议实测编码器输出电平，并优先把编码器供电改成 3.3V。

### 1.3 传感器和提示模块

| 模块 | 信号名 | MSPM0G3507 引脚 | 连接到 | 说明 |
| --- | --- | --- | --- | --- |
| 数字灰度 0 | `CCD0` | `PA27` | 灰度 D0 | GPIO 输入 |
| 数字灰度 1 | `CCD1` | `PA26` | 灰度 D1 | GPIO 输入 |
| 数字灰度 2 | `CCD2` | `PA25` | 灰度 D2 | GPIO 输入 |
| 数字灰度 3 | `CCD3` | `PA24` | 灰度 D3 | GPIO 输入 |
| 数字灰度 4 | `CCD4` | `PA7` | 灰度 D4 | GPIO 输入 |
| 数字灰度 5 | `CCD5` | `PB18` | 灰度 D5 | GPIO 输入 |
| 数字灰度 6 | `CCD6` | `PB19` | 灰度 D6 | GPIO 输入 |
| I2C 数据 | `SDA` | `PA16` | MPU6050/OLED SDA | `I2C1_SDA` |
| I2C 时钟 | `SCL` | `PA17` | MPU6050/OLED SCL | `I2C1_SCL` |
| MPU6050 中断 | `MPU_INT` | `PA15` | MPU6050 INT | GPIO 输入 |
| 蜂鸣器 | `BUZZ` | `PB7` | 蜂鸣器驱动 | GPIO 输出 |
| 板载 LED | `LED` | `PA14` | LED | GPIO 输出 |
| 启动按键 | `START` | `PA18` | 按键 | GPIO 输入 |
| 调试串口 TX | `UART0_TX` | `PA10` | CH340 RX | 板载调试串口 |
| 调试串口 RX | `UART0_RX` | `PA11` | CH340 TX | 板载调试串口 |
| 超声波触发预留 | `TRIG` | `PA12` | HC-SR04 TRIG | GPIO 输出 |
| 超声波回波预留 | `ECHO` | `PA13` | HC-SR04 ECHO | 必须降到 3.3V |

### 1.4 按键/拨码建议

只保留一个 `START` 按键可以跑固定任务，但比赛现场要选择 `H1/H2/H3/H4` 会不方便。推荐增加 2 位拨码开关做任务选择，再保留一个启动键。

| 功能 | 信号名 | MSPM0G3507 引脚 | 连接方式 | 说明 |
| --- | --- | --- | --- | --- |
| 启动键 | `START` | `PA18` | 按键到 GND，GPIO 上拉 | 开始/确认 |
| 任务选择 bit0 | `MODE0` | `PA21` | 拨码到 GND，GPIO 上拉 | `00=H1, 01=H2, 10=H3, 11=H4` |
| 任务选择 bit1 | `MODE1` | `PA22` | 拨码到 GND，GPIO 上拉 | 和 `MODE0` 组成 2 位任务号 |
| 停止/备用键 | `STOP` | `PA28` | 按键到 GND，GPIO 上拉 | 可选，调试和急停逻辑用 |

如果你不想加拨码，也可以只加一个 `MODE` 键，用短按循环 `H1/H2/H3/H4`、`START` 确认；但拨码更直观，断电后任务选择也不会丢。

## 2. 数据手册引脚功能核对表

| 当前用途 | 引脚 | 48-LQFP/PT 物理脚号 | PINCMx | 数据手册模拟功能 | 数据手册数字复用功能 | IO 结构/备注 |
| --- | --- | ---: | ---: | --- | --- | --- |
| 左编码器 A | `PA0` | 1 | 1 | - | `UART0_TX` / `I2C0_SDA` / `TIMA0_C0` / `TIMA_FAL1` / `TIMG8_C1` / `FCC_IN` | 5V 容限开漏 IO；本方案只作输入 |
| 左编码器 B | `PA1` | 2 | 2 | - | `UART0_RX` / `I2C0_SCL` / `TIMA0_C1` / `TIMA_FAL2` / `TIMG8_IDX` / `TIMG8_C0` | 5V 容限开漏 IO；本方案只作输入 |
| 灰度 CCD4 | `PA7` | 13 | 14 | - | `COMP0_OUT` / `CLK_OUT` / `TIMG8_C0` / `TIMA0_C2` / `TIMG8_IDX` / `TIMG7_C1` / `TIMA0_C1` | 标准 IO |
| 左电机方向 `AIN1` | `PB2` | 14 | 15 | - | `UART3_TX` / `UART2_CTS` / `I2C1_SCL` / `TIMA0_C3` / `UART1_CTS` / `TIMG6_C0` / `TIMA1_C0` | 标准 IO |
| 左电机方向 `AIN2` | `PB3` | 15 | 16 | - | `UART3_RX` / `UART2_RTS` / `I2C1_SDA` / `TIMA0_C3N` / `UART1_RTS` / `TIMG6_C1` / `TIMA1_C1` | 标准 IO |
| 左电机 PWM `PWMA` | `PA8` | 16 | 19 | - | `UART1_TX` / `SPI0_CS0` / `UART0_RTS` / `TIMA0_C0` / `TIMA1_C0N` | 标准 IO；当前使用 `TIMA0_C0` |
| 右电机 PWM `PWMB` | `PA9` | 17 | 20 | - | `UART1_RX` / `SPI0_PICO` / `UART0_CTS` / `TIMA0_C1` / `RTC_OUT` / `TIMA0_C0N` / `TIMA1_C1N` / `CLK_OUT` | 高速 IO；当前使用 `TIMA0_C1` |
| 调试串口 TX | `PA10` | 18 | 21 | - | `UART0_TX` / `SPI0_POCI` / `I2C0_SDA` / `TIMA1_C0` / `TIMG12_C0` / `TIMA0_C2` / `I2C1_SDA` / `CLK_OUT` | 高驱动 IO；当前使用 `UART0_TX` |
| 调试串口 RX | `PA11` | 19 | 22 | - | `UART0_RX` / `SPI0_SCK` / `I2C0_SCL` / `TIMA1_C1` / `COMP0_OUT` / `TIMA0_C2N` / `I2C1_SCL` | 高驱动 IO；当前使用 `UART0_RX` |
| TB6612 `STBY` | `PB6` | 20 | 23 | - | `UART1_TX` / `SPI1_CS0` / `SPI0_CS1` / `TIMG8_C0` / `UART2_CTS` / `TIMG6_C0` / `TIMA1_C0N` | 标准 IO |
| 蜂鸣器 | `PB7` | 21 | 24 | - | `UART1_RX` / `SPI1_POCI` / `SPI0_CS2` / `TIMG8_C1` / `UART2_RTS` / `TIMG6_C1` / `TIMA1_C1N` | 标准 IO |
| 右电机方向 `BIN1` | `PB8` | 22 | 25 | - | `UART1_CTS` / `SPI1_PICO` / `TIMA0_C0` / `COMP1_OUT` | 标准 IO |
| 右电机方向 `BIN2` | `PB9` | 23 | 26 | - | `UART1_RTS` / `SPI1_SCK` / `TIMA0_C1` / `TIMA0_C0N` | 标准 IO |
| 超声波 TRIG 预留 | `PA12` | 27 | 34 | - | `UART3_CTS` / `SPI0_SCK` / `TIMG0_C0` / `CAN_TX` / `TIMA0_C3` / `FCC_IN` | 高速 IO |
| 超声波 ECHO 预留 | `PA13` | 28 | 35 | `COMP0_IN2-` | `UART3_RTS` / `SPI0_POCI` / `UART3_RX` / `TIMG0_C1` / `CAN_RX` / `TIMA0_C3N` | 高速 IO；5V ECHO 必须降压 |
| 板载 LED | `PA14` | 29 | 36 | `COMP0_IN2+` / `A0_12` | `UART0_CTS` / `SPI0_PICO` / `UART3_TX` / `TIMG12_C0` / `CLK_OUT` | 高速 IO |
| MPU6050 INT | `PA15` | 30 | 37 | `A1_0` / `DAC_OUT` / `OPA0_IN2+` / `OPA1_IN2+` / `COMP0_IN3+` / `COMP1_IN3+` | `UART0_RTS` / `SPI1_CS2` / `I2C1_SCL` / `TIMA1_C0` / `TIMG8_IDX` / `TIMA1_C0N` / `TIMA0_C2` | 标准 IO |
| I2C SDA | `PA16` | 31 | 38 | `A1_1` / `OPA1_OUT` | `COMP2_OUT` / `SPI1_POCI` / `I2C1_SDA` / `TIMA1_C1` / `TIMA1_C1N` / `TIMA0_C2N` / `FCC_IN` | 标准 IO；当前使用 `I2C1_SDA` |
| I2C SCL | `PA17` | 32 | 39 | `A1_2` / `OPA1_IN1-` / `COMP0_IN1-` | `UART1_TX` / `SPI1_SCK` / `I2C1_SCL` / `TIMA0_C3` / `TIMG7_C0` / `TIMA1_C0` | 带唤醒标准 IO；当前使用 `I2C1_SCL` |
| 启动按键 | `PA18` | 33 | 40 | `A1_3` / `OPA1_IN1+` / `COMP0_IN1+` / `GPAMP_IN-` | `UART1_RX` / `SPI1_PICO` / `I2C1_SDA` / `TIMA0_C3N` / `TIMG7_C1` / `TIMA1_C1` | 带唤醒标准 IO |
| 任务选择 bit0 预留 | `PA21` | 39 | 46 | `A1_7` / `COMP2_IN1-` / `VREF-` | `UART2_TX` / `TIMG8_C0` / `UART1_CTS` / `TIMA0_C0` / `TIMG6_C0` | 标准 IO；建议作拨码输入 |
| 任务选择 bit1 预留 | `PA22` | 40 | 47 | `A0_7` / `GPAMP_OUT` / `OPA0_OUT` | `UART2_RX` / `TIMG8_C1` / `UART1_RTS` / `TIMA0_C1` / `CLK_OUT` / `TIMA0_C0N` / `TIMG6_C1` | 标准 IO；建议作拨码输入 |
| 停止/备用键预留 | `PA28` | 3 | 3 | - | `UART0_TX` / `I2C0_SDA` / `TIMA0_C3` / `TIMA_FAL0` / `TIMG7_C0` / `TIMA1_C0` | 高驱动 IO；建议作按键输入 |
| 灰度 CCD5 | `PB18` | 37 | 44 | `A1_5` / `COMP1_IN2+` | `UART2_RX` / `SPI0_SCK` / `SPI1_CS2` / `TIMA1_C1` / `TIMA0_C2N` | 标准 IO |
| 灰度 CCD6 | `PB19` | 38 | 45 | `A1_6` / `COMP2_IN1+` / `OPA1_IN0+` | `COMP2_OUT` / `SPI0_POCI` / `TIMG8_C1` / `UART0_CTS` / `TIMG7_C1` | 标准 IO |
| 右编码器 A | `PB20` | 41 | 48 | `A0_6` / `OPA1_IN0-` | `SPI0_CS2` / `SPI1_CS0` / `TIMA0_C2` / `TIMG12_C0` / `TIMA_FAL1` / `TIMA0_C1` / `TIMA1_C1N` | 标准 IO |
| 右编码器 B | `PB24` | 42 | 52 | `A0_5` / `COMP1_IN1+` | `SPI0_CS3` / `SPI0_CS1` / `TIMA0_C3` / `TIMG12_C1` / `TIMA0_C1N` / `TIMA1_C0N` | 标准 IO |
| 灰度 CCD3 | `PA24` | 44 | 54 | `A0_3` / `OPA0_IN1-` | `UART2_RX` / `SPI0_CS2` / `TIMA0_C3N` / `TIMG0_C1` / `UART3_RTS` / `TIMG7_C1` / `TIMA1_C1` | 标准 IO |
| 灰度 CCD2 | `PA25` | 45 | 55 | `A0_2` / `OPA0_IN1+` | `UART3_RX` / `SPI1_CS3` / `TIMG12_C1` / `TIMA0_C3` / `TIMA0_C1N` | 标准 IO |
| 灰度 CCD1 | `PA26` | 46 | 59 | `A0_1` / `COMP0_IN0+` / `OPA0_IN0+` / `GPAMP_IN+` | `UART3_TX` / `SPI1_CS0` / `TIMG8_C0` / `TIMA_FAL0` / `CAN_TX` / `TIMG7_C0` | 标准 IO |
| 灰度 CCD0 | `PA27` | 47 | 60 | `A0_0` / `COMP0_IN0-` / `OPA0_IN0-` | `RTC_OUT` / `SPI1_CS1` / `TIMG8_C1` / `TIMA_FAL2` / `CAN_RX` / `TIMG7_C1` | 标准 IO |

说明：表里的 `A0_x`、`A1_x`、`COMPx`、`OPAx`、`GPAMP` 是该引脚可选择的模拟复用功能，不表示这个脚只能做模拟输入。当前灰度传感器是数字输出型，SysConfig 把 `PA27/PA26/PA25/PA24/PA7/PB18/PB19` 配成 GPIO 输入，所以它们读的是数字高低电平。只有以后换成模拟灰度传感器时，才需要按 `A0_x/A1_x` 这些 ADC 通道重新设计。

## 3. 为什么 PA8/PA9 可以做 PWM

`PA8` 的数据手册数字复用功能包含 `TIMA0_C0`，当前 SysConfig 生成结果为：

```c
#define GPIO_PWM_MOTOR_C0_PIN         DL_GPIO_PIN_8
#define GPIO_PWM_MOTOR_C0_IOMUX_FUNC  IOMUX_PINCM19_PF_TIMA0_CCP0
```

`PA9` 的数据手册数字复用功能包含 `TIMA0_C1`，当前 SysConfig 生成结果为：

```c
#define GPIO_PWM_MOTOR_C1_PIN         DL_GPIO_PIN_9
#define GPIO_PWM_MOTOR_C1_IOMUX_FUNC  IOMUX_PINCM20_PF_TIMA0_CCP1
```

所以 `PA8/PA9` 是硬件 PWM 输出，不是软件模拟 PWM。

## 4. 电源和避坑

| 项目 | 要求 |
| --- | --- |
| MCU IO 电平 | 3.3V |
| 灰度传感器 | 优先 3.3V 供电；如果输出 5V，必须降压 |
| 编码器 A/B | 进 MCU 前必须是 3.3V 兼容电平 |
| HC-SR04 ECHO | 常见为 5V，必须分压或电平转换到 3.3V |
| TB6612 `VCC` | 接 3.3V 逻辑电源 |
| TB6612 `VM` | 接电机电源，按电机额定电压 |
| GND | 电池、降压、MSPM0、TB6612、灰度、编码器必须共地 |

不要占用：

```text
PB14 / PB15 / PB16 / PB17：板载 SPI Flash
PB4 / PB5：这块板没有引出
PA2：ROSC 相关脚，不建议用
PA19 / PA20：SWD 调试脚
NRST：复位脚
```
