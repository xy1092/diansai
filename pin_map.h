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
/* 当前默认不接编码器；需要时关闭 NUEDC_NO_ENCODER 并补 SysConfig。 */

/* ---------- IMU / OLED I²C ---------- */
#ifdef I2C_INST_INST
#define I2C_INST                I2C_INST_INST
#endif
#define MPU6050_I2C_ADDR        0x68
#define SSD1306_I2C_ADDR        0x3C

/* ---------- UART ---------- */
/* 地猛星板载 CH340 使用 UART0/PA10/PA11。赛题禁止摄像头，不使用 OpenMV。 */

/* ---------- Notify / Start ---------- */
#define NOTIFY_PORT             GPIO_NOTIFY_PORT
#define NOTIFY_LED_PIN          GPIO_NOTIFY_LED_PIN
#ifdef GPIO_BUZZER_BUZZ_PIN
#define BUZZER_PORT             GPIO_BUZZER_PORT
#define BUZZER_PIN              GPIO_BUZZER_BUZZ_PIN
#endif
#define CONTROL_PORT            GPIO_CONTROL_PORT
#define CONTROL_START_PIN       GPIO_CONTROL_START_PIN

/* ---------- ADC (7 路灰度 CCD；电池采样为可选项) ---------- */
#define ADC_BAT_INST            ADC_SENSE_INST
#ifdef ADC_SENSE_ADCMEM_BAT
#define ADC_BAT_CHAN            ADC_SENSE_ADCMEM_BAT
#endif
#define ADC_CCD0_INST           ADC_SENSE_INST
#define ADC_CCD1_INST           ADC_SENSE_INST
#define ADC_CCD2_INST           ADC_SENSE_INST
#define ADC_CCD3_INST           ADC_SENSE_INST
#define ADC_CCD4_INST           ADC_SENSE_R_INST
#define ADC_CCD5_INST           ADC_SENSE_R_INST
#define ADC_CCD6_INST           ADC_SENSE_R_INST
#define ADC_CCD0_CHAN           ADC_SENSE_ADCMEM_0
#define ADC_CCD1_CHAN           ADC_SENSE_ADCMEM_1
#define ADC_CCD2_CHAN           ADC_SENSE_ADCMEM_2
#define ADC_CCD3_CHAN           ADC_SENSE_ADCMEM_3
#define ADC_CCD4_CHAN           ADC_SENSE_R_ADCMEM_0
#define ADC_CCD5_CHAN           ADC_SENSE_R_ADCMEM_1
#define ADC_CCD6_CHAN           ADC_SENSE_R_ADCMEM_2

#endif /* PIN_MAP_H */
