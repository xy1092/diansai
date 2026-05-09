/*
 * Copyright (c) 2023, Texas Instruments Incorporated - http://www.ti.com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ============ ti_msp_dl_config.h =============
 *  Configured MSPM0 DriverLib module declarations
 *
 *  DO NOT EDIT - This file is generated for the MSPM0G350X
 *  by the SysConfig tool.
 */
#ifndef ti_msp_dl_config_h
#define ti_msp_dl_config_h

#define CONFIG_MSPM0G350X
#define CONFIG_MSPM0G3507

#if defined(__ti_version__) || defined(__TI_COMPILER_VERSION__)
#define SYSCONFIG_WEAK __attribute__((weak))
#elif defined(__IAR_SYSTEMS_ICC__)
#define SYSCONFIG_WEAK __weak
#elif defined(__GNUC__)
#define SYSCONFIG_WEAK __attribute__((weak))
#endif

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>
#include <ti/driverlib/m0p/dl_core.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  ======== SYSCFG_DL_init ========
 *  Perform all required MSP DL initialization
 *
 *  This function should be called once at a point before any use of
 *  MSP DL.
 */


/* clang-format off */

#define POWER_STARTUP_DELAY                                                (16)



#define CPUCLK_FREQ                                                     32000000



/* Defines for PWM_MOTOR */
#define PWM_MOTOR_INST                                                     TIMA0
#define PWM_MOTOR_INST_IRQHandler                               TIMA0_IRQHandler
#define PWM_MOTOR_INST_INT_IRQN                                 (TIMA0_INT_IRQn)
#define PWM_MOTOR_INST_CLK_FREQ                                         32000000
/* GPIO defines for channel 0 */
#define GPIO_PWM_MOTOR_C0_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C0_PIN                                      DL_GPIO_PIN_8
#define GPIO_PWM_MOTOR_C0_IOMUX                                  (IOMUX_PINCM19)
#define GPIO_PWM_MOTOR_C0_IOMUX_FUNC                 IOMUX_PINCM19_PF_TIMA0_CCP0
#define GPIO_PWM_MOTOR_C0_IDX                                DL_TIMER_CC_0_INDEX
/* GPIO defines for channel 1 */
#define GPIO_PWM_MOTOR_C1_PORT                                             GPIOA
#define GPIO_PWM_MOTOR_C1_PIN                                      DL_GPIO_PIN_9
#define GPIO_PWM_MOTOR_C1_IOMUX                                  (IOMUX_PINCM20)
#define GPIO_PWM_MOTOR_C1_IOMUX_FUNC                 IOMUX_PINCM20_PF_TIMA0_CCP1
#define GPIO_PWM_MOTOR_C1_IDX                                DL_TIMER_CC_1_INDEX




/* Defines for I2C_INST */
#define I2C_INST_INST                                                       I2C1
#define I2C_INST_INST_IRQHandler                                 I2C1_IRQHandler
#define I2C_INST_INST_INT_IRQN                                     I2C1_INT_IRQn
#define I2C_INST_BUS_SPEED_HZ                                             400000
#define GPIO_I2C_INST_SDA_PORT                                             GPIOA
#define GPIO_I2C_INST_SDA_PIN                                     DL_GPIO_PIN_16
#define GPIO_I2C_INST_IOMUX_SDA                                  (IOMUX_PINCM38)
#define GPIO_I2C_INST_IOMUX_SDA_FUNC                   IOMUX_PINCM38_PF_I2C1_SDA
#define GPIO_I2C_INST_SCL_PORT                                             GPIOA
#define GPIO_I2C_INST_SCL_PIN                                     DL_GPIO_PIN_17
#define GPIO_I2C_INST_IOMUX_SCL                                  (IOMUX_PINCM39)
#define GPIO_I2C_INST_IOMUX_SCL_FUNC                   IOMUX_PINCM39_PF_I2C1_SCL


/* Defines for UART_DEBUG */
#define UART_DEBUG_INST                                                    UART0
#define UART_DEBUG_INST_FREQUENCY                                       32000000
#define UART_DEBUG_INST_IRQHandler                              UART0_IRQHandler
#define UART_DEBUG_INST_INT_IRQN                                  UART0_INT_IRQn
#define GPIO_UART_DEBUG_RX_PORT                                            GPIOA
#define GPIO_UART_DEBUG_TX_PORT                                            GPIOA
#define GPIO_UART_DEBUG_RX_PIN                                    DL_GPIO_PIN_11
#define GPIO_UART_DEBUG_TX_PIN                                    DL_GPIO_PIN_10
#define GPIO_UART_DEBUG_IOMUX_RX                                 (IOMUX_PINCM22)
#define GPIO_UART_DEBUG_IOMUX_TX                                 (IOMUX_PINCM21)
#define GPIO_UART_DEBUG_IOMUX_RX_FUNC                  IOMUX_PINCM22_PF_UART0_RX
#define GPIO_UART_DEBUG_IOMUX_TX_FUNC                  IOMUX_PINCM21_PF_UART0_TX
#define UART_DEBUG_BAUD_RATE                                            (115200)
#define UART_DEBUG_IBRD_32_MHZ_115200_BAUD                                  (17)
#define UART_DEBUG_FBRD_32_MHZ_115200_BAUD                                  (23)





/* Defines for ADC_SENSE */
#define ADC_SENSE_INST                                                      ADC0
#define ADC_SENSE_INST_IRQHandler                                ADC0_IRQHandler
#define ADC_SENSE_INST_INT_IRQN                                  (ADC0_INT_IRQn)
#define ADC_SENSE_ADCMEM_0                                    DL_ADC12_MEM_IDX_0
#define ADC_SENSE_ADCMEM_0_REF                   DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_SENSE_ADCMEM_0_REF_VOLTAGE_V                                     3.3
#define ADC_SENSE_ADCMEM_1                                    DL_ADC12_MEM_IDX_1
#define ADC_SENSE_ADCMEM_1_REF                   DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_SENSE_ADCMEM_1_REF_VOLTAGE_V                                     3.3
#define ADC_SENSE_ADCMEM_2                                    DL_ADC12_MEM_IDX_2
#define ADC_SENSE_ADCMEM_2_REF                   DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_SENSE_ADCMEM_2_REF_VOLTAGE_V                                     3.3
#define ADC_SENSE_ADCMEM_3                                    DL_ADC12_MEM_IDX_3
#define ADC_SENSE_ADCMEM_3_REF                   DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_SENSE_ADCMEM_3_REF_VOLTAGE_V                                     3.3
#define GPIO_ADC_SENSE_C0_PORT                                             GPIOA
#define GPIO_ADC_SENSE_C0_PIN                                     DL_GPIO_PIN_27
#define GPIO_ADC_SENSE_IOMUX_C0                                  (IOMUX_PINCM60)
#define GPIO_ADC_SENSE_IOMUX_C0_FUNC              (IOMUX_PINCM60_PF_UNCONNECTED)
#define GPIO_ADC_SENSE_C1_PORT                                             GPIOA
#define GPIO_ADC_SENSE_C1_PIN                                     DL_GPIO_PIN_26
#define GPIO_ADC_SENSE_IOMUX_C1                                  (IOMUX_PINCM59)
#define GPIO_ADC_SENSE_IOMUX_C1_FUNC              (IOMUX_PINCM59_PF_UNCONNECTED)
#define GPIO_ADC_SENSE_C2_PORT                                             GPIOA
#define GPIO_ADC_SENSE_C2_PIN                                     DL_GPIO_PIN_25
#define GPIO_ADC_SENSE_IOMUX_C2                                  (IOMUX_PINCM55)
#define GPIO_ADC_SENSE_IOMUX_C2_FUNC              (IOMUX_PINCM55_PF_UNCONNECTED)
#define GPIO_ADC_SENSE_C3_PORT                                             GPIOA
#define GPIO_ADC_SENSE_C3_PIN                                     DL_GPIO_PIN_24
#define GPIO_ADC_SENSE_IOMUX_C3                                  (IOMUX_PINCM54)
#define GPIO_ADC_SENSE_IOMUX_C3_FUNC              (IOMUX_PINCM54_PF_UNCONNECTED)

/* Defines for ADC_SENSE_R */
#define ADC_SENSE_R_INST                                                    ADC1
#define ADC_SENSE_R_INST_IRQHandler                              ADC1_IRQHandler
#define ADC_SENSE_R_INST_INT_IRQN                                (ADC1_INT_IRQn)
#define ADC_SENSE_R_ADCMEM_0                                  DL_ADC12_MEM_IDX_0
#define ADC_SENSE_R_ADCMEM_0_REF                 DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_SENSE_R_ADCMEM_0_REF_VOLTAGE_V                                     3.3
#define ADC_SENSE_R_ADCMEM_1                                  DL_ADC12_MEM_IDX_1
#define ADC_SENSE_R_ADCMEM_1_REF                 DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_SENSE_R_ADCMEM_1_REF_VOLTAGE_V                                     3.3
#define ADC_SENSE_R_ADCMEM_2                                  DL_ADC12_MEM_IDX_2
#define ADC_SENSE_R_ADCMEM_2_REF                 DL_ADC12_REFERENCE_VOLTAGE_VDDA
#define ADC_SENSE_R_ADCMEM_2_REF_VOLTAGE_V                                     3.3
#define GPIO_ADC_SENSE_R_C4_PORT                                           GPIOB
#define GPIO_ADC_SENSE_R_C4_PIN                                   DL_GPIO_PIN_17
#define GPIO_ADC_SENSE_R_IOMUX_C4                                (IOMUX_PINCM43)
#define GPIO_ADC_SENSE_R_IOMUX_C4_FUNC            (IOMUX_PINCM43_PF_UNCONNECTED)
#define GPIO_ADC_SENSE_R_C5_PORT                                           GPIOB
#define GPIO_ADC_SENSE_R_C5_PIN                                   DL_GPIO_PIN_18
#define GPIO_ADC_SENSE_R_IOMUX_C5                                (IOMUX_PINCM44)
#define GPIO_ADC_SENSE_R_IOMUX_C5_FUNC            (IOMUX_PINCM44_PF_UNCONNECTED)
#define GPIO_ADC_SENSE_R_C6_PORT                                           GPIOB
#define GPIO_ADC_SENSE_R_C6_PIN                                   DL_GPIO_PIN_19
#define GPIO_ADC_SENSE_R_IOMUX_C6                                (IOMUX_PINCM45)
#define GPIO_ADC_SENSE_R_IOMUX_C6_FUNC            (IOMUX_PINCM45_PF_UNCONNECTED)



/* Port definition for Pin Group GPIO_NOTIFY */
#define GPIO_NOTIFY_PORT                                                 (GPIOA)

/* Defines for LED: GPIOA.14 with pinCMx 36 on package pin 29 */
#define GPIO_NOTIFY_LED_PIN                                     (DL_GPIO_PIN_14)
#define GPIO_NOTIFY_LED_IOMUX                                    (IOMUX_PINCM36)
/* Port definition for Pin Group GPIO_CONTROL */
#define GPIO_CONTROL_PORT                                                (GPIOA)

/* Defines for START: GPIOA.18 with pinCMx 40 on package pin 33 */
#define GPIO_CONTROL_START_PIN                                  (DL_GPIO_PIN_18)
#define GPIO_CONTROL_START_IOMUX                                 (IOMUX_PINCM40)
/* Port definition for Pin Group GPIO_BUZZER */
#define GPIO_BUZZER_PORT                                                 (GPIOB)

/* Defines for BUZZ: GPIOB.7 with pinCMx 24 on package pin 21 */
#define GPIO_BUZZER_BUZZ_PIN                                     (DL_GPIO_PIN_7)
#define GPIO_BUZZER_BUZZ_IOMUX                                   (IOMUX_PINCM24)
/* Port definition for Pin Group GPIO_MOTOR_DIR */
#define GPIO_MOTOR_DIR_PORT                                              (GPIOB)

/* Defines for LEFT_AIN1: GPIOB.2 with pinCMx 15 on package pin 14 */
#define GPIO_MOTOR_DIR_LEFT_AIN1_PIN                             (DL_GPIO_PIN_2)
#define GPIO_MOTOR_DIR_LEFT_AIN1_IOMUX                           (IOMUX_PINCM15)
/* Defines for LEFT_AIN2: GPIOB.3 with pinCMx 16 on package pin 15 */
#define GPIO_MOTOR_DIR_LEFT_AIN2_PIN                             (DL_GPIO_PIN_3)
#define GPIO_MOTOR_DIR_LEFT_AIN2_IOMUX                           (IOMUX_PINCM16)
/* Defines for RIGHT_BIN1: GPIOB.8 with pinCMx 25 on package pin 22 */
#define GPIO_MOTOR_DIR_RIGHT_BIN1_PIN                            (DL_GPIO_PIN_8)
#define GPIO_MOTOR_DIR_RIGHT_BIN1_IOMUX                          (IOMUX_PINCM25)
/* Defines for RIGHT_BIN2: GPIOB.9 with pinCMx 26 on package pin 23 */
#define GPIO_MOTOR_DIR_RIGHT_BIN2_PIN                            (DL_GPIO_PIN_9)
#define GPIO_MOTOR_DIR_RIGHT_BIN2_IOMUX                          (IOMUX_PINCM26)
/* Defines for STBY: GPIOB.6 with pinCMx 23 on package pin 20 */
#define GPIO_MOTOR_DIR_STBY_PIN                                  (DL_GPIO_PIN_6)
#define GPIO_MOTOR_DIR_STBY_IOMUX                                (IOMUX_PINCM23)


/* clang-format on */

void SYSCFG_DL_init(void);
void SYSCFG_DL_initPower(void);
void SYSCFG_DL_GPIO_init(void);
void SYSCFG_DL_SYSCTL_init(void);
void SYSCFG_DL_SYSCTL_CLK_init(void);
void SYSCFG_DL_PWM_MOTOR_init(void);
void SYSCFG_DL_I2C_INST_init(void);
void SYSCFG_DL_UART_DEBUG_init(void);
void SYSCFG_DL_ADC_SENSE_init(void);
void SYSCFG_DL_ADC_SENSE_R_init(void);


bool SYSCFG_DL_saveConfiguration(void);
bool SYSCFG_DL_restoreConfiguration(void);

#ifdef __cplusplus
}
#endif

#endif /* ti_msp_dl_config_h */
