#ifndef BSP_ULTRASONIC_H
#define BSP_ULTRASONIC_H

#include <stdint.h>

void    BSP_Ultrasonic_Init(void);
void    BSP_Ultrasonic_SetTrig(uint8_t on);
uint8_t BSP_Ultrasonic_ReadEcho(void);

#endif /* BSP_ULTRASONIC_H */
