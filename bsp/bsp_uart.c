#include "bsp_uart.h"
#include "../pin_map.h"
#include <stdarg.h>
#include <stdio.h>

static uart_rx_cb_t s_cb_dbg = 0;

void BSP_Uart_Init(void)
{
    DL_UART_Main_enableInterrupt(UART_DEBUG_INST,
        DL_UART_MAIN_INTERRUPT_RX);
    NVIC_EnableIRQ(UART_DEBUG_INST_INT_IRQN);
}

void BSP_Uart_SetRxCallback(uint8_t is_openmv, uart_rx_cb_t cb)
{
    (void)is_openmv;
    s_cb_dbg = cb;
}

void BSP_Uart_Send(uint8_t is_openmv, const uint8_t *data, uint16_t len)
{
    UART_Regs *inst = UART_DEBUG_INST;
    (void)is_openmv;
    for (uint16_t i = 0; i < len; ++i) {
        while (DL_UART_Main_isBusy(inst)) {}
        DL_UART_Main_transmitData(inst, data[i]);
    }
}

void BSP_Uart_Printf(const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) BSP_Uart_Send(0, (uint8_t *)buf, (uint16_t)n);
}

/* ===== ISR（函数名以 SysConfig 为准）===== */
void UART_DEBUG_INST_IRQHandler(void)
{
    if (DL_UART_Main_getPendingInterrupt(UART_DEBUG_INST)
        == DL_UART_MAIN_IIDX_RX) {
        uint8_t b = DL_UART_Main_receiveData(UART_DEBUG_INST);
        if (s_cb_dbg) s_cb_dbg(b);
    }
}
