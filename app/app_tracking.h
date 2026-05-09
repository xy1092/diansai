#ifndef APP_TRACKING_H
#define APP_TRACKING_H

#include <stdint.h>
#include "../bsp/bsp_adc.h"
#include "../middleware/pid.h"

typedef struct {
    uint16_t raw[BSP_ADC_CCD_COUNT];
    uint16_t contrast;
    uint16_t strength;
    float    bias;
    float    last_bias;
    uint8_t  on_line;
} TrackInfo_t;

void              App_Tracking_Init(PID_t *line_pid);
float             App_Tracking_LineControl(void);
const TrackInfo_t *App_Tracking_GetInfo(void);
uint8_t           App_Tracking_IsOnLine(void);
float             App_Tracking_GetLastBias(void);

#endif
