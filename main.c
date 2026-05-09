#include "ti_msp_dl_config.h"
#include "app/app_main.h"

#if !defined(CPUCLK_FREQ_MHZ) && defined(CPUCLK_FREQ)
#define CPUCLK_FREQ_MHZ (CPUCLK_FREQ / 1000000u)
#endif

volatile uint32_t g_tick_ms = 0;

void SysTick_Handler(void)
{
    g_tick_ms++;
    App_Tick();
}

int main(void)
{
    SYSCFG_DL_init();

    App_Init();

    SysTick_Config(CPUCLK_FREQ_MHZ * 1000u);   /* 1 ms */

    while (1) {
        App_Loop();
    }
}
