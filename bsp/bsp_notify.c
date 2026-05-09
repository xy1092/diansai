#include "../pin_map.h"
#include <stdint.h>

void BSP_Notify_Set(uint8_t on)
{
    if (on) {
        DL_GPIO_setPins(NOTIFY_PORT, NOTIFY_LED_PIN);
#ifdef BUZZER_PIN
        DL_GPIO_setPins(BUZZER_PORT, BUZZER_PIN);
#endif
    } else {
        DL_GPIO_clearPins(NOTIFY_PORT, NOTIFY_LED_PIN);
#ifdef BUZZER_PIN
        DL_GPIO_clearPins(BUZZER_PORT, BUZZER_PIN);
#endif
    }
}
