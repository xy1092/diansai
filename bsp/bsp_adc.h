#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>

#define BSP_ADC_CCD_COUNT 7u

void     BSP_Adc_Init(void);

/* 原始 12-bit 值 */
uint16_t BSP_Adc_GetBattery(void);

/* 返回电池电压（mV），已考虑分压比 */
uint16_t BSP_Adc_GetBatteryMv(void);

/* 7 路灰度，v[0..6] */
void     BSP_Adc_GetCCD(uint16_t v[BSP_ADC_CCD_COUNT]);

#endif
