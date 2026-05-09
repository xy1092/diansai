#include "bsp_adc.h"
#include "../pin_map.h"

#define VREF_MV           3300u
#define BAT_DIV_RATIO_X10 30u   /* 3:1 分压 → ×3.0 */

static void sample_sequence(ADC12_Regs *inst, uint32_t done_interrupt)
{
    DL_ADC12_clearInterruptStatus(inst, done_interrupt);
    DL_ADC12_startConversion(inst);
    while ((DL_ADC12_getRawInterruptStatus(inst, done_interrupt) &
        done_interrupt) == 0u) {}
    DL_ADC12_enableConversions(inst);
}

#ifdef ADC_BAT_CHAN
static uint16_t sample_once(uint8_t memidx)
{
    sample_sequence(ADC_BAT_INST, DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);
    return DL_ADC12_getMemResult(ADC_BAT_INST, memidx);
}
#endif

void BSP_Adc_Init(void)
{
    DL_ADC12_enableConversions(ADC_BAT_INST);
#ifdef ADC_SENSE_R_INST
    DL_ADC12_enableConversions(ADC_SENSE_R_INST);
#endif
}

uint16_t BSP_Adc_GetBattery(void)
{
#ifdef ADC_BAT_CHAN
    return sample_once(ADC_BAT_CHAN);
#else
    return 0u;
#endif
}

uint16_t BSP_Adc_GetBatteryMv(void)
{
#ifdef ADC_BAT_CHAN
    uint32_t raw = sample_once(ADC_BAT_CHAN);
    uint32_t mv  = raw * VREF_MV / 4095u;
    return (uint16_t)(mv * BAT_DIV_RATIO_X10 / 10u);
#else
    return 0u;
#endif
}

void BSP_Adc_GetCCD(uint16_t v[BSP_ADC_CCD_COUNT])
{
    sample_sequence(ADC_SENSE_INST, DL_ADC12_INTERRUPT_MEM3_RESULT_LOADED);
    sample_sequence(ADC_SENSE_R_INST, DL_ADC12_INTERRUPT_MEM2_RESULT_LOADED);
    v[0] = DL_ADC12_getMemResult(ADC_CCD0_INST, ADC_CCD0_CHAN);
    v[1] = DL_ADC12_getMemResult(ADC_CCD1_INST, ADC_CCD1_CHAN);
    v[2] = DL_ADC12_getMemResult(ADC_CCD2_INST, ADC_CCD2_CHAN);
    v[3] = DL_ADC12_getMemResult(ADC_CCD3_INST, ADC_CCD3_CHAN);
    v[4] = DL_ADC12_getMemResult(ADC_CCD4_INST, ADC_CCD4_CHAN);
    v[5] = DL_ADC12_getMemResult(ADC_CCD5_INST, ADC_CCD5_CHAN);
    v[6] = DL_ADC12_getMemResult(ADC_CCD6_INST, ADC_CCD6_CHAN);
}
