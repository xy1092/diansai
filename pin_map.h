#ifndef PIN_MAP_H
#define PIN_MAP_H

/*
 * 信号名 → SysConfig 生成的宏。
 * 实际引脚在 .syscfg 中配置后，由 ti_msp_dl_config.h 产生。
 * 这里做一层别名，方便换板/换引脚时只改一个文件。
 */
#include "ti_msp_dl_config.h"

/* ---------- Motor PWM (TIMA0 on PA8/PA9) ---------- */
#define PWM_CH_LEFT             GPIO_PWM_MOTOR_C0_IDX
#define PWM_CH_RIGHT            GPIO_PWM_MOTOR_C1_IDX

/* ---------- Motor Direction GPIO ---------- */
#define MOTOR_DIR_PORT          GPIO_MOTOR_DIR_PORT
#define LEFT_AIN1_PIN           GPIO_MOTOR_DIR_LEFT_AIN1_PIN
#define LEFT_AIN2_PIN           GPIO_MOTOR_DIR_LEFT_AIN2_PIN
#define RIGHT_BIN1_PIN          GPIO_MOTOR_DIR_RIGHT_BIN1_PIN
#define RIGHT_BIN2_PIN          GPIO_MOTOR_DIR_RIGHT_BIN2_PIN
#ifdef GPIO_MOTOR_DIR_STBY_PIN
#define MOTOR_STBY_PIN          GPIO_MOTOR_DIR_STBY_PIN
#endif

/* ---------- Encoder ---------- */
#define ENC_LEFT_PORT           GPIO_ENCODER_A_PORT
#define ENC_LEFT_A_PIN          GPIO_ENCODER_A_LEFT_A_PIN
#define ENC_LEFT_B_PIN          GPIO_ENCODER_A_LEFT_B_PIN
#define ENC_RIGHT_PORT          GPIO_ENCODER_B_PORT
#define ENC_RIGHT_A_PIN         GPIO_ENCODER_B_RIGHT_A_PIN
#define ENC_RIGHT_B_PIN         GPIO_ENCODER_B_RIGHT_B_PIN

/* ---------- IMU / OLED I²C ---------- */
#ifdef I2C_INST_INST
#define I2C_INST                I2C_INST_INST
#endif
#define MPU6050_I2C_ADDR        0x68
#define SSD1306_I2C_ADDR        0x3C

/* ---------- UART ---------- */
/* 地猛星板载 CH340 使用 UART0/PA10/PA11。 */
#ifdef UART_K230_INST
#define K230_UART_INST          UART_K230_INST
#define K230_UART_IRQN          UART_K230_INST_INT_IRQN
#endif

/* ---------- Reserved Extension Ports ---------- */
#ifdef GPIO_RESERVED_A_ULTRASONIC_TRIG_PIN
#define ULTRASONIC_PORT         GPIO_RESERVED_A_PORT
#define ULTRASONIC_TRIG_PIN     GPIO_RESERVED_A_ULTRASONIC_TRIG_PIN
#define ULTRASONIC_ECHO_PIN     GPIO_RESERVED_A_ULTRASONIC_ECHO_PIN
#endif
#ifdef GPIO_RESERVED_A_K230_RESET_PIN
#define K230_RESET_PORT         GPIO_RESERVED_A_PORT
#define K230_RESET_PIN          GPIO_RESERVED_A_K230_RESET_PIN
#endif
#ifdef GPIO_RESERVED_B_K230_READY_PIN
#define K230_READY_PORT         GPIO_RESERVED_B_PORT
#define K230_READY_PIN          GPIO_RESERVED_B_K230_READY_PIN
#endif

/* ---------- Notify / Start ---------- */
#define NOTIFY_PORT             GPIO_NOTIFY_PORT
#define NOTIFY_LED_PIN          GPIO_NOTIFY_LED_PIN
#ifdef GPIO_BUZZER_BUZZ_PIN
#define BUZZER_PORT             GPIO_BUZZER_PORT
#define BUZZER_PIN              GPIO_BUZZER_BUZZ_PIN
#endif
#define CONTROL_PORT            GPIO_CONTROL_PORT
#define CONTROL_START_PIN       GPIO_CONTROL_START_PIN

/* ---------- Digital line sensors (7 路数字灰度) ---------- */
#define LINE_A_PORT             GPIO_LINE_A_PORT
#define LINE_CCD0_PIN           GPIO_LINE_A_CCD0_PIN
#define LINE_CCD1_PIN           GPIO_LINE_A_CCD1_PIN
#define LINE_CCD2_PIN           GPIO_LINE_A_CCD2_PIN
#define LINE_CCD3_PIN           GPIO_LINE_A_CCD3_PIN
#define LINE_CCD4_PIN           GPIO_LINE_A_CCD4_PIN
#define LINE_B_PORT             GPIO_LINE_B_PORT
#define LINE_CCD5_PIN           GPIO_LINE_B_CCD5_PIN
#define LINE_CCD6_PIN           GPIO_LINE_B_CCD6_PIN

#endif /* PIN_MAP_H */
