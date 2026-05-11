#include "bsp_ultrasonic.h"
#include "../pin_map.h"

void BSP_Ultrasonic_Init(void)
{
#ifdef ULTRASONIC_TRIG_PIN
    DL_GPIO_clearPins(ULTRASONIC_PORT, ULTRASONIC_TRIG_PIN);
#endif
}

void BSP_Ultrasonic_SetTrig(uint8_t on)
{
#ifdef ULTRASONIC_TRIG_PIN
    if (on) {
        DL_GPIO_setPins(ULTRASONIC_PORT, ULTRASONIC_TRIG_PIN);
    } else {
        DL_GPIO_clearPins(ULTRASONIC_PORT, ULTRASONIC_TRIG_PIN);
    }
#else
    (void)on;
#endif
}

uint8_t BSP_Ultrasonic_ReadEcho(void)
{
#ifdef ULTRASONIC_ECHO_PIN
    return (DL_GPIO_readPins(ULTRASONIC_PORT, ULTRASONIC_ECHO_PIN) != 0u) ? 1u : 0u;
#else
    return 0u;
#endif
}
