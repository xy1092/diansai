#include "bsp_motor.h"
#include "../pin_map.h"

#define PWM_MAX  1000

static inline void set_forward_dir(uint8_t ch)
{
    if (ch == 0) {
        DL_GPIO_setPins(MOTOR_DIR_PORT, LEFT_AIN1_PIN);
        DL_GPIO_clearPins(MOTOR_DIR_PORT, LEFT_AIN2_PIN);
    } else {
        DL_GPIO_setPins(MOTOR_DIR_PORT, RIGHT_BIN1_PIN);
        DL_GPIO_clearPins(MOTOR_DIR_PORT, RIGHT_BIN2_PIN);
    }
}

void BSP_Motor_Init(void)
{
    DL_TimerA_startCounter(PWM_MOTOR_INST);
    BSP_Motor_Standby(1);
    BSP_Motor_SetDuty(0, 0);
    BSP_Motor_SetDuty(1, 0);
}

void BSP_Motor_SetDuty(uint8_t ch, int16_t duty)
{
    if (duty >  PWM_MAX) duty =  PWM_MAX;
    if (duty < 0) duty = 0;

    set_forward_dir(ch);

    uint32_t abs_duty = (uint32_t)duty;
    uint8_t  cc_idx   = (ch == 0) ? PWM_CH_LEFT : PWM_CH_RIGHT;
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, abs_duty, cc_idx);
}

void BSP_Motor_Brake(uint8_t ch)
{
    /* TB6612: AIN1 = AIN2 = 1 → 短接刹车 */
    if (ch == 0) {
        DL_GPIO_setPins(MOTOR_DIR_PORT, LEFT_AIN1_PIN | LEFT_AIN2_PIN);
    } else {
        DL_GPIO_setPins(MOTOR_DIR_PORT, RIGHT_BIN1_PIN | RIGHT_BIN2_PIN);
    }
    uint8_t cc_idx = (ch == 0) ? PWM_CH_LEFT : PWM_CH_RIGHT;
    DL_TimerA_setCaptureCompareValue(PWM_MOTOR_INST, 0, cc_idx);
}

void BSP_Motor_Standby(uint8_t on)
{
#ifdef MOTOR_STBY_PIN
    if (on) {
        DL_GPIO_setPins(MOTOR_DIR_PORT, MOTOR_STBY_PIN);
    } else {
        DL_GPIO_clearPins(MOTOR_DIR_PORT, MOTOR_STBY_PIN);
    }
#else
    (void)on;
#endif
}
