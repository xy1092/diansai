#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>
#include "pid.h"

/*
 * 串口协议（调试 UART，默认 115200 8N1）：
 *
 *   MCU → PC 数据行（100 Hz 默认，可调）：
 *     $P,<ts_ms>,<ch>,<sp>,<meas>,<out>,<err>,<integ>,<deriv>,<p>,<i>,<d>,<raw_out>\r\n
 *     ch ∈ { L, R, LINE, ANG }
 *
 *   MCU → PC 原始循迹采样（默认关闭）：
 *     $L,<ts_ms>,<bias>,<contrast>,<strength>,<on_line>,<r0>,...,<r6>\r\n
 *
 *   PC → MCU 命令：
 *     $SET,<ch>,<KP|KI|KD>,<float>\r\n
 *     $RATE,<hz>\r\n          修改发送速率
 *     $RAWLINE,<0|1>\r\n      开/关 7 路灰度原始流
 *     $RST\r\n                重置所有 PID 积分
 *     $PAUSE\r\n / $RESUME\r\n
 *     $DUMP\r\n               打印当前增益一次
 */

typedef enum {
    TELEM_CH_L    = 0,
    TELEM_CH_R    = 1,
    TELEM_CH_LINE = 2,
    TELEM_CH_ANG  = 3,
    TELEM_CH_MAX
} TelemCh_t;

void Telem_Init(void);

/* 把四个 PID 与对应的 setpoint / measure 信号注册进来 */
void Telem_Bind(TelemCh_t ch, PID_t *p, const float *sp, const float *meas);
void Telem_BindLineSensors(const uint16_t *raw, uint8_t raw_count,
                           const uint16_t *contrast, const uint16_t *strength,
                           const float *bias, const uint8_t *on_line);
void Telem_SetRawLineEnabled(uint8_t on);

/* 每 1 ms 调用（或更快）以驱动节拍；内部会按 rate 输出一行 */
void Telem_Tick(uint32_t ts_ms);

/* 串口 RX 字节回调（绑给 bsp_uart 的调试串口） */
void Telem_OnRxByte(uint8_t b);

#endif
