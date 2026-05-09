#ifndef BSP_OLED_H
#define BSP_OLED_H

#include <stdint.h>

void BSP_OLED_Init(void);
void BSP_OLED_Clear(void);
void BSP_OLED_ShowStr(uint8_t x, uint8_t y, const char *s);
void BSP_OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len);
void BSP_OLED_ShowFloat(uint8_t x, uint8_t y, float v);
void BSP_OLED_Refresh(void);

#endif
