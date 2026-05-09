#include "telemetry.h"
#include "../bsp/bsp_uart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CMD_BUF_LEN   64
#define DEFAULT_HZ    100u

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

static TelemBind_t s_binds[TELEM_CH_MAX];
static const char *s_ch_name[TELEM_CH_MAX] = { "L", "R", "LINE", "ANG" };
static TelemLineBind_t s_line_bind;

static uint16_t s_period_ms = 10u;      /* 100 Hz */
static uint8_t  s_enabled   = 1u;
static uint8_t  s_raw_line_enabled = 0u;
static uint32_t s_last_emit_ms = 0u;
static uint8_t  s_emit_primed = 0u;

static char     s_cmd_buf[CMD_BUF_LEN];
static uint8_t  s_cmd_idx   = 0u;

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
    BSP_Uart_SetRxCallback(0, Telem_OnRxByte);   /* 0 = 调试 UART */
}

void Telem_SetRawLineEnabled(uint8_t on)
{
    s_raw_line_enabled = on ? 1u : 0u;
}

void Telem_Tick(uint32_t ts_ms)
{
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
}

static void handle_line(char *line)
{
    /* 去掉末尾 \r\n */
    char *p = line;
    while (*p && *p != '\r' && *p != '\n') ++p;
    *p = 0;
    if (line[0] != '$') return;

    if (!strncmp(line, "$SET,", 5)) {
        char ch[8], gain[4];
        float v;
        if (sscanf(line + 5, "%7[^,],%3[^,],%f", ch, gain, &v) == 3) {
            apply_set(ch, gain, v);
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
