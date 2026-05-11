#include "bsp_adc.h"
#include "../pin_map.h"

#define LINE_LOW_VALUE   0u
#define LINE_HIGH_VALUE  4095u

#ifndef BSP_LINE_SENSOR_ACTIVE_LOW
#define BSP_LINE_SENSOR_ACTIVE_LOW 0
#endif

void BSP_Adc_Init(void)
{
}

uint16_t BSP_Adc_GetBattery(void)
{
    return 0u;
}

uint16_t BSP_Adc_GetBatteryMv(void)
{
    return 0u;
}

static uint16_t gpio_to_raw(GPIO_Regs *port, uint32_t pin)
{
    uint8_t high = (DL_GPIO_readPins(port, pin) != 0u) ? 1u : 0u;

#if BSP_LINE_SENSOR_ACTIVE_LOW
    high = high ? 0u : 1u;
#endif
    return high ? LINE_HIGH_VALUE : LINE_LOW_VALUE;
}

void BSP_Adc_GetCCD(uint16_t v[BSP_ADC_CCD_COUNT])
{
    v[0] = gpio_to_raw(LINE_A_PORT, LINE_CCD0_PIN);
    v[1] = gpio_to_raw(LINE_A_PORT, LINE_CCD1_PIN);
    v[2] = gpio_to_raw(LINE_A_PORT, LINE_CCD2_PIN);
    v[3] = gpio_to_raw(LINE_A_PORT, LINE_CCD3_PIN);
    v[4] = gpio_to_raw(LINE_A_PORT, LINE_CCD4_PIN);
    v[5] = gpio_to_raw(LINE_B_PORT, LINE_CCD5_PIN);
    v[6] = gpio_to_raw(LINE_B_PORT, LINE_CCD6_PIN);
}
