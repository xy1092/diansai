#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdint.h>

/* ch: 0 = 左电机, 1 = 右电机; duty: -1000 ~ +1000 */
void BSP_Motor_Init(void);
void BSP_Motor_SetDuty(uint8_t ch, int16_t duty);
void BSP_Motor_Brake(uint8_t ch);
void BSP_Motor_Standby(uint8_t on);

#endif
