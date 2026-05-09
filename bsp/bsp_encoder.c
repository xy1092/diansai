#include "bsp_encoder.h"
#include "../pin_map.h"

static uint16_t s_last[2] = {0};
static int64_t  s_total[2] = {0};

static inline uint16_t read_raw(uint8_t ch)
{
    return (uint16_t)DL_TimerG_getTimerCount(
        (ch == 0) ? ENC_LEFT_INST : ENC_RIGHT_INST);
}

void BSP_Encoder_Init(void)
{
    DL_TimerG_startCounter(ENC_LEFT_INST);
    DL_TimerG_startCounter(ENC_RIGHT_INST);
    s_last[0] = read_raw(0);
    s_last[1] = read_raw(1);
    s_total[0] = 0;
    s_total[1] = 0;
}

int32_t BSP_Encoder_GetDelta(uint8_t ch)
{
    uint16_t cur = read_raw(ch);
    int32_t  d   = (int16_t)(cur - s_last[ch]);   /* 强转为 int16 处理 wrap */
    s_last[ch]   = cur;
    s_total[ch] += d;
    return d;
}

int64_t BSP_Encoder_GetTotal(uint8_t ch)
{
    return s_total[ch];
}

void BSP_Encoder_Reset(uint8_t ch)
{
    s_total[ch] = 0;
    s_last[ch]  = read_raw(ch);
}
