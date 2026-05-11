#include "bsp_encoder.h"
#include "../pin_map.h"

static uint8_t  s_last_state[2] = {0};
static int32_t  s_delta[2] = {0};
static int64_t  s_total[2] = {0};

static uint8_t read_state(uint8_t ch)
{
    GPIO_Regs *port = (ch == 0u) ? ENC_LEFT_PORT : ENC_RIGHT_PORT;
    uint32_t a_pin = (ch == 0u) ? ENC_LEFT_A_PIN : ENC_RIGHT_A_PIN;
    uint32_t b_pin = (ch == 0u) ? ENC_LEFT_B_PIN : ENC_RIGHT_B_PIN;
    uint8_t a = (DL_GPIO_readPins(port, a_pin) != 0u) ? 1u : 0u;
    uint8_t b = (DL_GPIO_readPins(port, b_pin) != 0u) ? 1u : 0u;

    return (uint8_t)((a << 1) | b);
}

static int8_t decode_step(uint8_t last, uint8_t cur)
{
    static const int8_t table[16] = {
         0,  1, -1,  0,
        -1,  0,  0,  1,
         1,  0,  0, -1,
         0, -1,  1,  0,
    };

    return table[((last & 0x03u) << 2) | (cur & 0x03u)];
}

void BSP_Encoder_Init(void)
{
    s_last_state[0] = read_state(0);
    s_last_state[1] = read_state(1);
    s_delta[0] = 0;
    s_delta[1] = 0;
    s_total[0] = 0;
    s_total[1] = 0;
}

void BSP_Encoder_Poll(void)
{
    for (uint8_t ch = 0; ch < 2u; ++ch) {
        uint8_t cur = read_state(ch);
        int32_t d = decode_step(s_last_state[ch], cur);

        s_last_state[ch] = cur;
        s_delta[ch] += d;
        s_total[ch] += d;
    }
}

int32_t BSP_Encoder_GetDelta(uint8_t ch)
{
    int32_t d = s_delta[ch];
    s_delta[ch] = 0;
    return d;
}

int64_t BSP_Encoder_GetTotal(uint8_t ch)
{
    return s_total[ch];
}

void BSP_Encoder_Reset(uint8_t ch)
{
    s_delta[ch] = 0;
    s_total[ch] = 0;
    s_last_state[ch] = read_state(ch);
}
