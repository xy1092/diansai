#include "protocol.h"
#include "../bsp/bsp_uart.h"
#include "ti_msp_dl_config.h"

#define MAX_PAYLOAD 32

typedef enum {
    PS_SYNC1, PS_SYNC2, PS_LEN, PS_TYPE, PS_DATA, PS_CRC, PS_CR, PS_LF
} ParseState_t;

static ParseState_t s_state;
static uint8_t      s_len, s_type, s_idx;
static uint8_t      s_crc_calc;
static uint8_t      s_buf[MAX_PAYLOAD];

static ProtoVision_t s_vision;
static uint8_t       s_vision_fresh = 0;
static uint8_t       s_start_flag   = 0;
static uint8_t       s_mission_req  = 0;

extern volatile uint32_t g_tick_ms;

static void on_rx_byte(uint8_t b);

void Proto_Init(void)
{
    s_state = PS_SYNC1;
    BSP_Uart_SetRxCallback(0, on_rx_byte);
}

void Proto_Poll(void)
{
#if defined(GPIO_CONTROL_PORT) && defined(GPIO_CONTROL_START_PIN)
    static uint8_t last_pressed = 0u;
    static uint32_t last_ms = 0u;
    uint8_t pressed = (DL_GPIO_readPins(GPIO_CONTROL_PORT, GPIO_CONTROL_START_PIN) != 0u) ? 1u : 0u;

    if (pressed && !last_pressed && (g_tick_ms - last_ms) >= 120u) {
        s_start_flag = 1u;
        last_ms = g_tick_ms;
    }
    last_pressed = pressed;
#endif
}

uint8_t Proto_HasFreshVisionFrame(void)
{
    if (s_vision_fresh) { s_vision_fresh = 0; return 1; }
    return 0;
}

ProtoVision_t Proto_GetVision(void) { return s_vision; }

uint8_t Proto_GetStartFlag(void)
{
    if (s_start_flag) { s_start_flag = 0; return 1; }
    return 0;
}

uint8_t Proto_GetMissionRequest(void)
{
    if (s_mission_req) {
        uint8_t req = s_mission_req;
        s_mission_req = 0;
        return req;
    }
    return 0;
}

static void dispatch_frame(void)
{
    if (s_type == 0x01 && s_len >= 7) {
        s_vision.center_x = (int16_t)(s_buf[0] | (s_buf[1] << 8));
        s_vision.center_y = (int16_t)(s_buf[2] | (s_buf[3] << 8));
        s_vision.class_id = s_buf[4];
        s_vision.area     = (uint16_t)(s_buf[5] | (s_buf[6] << 8));
        s_vision.ts_ms    = g_tick_ms;
        s_vision_fresh    = 1;
    } else if (s_type == 0x02 && s_len >= 1) {
        if (s_buf[0] == 0x01) s_start_flag = 1;
        if (s_buf[0] >= 0x11 && s_buf[0] <= 0x14) s_mission_req = (uint8_t)(s_buf[0] - 0x10);
    }
}

static void on_rx_byte(uint8_t b)
{
    if (b >= '1' && b <= '4') {
        s_mission_req = (uint8_t)(b - '0');
        s_state = PS_SYNC1;
        return;
    }
    if (b == 's' || b == 'S') {
        s_start_flag = 1u;
        s_state = PS_SYNC1;
        return;
    }

    switch (s_state) {
    case PS_SYNC1: if (b == 0xAA) s_state = PS_SYNC2; break;
    case PS_SYNC2: s_state = (b == 0x55) ? PS_LEN : PS_SYNC1; break;
    case PS_LEN:
        if (b == 0 || b > MAX_PAYLOAD) { s_state = PS_SYNC1; break; }
        s_len = b; s_crc_calc = b;
        s_state = PS_TYPE;
        break;
    case PS_TYPE:
        s_type = b; s_crc_calc ^= b;
        s_idx = 0;
        s_state = PS_DATA;
        break;
    case PS_DATA:
        if (s_idx < s_len) {
            s_buf[s_idx++] = b;
            s_crc_calc ^= b;
        }
        if (s_idx >= s_len) s_state = PS_CRC;
        break;
    case PS_CRC:
        if (b != s_crc_calc) { s_state = PS_SYNC1; break; }
        s_state = PS_CR;
        break;
    case PS_CR:
        s_state = (b == 0x0D) ? PS_LF : PS_SYNC1;
        break;
    case PS_LF:
        if (b == 0x0A) dispatch_frame();
        s_state = PS_SYNC1;
        break;
    }
}
