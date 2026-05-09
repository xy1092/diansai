#include "app_tracking.h"
#include "../bsp/bsp_adc.h"
#include "../middleware/filter.h"

#define TRACK_CONTRAST_TH   120u
#define TRACK_STRENGTH_TH   260u

static PID_t     *s_pid = 0;
static TrackInfo_t s_info;
static LPF1_t     s_bias_lpf;
static const int8_t kTrackWeights[BSP_ADC_CCD_COUNT] = { -6, -4, -2, 0, 2, 4, 6 };

static void update_track_info(void)
{
    uint16_t raw[BSP_ADC_CCD_COUNT];
    uint16_t min_v, max_v;
    uint32_t strength = 0;
    int32_t  weighted = 0;

    BSP_Adc_GetCCD(raw);

    min_v = raw[0];
    max_v = raw[0];
    for (uint8_t i = 0; i < BSP_ADC_CCD_COUNT; ++i) {
        s_info.raw[i] = raw[i];
        if (raw[i] < min_v) min_v = raw[i];
        if (raw[i] > max_v) max_v = raw[i];
    }

    for (uint8_t i = 0; i < BSP_ADC_CCD_COUNT; ++i) {
        uint16_t v = (uint16_t)(raw[i] - min_v);
        strength += v;
        weighted += (int32_t)kTrackWeights[i] * (int32_t)v;
    }

    s_info.contrast = (uint16_t)(max_v - min_v);
    s_info.strength = (uint16_t)strength;
    s_info.on_line  = (s_info.contrast >= TRACK_CONTRAST_TH &&
                      s_info.strength >= TRACK_STRENGTH_TH) ? 1u : 0u;

    if (s_info.on_line && strength != 0u) {
        s_info.bias = LPF1_Update(&s_bias_lpf, (float)weighted / (float)strength);
        s_info.last_bias = s_info.bias;
    } else {
        s_info.bias = s_info.last_bias;
    }
}

void App_Tracking_Init(PID_t *line_pid)
{
    s_pid = line_pid;
    s_info.bias = 0.0f;
    s_info.last_bias = 0.0f;
    s_info.on_line = 0u;
    LPF1_Init(&s_bias_lpf, 0.35f);
}

float App_Tracking_LineControl(void)
{
    update_track_info();

    if (!s_pid) return 0.0f;
    if (!s_info.on_line) {
        float hold = s_info.last_bias;
        if (hold > 0.0f) return 220.0f;
        if (hold < 0.0f) return -220.0f;
        return 0.0f;
    }

    return PID_Update(s_pid, 0.0f, s_info.bias);
}

const TrackInfo_t *App_Tracking_GetInfo(void)
{
    return &s_info;
}

uint8_t App_Tracking_IsOnLine(void)
{
    return s_info.on_line;
}

float App_Tracking_GetLastBias(void)
{
    return s_info.last_bias;
}
