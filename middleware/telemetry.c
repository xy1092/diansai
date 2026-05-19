#include "telemetry.h"
#include "../app/app_main.h"
#include "../bsp/bsp_uart.h"
#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CMD_BUF_LEN   64
#define DEFAULT_HZ    100u
#define MAX_PARAMS    16u
#define BLACKBOX_CAPACITY 512u
#define BLACKBOX_PERIOD_MS 100u

typedef struct {
    PID_t       *pid;
    const float *sp;
    const float *meas;
    uint8_t      valid;
} TelemBind_t;

typedef struct {
    const uint16_t *raw;
    uint8_t         raw_count;
    const uint16_t *contrast;
    const uint16_t *strength;
    const float    *bias;
    const uint8_t  *on_line;
    uint8_t         valid;
} TelemLineBind_t;

typedef struct {
    const uint8_t  *mission;
    const uint8_t  *state;
    const uint8_t  *loop;
    const uint8_t  *seg;
    const float    *x;
    const float    *y;
    const float    *theta;
    const float    *seg_dist;
    const uint32_t *mission_time;
    uint8_t         valid;
} TelemAppBind_t;

typedef struct {
    const char *name;
    float      *value;
    float       min_v;
    float       max_v;
    uint8_t     valid;
} TelemParam_t;

typedef struct __attribute__((packed)) {
    uint16_t mission_time_10ms;
    uint8_t  mission;
    uint8_t  state;
    uint8_t  loop;
    uint8_t  seg;
    int16_t  x_10;
    int16_t  y_10;
    int16_t  theta_deg_10;
    int16_t  seg_dist_10;
    int16_t  sp_l_1000;
    int16_t  meas_l_1000;
    int16_t  out_l;
    int16_t  sp_r_1000;
    int16_t  meas_r_1000;
    int16_t  out_r;
    int16_t  line_bias_1000;
    uint16_t contrast;
    uint16_t strength;
    uint8_t  on_line;
} BlackboxSample_t;

static TelemBind_t s_binds[TELEM_CH_MAX];
static const char *s_ch_name[TELEM_CH_MAX] = { "L", "R", "LINE", "ANG" };
static TelemLineBind_t s_line_bind;
static TelemAppBind_t s_app_bind;
static TelemParam_t s_params[MAX_PARAMS];
static BlackboxSample_t s_blackbox[BLACKBOX_CAPACITY];

static uint16_t s_period_ms = 10u;      /* 100 Hz */
static uint8_t  s_enabled   = 0u;
static uint8_t  s_raw_line_enabled = 0u;
static uint32_t s_last_emit_ms = 0u;
static uint8_t  s_emit_primed = 0u;
static uint16_t s_blackbox_wr = 0u;
static uint16_t s_blackbox_count = 0u;
static uint32_t s_blackbox_last_ms = 0u;
static uint8_t  s_blackbox_enabled = 1u;
static uint8_t  s_blackbox_primed = 0u;

static char     s_cmd_buf[CMD_BUF_LEN];
static uint8_t  s_cmd_idx   = 0u;
static uint8_t  s_in_telem_cmd = 0u;

/* ---------- 对外 API ---------- */

void Telem_Bind(TelemCh_t ch, PID_t *p, const float *sp, const float *meas)
{
    if (ch >= TELEM_CH_MAX) return;
    s_binds[ch].pid   = p;
    s_binds[ch].sp    = sp;
    s_binds[ch].meas  = meas;
    s_binds[ch].valid = (p && sp && meas) ? 1u : 0u;
}

void Telem_BindLineSensors(const uint16_t *raw, uint8_t raw_count,
                           const uint16_t *contrast, const uint16_t *strength,
                           const float *bias, const uint8_t *on_line)
{
    s_line_bind.raw = raw;
    s_line_bind.raw_count = raw_count;
    s_line_bind.contrast = contrast;
    s_line_bind.strength = strength;
    s_line_bind.bias = bias;
    s_line_bind.on_line = on_line;
    s_line_bind.valid = (raw && contrast && strength && bias && on_line && raw_count > 0u) ? 1u : 0u;
}

void Telem_Init(void)
{
    BSP_Uart_SetRxCallback(0, Telem_RxDispatchByte);   /* 0 = 调试 UART */
}

void Telem_SetRawLineEnabled(uint8_t on)
{
    s_raw_line_enabled = on ? 1u : 0u;
}

void Telem_BindAppState(const uint8_t *mission, const uint8_t *state,
                        const uint8_t *loop, const uint8_t *seg,
                        const float *x, const float *y, const float *theta,
                        const float *seg_dist, const uint32_t *mission_time)
{
    s_app_bind.mission = mission;
    s_app_bind.state = state;
    s_app_bind.loop = loop;
    s_app_bind.seg = seg;
    s_app_bind.x = x;
    s_app_bind.y = y;
    s_app_bind.theta = theta;
    s_app_bind.seg_dist = seg_dist;
    s_app_bind.mission_time = mission_time;
    s_app_bind.valid = (mission && state && loop && seg && x && y && theta &&
                        seg_dist && mission_time) ? 1u : 0u;
}

void Telem_RegisterParam(const char *name, float *value, float min_v, float max_v)
{
    if (!name || !value) return;
    for (uint8_t i = 0; i < MAX_PARAMS; ++i) {
        if (!s_params[i].valid) {
            s_params[i].name = name;
            s_params[i].value = value;
            s_params[i].min_v = min_v;
            s_params[i].max_v = max_v;
            s_params[i].valid = 1u;
            return;
        }
    }
}

static int16_t clamp_i16(float v)
{
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;
    return (int16_t)v;
}

static uint16_t clamp_u16_from_u32(uint32_t v)
{
    if (v > 65535u) return 65535u;
    return (uint16_t)v;
}

static void blackbox_clear(void)
{
    s_blackbox_wr = 0u;
    s_blackbox_count = 0u;
    s_blackbox_primed = 0u;
}

static void blackbox_capture(uint32_t ts_ms)
{
    if (!s_blackbox_enabled || !s_app_bind.valid) return;
    if (!s_blackbox_primed) {
        s_blackbox_primed = 1u;
        s_blackbox_last_ms = ts_ms;
    } else if ((uint32_t)(ts_ms - s_blackbox_last_ms) < BLACKBOX_PERIOD_MS) {
        return;
    } else {
        s_blackbox_last_ms = ts_ms;
    }

    BlackboxSample_t *s = &s_blackbox[s_blackbox_wr];
    PID_t *pid_l = s_binds[TELEM_CH_L].valid ? s_binds[TELEM_CH_L].pid : 0;
    PID_t *pid_r = s_binds[TELEM_CH_R].valid ? s_binds[TELEM_CH_R].pid : 0;

    s->mission_time_10ms = clamp_u16_from_u32(*s_app_bind.mission_time / 10u);
    s->mission = *s_app_bind.mission;
    s->state = *s_app_bind.state;
    s->loop = *s_app_bind.loop;
    s->seg = *s_app_bind.seg;
    s->x_10 = clamp_i16(*s_app_bind.x * 10.0f);
    s->y_10 = clamp_i16(*s_app_bind.y * 10.0f);
    s->theta_deg_10 = clamp_i16(*s_app_bind.theta * 572.957795f);
    s->seg_dist_10 = clamp_i16(*s_app_bind.seg_dist * 10.0f);
    s->sp_l_1000 = pid_l ? clamp_i16(pid_l->last_setpoint * 1000.0f) : 0;
    s->meas_l_1000 = pid_l ? clamp_i16(pid_l->last_measure * 1000.0f) : 0;
    s->out_l = pid_l ? clamp_i16(pid_l->last_output) : 0;
    s->sp_r_1000 = pid_r ? clamp_i16(pid_r->last_setpoint * 1000.0f) : 0;
    s->meas_r_1000 = pid_r ? clamp_i16(pid_r->last_measure * 1000.0f) : 0;
    s->out_r = pid_r ? clamp_i16(pid_r->last_output) : 0;
    s->line_bias_1000 = (s_line_bind.valid && s_line_bind.bias) ?
                        clamp_i16(*s_line_bind.bias * 1000.0f) : 0;
    s->contrast = (s_line_bind.valid && s_line_bind.contrast) ? *s_line_bind.contrast : 0u;
    s->strength = (s_line_bind.valid && s_line_bind.strength) ? *s_line_bind.strength : 0u;
    s->on_line = (s_line_bind.valid && s_line_bind.on_line) ? *s_line_bind.on_line : 0u;

    s_blackbox_wr++;
    if (s_blackbox_wr >= BLACKBOX_CAPACITY) s_blackbox_wr = 0u;
    if (s_blackbox_count < BLACKBOX_CAPACITY) s_blackbox_count++;
}

static void blackbox_dump(void)
{
    uint16_t start = (s_blackbox_count == BLACKBOX_CAPACITY) ? s_blackbox_wr : 0u;

    BSP_Uart_Printf("$BHEAD,%u,%u,%u,%u\r\n",
                    (unsigned)s_blackbox_count,
                    (unsigned)BLACKBOX_CAPACITY,
                    (unsigned)BLACKBOX_PERIOD_MS,
                    (unsigned)s_blackbox_enabled);

    for (uint16_t i = 0u; i < s_blackbox_count; ++i) {
        uint16_t idx = (uint16_t)(start + i);
        if (idx >= BLACKBOX_CAPACITY) idx -= BLACKBOX_CAPACITY;
        const BlackboxSample_t *s = &s_blackbox[idx];

        BSP_Uart_Printf("$B,%u,%u,%u,%u,%u,%u,%.1f,%.1f,%.1f,%.1f,%.3f,%.3f,%d,%.3f,%.3f,%d,%.3f,%u,%u,%u\r\n",
                        (unsigned)i,
                        (unsigned)s->mission_time_10ms * 10u,
                        (unsigned)s->mission,
                        (unsigned)s->state,
                        (unsigned)s->loop,
                        (unsigned)s->seg,
                        (float)s->x_10 / 10.0f,
                        (float)s->y_10 / 10.0f,
                        (float)s->theta_deg_10 / 10.0f,
                        (float)s->seg_dist_10 / 10.0f,
                        (float)s->sp_l_1000 / 1000.0f,
                        (float)s->meas_l_1000 / 1000.0f,
                        (int)s->out_l,
                        (float)s->sp_r_1000 / 1000.0f,
                        (float)s->meas_r_1000 / 1000.0f,
                        (int)s->out_r,
                        (float)s->line_bias_1000 / 1000.0f,
                        (unsigned)s->contrast,
                        (unsigned)s->strength,
                        (unsigned)s->on_line);
    }

    BSP_Uart_Printf("$BEND,%u\r\n", (unsigned)s_blackbox_count);
}

void Telem_Tick(uint32_t ts_ms)
{
    blackbox_capture(ts_ms);

    if (!s_enabled) return;
    if (!s_emit_primed) {
        s_emit_primed = 1u;
        s_last_emit_ms = ts_ms;
    } else if ((uint32_t)(ts_ms - s_last_emit_ms) < s_period_ms) {
        return;
    } else {
        s_last_emit_ms = ts_ms;
    }

    for (uint8_t i = 0; i < TELEM_CH_MAX; ++i) {
        if (!s_binds[i].valid) continue;
        PID_t *p = s_binds[i].pid;

        BSP_Uart_Printf("$P,%lu,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\r\n",
                        (unsigned long)ts_ms, s_ch_name[i],
                        p->last_setpoint, p->last_measure, p->last_output,
                        p->last_error, p->integral, p->last_deriv,
                        p->last_p, p->last_i, p->last_d, p->last_raw_output);
    }

    if (s_raw_line_enabled && s_line_bind.valid && s_line_bind.raw_count >= 7u) {
        BSP_Uart_Printf("$L,%lu,%.3f,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\r\n",
                        (unsigned long)ts_ms,
                        *s_line_bind.bias,
                        (unsigned)*s_line_bind.contrast,
                        (unsigned)*s_line_bind.strength,
                        (unsigned)*s_line_bind.on_line,
                        (unsigned)s_line_bind.raw[0],
                        (unsigned)s_line_bind.raw[1],
                        (unsigned)s_line_bind.raw[2],
                        (unsigned)s_line_bind.raw[3],
                        (unsigned)s_line_bind.raw[4],
                        (unsigned)s_line_bind.raw[5],
                        (unsigned)s_line_bind.raw[6]);
    }

    if (s_app_bind.valid) {
        BSP_Uart_Printf("$A,%lu,%u,%u,%u,%u,%.2f,%.2f,%.2f,%.2f,%lu\r\n",
                        (unsigned long)ts_ms,
                        (unsigned)*s_app_bind.mission,
                        (unsigned)*s_app_bind.state,
                        (unsigned)*s_app_bind.loop,
                        (unsigned)*s_app_bind.seg,
                        *s_app_bind.x,
                        *s_app_bind.y,
                        *s_app_bind.theta * 57.2957795f,
                        *s_app_bind.seg_dist,
                        (unsigned long)*s_app_bind.mission_time);
    }
}

/* ---------- 命令解析 ---------- */

static TelemCh_t ch_of(const char *s)
{
    if (!strcmp(s, "L"))    return TELEM_CH_L;
    if (!strcmp(s, "R"))    return TELEM_CH_R;
    if (!strcmp(s, "LINE")) return TELEM_CH_LINE;
    if (!strcmp(s, "ANG"))  return TELEM_CH_ANG;
    return TELEM_CH_MAX;
}

static void apply_set(const char *ch_s, const char *gain, float v)
{
    TelemCh_t ch = ch_of(ch_s);
    if (ch >= TELEM_CH_MAX || !s_binds[ch].valid) {
        BSP_Uart_Printf("$ERR,set,bad_ch\r\n"); return;
    }
    PID_t *p = s_binds[ch].pid;
    if      (!strcmp(gain, "KP")) p->kp = v;
    else if (!strcmp(gain, "KI")) p->ki = v;
    else if (!strcmp(gain, "KD")) p->kd = v;
    else { BSP_Uart_Printf("$ERR,set,bad_gain\r\n"); return; }
    BSP_Uart_Printf("$OK,set,%s,%s,%.3f\r\n", ch_s, gain, v);
}

static void dump_gains(void)
{
    for (uint8_t i = 0; i < TELEM_CH_MAX; ++i) {
        if (!s_binds[i].valid) continue;
        PID_t *p = s_binds[i].pid;
        BSP_Uart_Printf("$G,%s,%.3f,%.3f,%.3f\r\n",
                        s_ch_name[i], p->kp, p->ki, p->kd);
    }
    BSP_Uart_Printf("$CFG,rawline,%u\r\n", (unsigned)s_raw_line_enabled);
    for (uint8_t i = 0; i < MAX_PARAMS; ++i) {
        if (!s_params[i].valid) continue;
        BSP_Uart_Printf("$C,%s,%.3f,%.3f,%.3f\r\n",
                        s_params[i].name, *s_params[i].value,
                        s_params[i].min_v, s_params[i].max_v);
    }
}

static void apply_cfg_set(const char *name, float v)
{
    for (uint8_t i = 0; i < MAX_PARAMS; ++i) {
        if (!s_params[i].valid || strcmp(name, s_params[i].name) != 0) continue;
        if (v < s_params[i].min_v || v > s_params[i].max_v) {
            BSP_Uart_Printf("$ERR,cfgset,range,%s,%.3f,%.3f\r\n",
                            name, s_params[i].min_v, s_params[i].max_v);
            return;
        }
        *s_params[i].value = v;
        BSP_Uart_Printf("$OK,cfgset,%s,%.3f\r\n", name, v);
        return;
    }
    BSP_Uart_Printf("$ERR,cfgset,bad_name\r\n");
}

static uint8_t parse_float_arg(const char *s, float *out)
{
    char *end = 0;
    float v;

    if (!s || !out) return 0u;
    v = strtof(s, &end);
    if (end == s) return 0u;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end != 0) return 0u;
    *out = v;
    return 1u;
}

static uint8_t parse_set_args(char *args, char **ch, char **gain, float *value)
{
    char *p1;
    char *p2;

    if (!args || !ch || !gain || !value) return 0u;
    p1 = strchr(args, ',');
    if (!p1) return 0u;
    *p1++ = 0;
    p2 = strchr(p1, ',');
    if (!p2) return 0u;
    *p2++ = 0;

    *ch = args;
    *gain = p1;
    return parse_float_arg(p2, value);
}

static uint8_t parse_cfg_args(char *args, char **name, float *value)
{
    char *p;

    if (!args || !name || !value) return 0u;
    p = strchr(args, ',');
    if (!p) return 0u;
    *p++ = 0;

    *name = args;
    return parse_float_arg(p, value);
}

static void handle_line(char *line)
{
    /* 去掉末尾 \r\n */
    char *p = line;
    while (*p && *p != '\r' && *p != '\n') ++p;
    *p = 0;
    if (line[0] != '$') return;

    if (!strncmp(line, "$SET,", 5)) {
        char *ch;
        char *gain;
        float v;
        if (parse_set_args(line + 5, &ch, &gain, &v)) {
            apply_set(ch, gain, v);
        } else {
            BSP_Uart_Printf("$ERR,set,parse\r\n");
        }
    } else if (!strncmp(line, "$CFGSET,", 8)) {
        char *name;
        float v;
        if (parse_cfg_args(line + 8, &name, &v)) {
            apply_cfg_set(name, v);
        } else {
            BSP_Uart_Printf("$ERR,cfgset,parse\r\n");
        }
    } else if (!strncmp(line, "$RATE,", 6)) {
        int hz = atoi(line + 6);
        if (hz >= 1 && hz <= 500) {
            s_period_ms = (uint16_t)(1000 / hz);
            if (s_period_ms == 0u) s_period_ms = 1u;
            BSP_Uart_Printf("$OK,rate,%d\r\n", hz);
        }
    } else if (!strncmp(line, "$RAWLINE,", 9)) {
        int on = atoi(line + 9);
        Telem_SetRawLineEnabled(on ? 1u : 0u);
        BSP_Uart_Printf("$OK,rawline,%u\r\n", (unsigned)s_raw_line_enabled);
    } else if (!strcmp(line, "$RST")) {
        for (uint8_t i = 0; i < TELEM_CH_MAX; ++i) {
            if (s_binds[i].valid) PID_Reset(s_binds[i].pid);
        }
        BSP_Uart_Printf("$OK,rst\r\n");
    } else if (!strcmp(line, "$PAUSE")) {
        s_enabled = 0; BSP_Uart_Printf("$OK,pause\r\n");
    } else if (!strcmp(line, "$RESUME")) {
        s_enabled = 1; BSP_Uart_Printf("$OK,resume\r\n");
    } else if (!strcmp(line, "$DUMP")) {
        dump_gains();
    } else if (!strcmp(line, "$LOGDUMP")) {
        blackbox_dump();
    } else if (!strcmp(line, "$LOGCLR")) {
        blackbox_clear();
        BSP_Uart_Printf("$OK,logclr\r\n");
    } else if (!strncmp(line, "$LOG,", 5)) {
        int on = atoi(line + 5);
        s_blackbox_enabled = on ? 1u : 0u;
        BSP_Uart_Printf("$OK,log,%u\r\n", (unsigned)s_blackbox_enabled);
    } else if (!strcmp(line, "$DUTYSTOP")) {
        App_ClearDebugDuty();
        BSP_Uart_Printf("$OK,dutystop\r\n");
    } else if (!strncmp(line, "$DUTY,", 6)) {
        /* $DUTY,<left>,<right>[,<hold_ms>]  — left/right 0..1000, hold 60..60000 */
        char *p = line + 6;
        char *p2 = strchr(p, ',');
        if (!p2) {
            BSP_Uart_Printf("$ERR,duty,parse\r\n");
        } else {
            *p2++ = 0;
            char *p3 = strchr(p2, ',');
            uint32_t hold = 0u;
            if (p3) { *p3++ = 0; hold = (uint32_t)atoi(p3); }
            int left  = atoi(p);
            int right = atoi(p2);
            if (App_GetState() == STATE_RUN) {
                BSP_Uart_Printf("$ERR,duty,running\r\n");
            } else {
                App_SetDebugDuty((int16_t)left, (int16_t)right, hold);
                BSP_Uart_Printf("$OK,duty,%d,%d,%u\r\n", left, right, (unsigned)hold);
            }
        }
    } else {
        BSP_Uart_Printf("$ERR,unknown\r\n");
    }
}

void Telem_OnRxByte(uint8_t b)
{
    if (b == '\n') {
        s_cmd_buf[s_cmd_idx] = 0;
        handle_line(s_cmd_buf);
        s_cmd_idx = 0;
        return;
    }
    if (b == '\r') return;
    if (s_cmd_idx < CMD_BUF_LEN - 1) s_cmd_buf[s_cmd_idx++] = (char)b;
    else s_cmd_idx = 0;                /* 行太长丢弃 */
}

void Telem_RxDispatchByte(uint8_t b)
{
    if (s_in_telem_cmd) {
        Telem_OnRxByte(b);
        if (b == '\n') s_in_telem_cmd = 0u;
        return;
    }

    if (b == '$') {
        s_in_telem_cmd = 1u;
        Telem_OnRxByte(b);
        return;
    }

    Proto_OnRxByte(b);
}
