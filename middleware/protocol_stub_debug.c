#include "protocol.h"

extern volatile uint32_t g_tick_ms;

volatile uint8_t g_debug_proto_autostart = 1u;
volatile uint8_t g_debug_proto_default_mission = 1u;

static ProtoVision_t s_vision;
static uint8_t s_vision_fresh = 0u;
static uint8_t s_start_flag = 0u;
static uint8_t s_mission_req = 0u;
static uint8_t s_bootstrap_sent = 0u;

void Proto_Init(void)
{
    s_vision.center_x = 0;
    s_vision.center_y = 0;
    s_vision.class_id = 0u;
    s_vision.area = 0u;
    s_vision.ts_ms = 0u;
    s_vision_fresh = 0u;
    s_start_flag = 0u;
    s_mission_req = 0u;
    s_bootstrap_sent = 0u;
}

void Proto_Poll(void)
{
    if (!s_bootstrap_sent && g_debug_proto_autostart && g_tick_ms >= 50u) {
        s_bootstrap_sent = 1u;
        s_start_flag = 1u;
        if (g_debug_proto_default_mission >= 1u && g_debug_proto_default_mission <= 4u) {
            s_mission_req = g_debug_proto_default_mission;
        }
    }
}

uint8_t Proto_HasFreshVisionFrame(void)
{
    if (s_vision_fresh) {
        s_vision_fresh = 0u;
        return 1u;
    }
    return 0u;
}

ProtoVision_t Proto_GetVision(void)
{
    return s_vision;
}

uint8_t Proto_GetStartFlag(void)
{
    if (s_start_flag) {
        s_start_flag = 0u;
        return 1u;
    }
    return 0u;
}

uint8_t Proto_GetMissionRequest(void)
{
    uint8_t req = s_mission_req;
    s_mission_req = 0u;
    return req;
}
