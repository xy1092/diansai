#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "bsp_imu.h"
#include "bsp_uart.h"
#include "bsp_adc.h"
#include "bsp_oled.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

volatile int16_t  g_debug_motor_duty[2] = {0, 0};
volatile int32_t  g_debug_encoder_total[2] = {0, 0};
volatile uint16_t g_debug_battery_raw = 3000u;
volatile uint16_t g_debug_ccd_raw[BSP_ADC_CCD_COUNT] = {200, 500, 900, 1400, 900, 500, 200};
volatile uint8_t  g_debug_uart_echo = 0u;
volatile uint8_t  g_debug_use_manual_encoder_delta = 0u;
volatile int32_t  g_debug_manual_encoder_delta[2] = {0, 0};

static uart_rx_cb_t s_rx_cb[2] = {0, 0};

void BSP_Motor_Init(void)
{
    g_debug_motor_duty[0] = 0;
    g_debug_motor_duty[1] = 0;
}

void BSP_Motor_SetDuty(uint8_t ch, int16_t duty)
{
    if (ch < 2u) g_debug_motor_duty[ch] = duty;
}

void BSP_Motor_Brake(uint8_t ch)
{
    if (ch < 2u) g_debug_motor_duty[ch] = 0;
}

void BSP_Motor_Standby(uint8_t on)
{
    (void)on;
}

void BSP_Encoder_Init(void)
{
    g_debug_encoder_total[0] = 0;
    g_debug_encoder_total[1] = 0;
}

int32_t BSP_Encoder_GetDelta(uint8_t ch)
{
    int32_t delta;

    if (ch >= 2u) return 0;

    if (g_debug_use_manual_encoder_delta) {
        delta = g_debug_manual_encoder_delta[ch];
    } else {
        delta = (int32_t)g_debug_motor_duty[ch] / 20;
    }

    g_debug_encoder_total[ch] += delta;
    return delta;
}

int64_t BSP_Encoder_GetTotal(uint8_t ch)
{
    if (ch >= 2u) return 0;
    return g_debug_encoder_total[ch];
}

void BSP_Encoder_Reset(uint8_t ch)
{
    if (ch < 2u) g_debug_encoder_total[ch] = 0;
}

void BSP_IMU_Init(void)
{
}

void BSP_IMU_Update(float *pitch, float *roll, float *yaw)
{
    if (pitch) *pitch = 0.0f;
    if (roll)  *roll = 0.0f;
    if (yaw)   *yaw = 0.0f;
}

void BSP_IMU_GetGyro(float *gx, float *gy, float *gz)
{
    if (gx) *gx = 0.0f;
    if (gy) *gy = 0.0f;
    if (gz) *gz = 0.0f;
}

void BSP_IMU_GetAccel(float *ax, float *ay, float *az)
{
    if (ax) *ax = 0.0f;
    if (ay) *ay = 0.0f;
    if (az) *az = 1.0f;
}

void BSP_Uart_Init(void)
{
    s_rx_cb[0] = 0;
    s_rx_cb[1] = 0;
}

void BSP_Uart_SetRxCallback(uint8_t is_openmv, uart_rx_cb_t cb)
{
    if (is_openmv < 2u) s_rx_cb[is_openmv] = cb;
}

void BSP_Uart_Send(uint8_t is_openmv, const uint8_t *data, uint16_t len)
{
    (void)is_openmv;
    if (g_debug_uart_echo && data && len) {
        fwrite(data, 1u, len, stdout);
        fflush(stdout);
    }
}

void BSP_Uart_Printf(const char *fmt, ...)
{
    va_list ap;

    if (!g_debug_uart_echo) return;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);
}

void BSP_Adc_Init(void)
{
}

uint16_t BSP_Adc_GetBattery(void)
{
    return g_debug_battery_raw;
}

uint16_t BSP_Adc_GetBatteryMv(void)
{
    return (uint16_t)(((uint32_t)g_debug_battery_raw * 3300u / 4095u) * 3u);
}

void BSP_Adc_GetCCD(uint16_t v[BSP_ADC_CCD_COUNT])
{
    uint8_t i;

    for (i = 0; i < BSP_ADC_CCD_COUNT; ++i) {
        v[i] = g_debug_ccd_raw[i];
    }
}

void BSP_OLED_Init(void)
{
}

void BSP_OLED_Clear(void)
{
}

void BSP_OLED_ShowStr(uint8_t x, uint8_t y, const char *s)
{
    (void)x;
    (void)y;
    (void)s;
}

void BSP_OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len)
{
    (void)x;
    (void)y;
    (void)num;
    (void)len;
}

void BSP_OLED_ShowFloat(uint8_t x, uint8_t y, float v)
{
    (void)x;
    (void)y;
    (void)v;
}

void BSP_OLED_Refresh(void)
{
}
