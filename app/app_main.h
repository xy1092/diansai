#ifndef APP_MAIN_H
#define APP_MAIN_H

#include <stdint.h>

typedef enum {
    STATE_IDLE = 0,
    STATE_READY,
    STATE_RUN,
    STATE_STOP,
    STATE_ERROR,
} AppState_t;

typedef enum {
    MISSION_H1 = 1,
    MISSION_H2,
    MISSION_H3,
    MISSION_H4,
} AppMission_t;

void         App_Init(void);
void         App_Tick(void);     /* 1 ms, from SysTick ISR */
void         App_Loop(void);     /* from main while(1) */
AppState_t   App_GetState(void);
AppMission_t App_GetMission(void);

#endif
