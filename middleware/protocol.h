#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* 板载 CH340 调试串口控制：'1'~'4' 选任务，'s'/'S' 启动。实体 MODE0/MODE1 拨码低电平有效。 */

typedef struct {
    int16_t center_x;
    int16_t center_y;
    uint8_t class_id;
    uint16_t area;
    uint32_t ts_ms;    /* 本地时间戳 */
} ProtoVision_t;

void           Proto_Init(void);
void           Proto_Poll(void);          /* 从 app 主循环调用，轮询板载按键 */

uint8_t        Proto_HasFreshVisionFrame(void);
ProtoVision_t  Proto_GetVision(void);

uint8_t        Proto_GetStartFlag(void);
uint8_t        Proto_GetMissionRequest(void);
uint8_t        Proto_GetStopFlag(void);

#endif
