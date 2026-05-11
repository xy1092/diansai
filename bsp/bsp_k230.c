#include "bsp_k230.h"
#include "../pin_map.h"

static k230_rx_cb_t s_k230_cb = 0;

void BSP_K230_Init(void)
{
#ifdef K230_UART_INST
    DL_UART_Main_enableInterrupt(K230_UART_INST, DL_UART_MAIN_INTERRUPT_RX);
    NVIC_EnableIRQ(K230_UART_IRQN);
#endif
#ifdef K230_RESET_PIN
    DL_GPIO_setPins(K230_RESET_PORT, K230_RESET_PIN);
#endif
}

void BSP_K230_SetRxCallback(k230_rx_cb_t cb)
{
    s_k230_cb = cb;
}

void BSP_K230_Send(const uint8_t *data, uint16_t len)
{
#ifdef K230_UART_INST
    for (uint16_t i = 0; i < len; ++i) {
        while (DL_UART_Main_isBusy(K230_UART_INST)) {}
        DL_UART_Main_transmitData(K230_UART_INST, data[i]);
    }
#else
    (void)data;
    (void)len;
#endif
}

void BSP_K230_Reset(uint8_t hold_reset)
{
#ifdef K230_RESET_PIN
    if (hold_reset) {
        DL_GPIO_clearPins(K230_RESET_PORT, K230_RESET_PIN);
    } else {
        DL_GPIO_setPins(K230_RESET_PORT, K230_RESET_PIN);
    }
#else
    (void)hold_reset;
#endif
}

uint8_t BSP_K230_IsReady(void)
{
#ifdef K230_READY_PIN
    return (DL_GPIO_readPins(K230_READY_PORT, K230_READY_PIN) != 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}

#ifdef K230_UART_INST
void UART_K230_INST_IRQHandler(void)
{
    if (DL_UART_Main_getPendingInterrupt(K230_UART_INST) == DL_UART_MAIN_IIDX_RX) {
        uint8_t b = DL_UART_Main_receiveData(K230_UART_INST);
        if (s_k230_cb) s_k230_cb(b);
    }
}
#endif
