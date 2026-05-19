#include "app_main.h"
#include "app_tracking.h"
#include "../bsp/bsp_motor.h"
#ifndef NUEDC_NO_ENCODER
#include "../bsp/bsp_encoder.h"
#endif
#include "../bsp/bsp_imu.h"
#include "../bsp/bsp_uart.h"
#include "../bsp/bsp_adc.h"
#include "../bsp/bsp_oled.h"
#ifdef NUEDC_USE_ULTRASONIC
#include "../bsp/bsp_ultrasonic.h"
#endif
#ifdef NUEDC_USE_K230
#include "../bsp/bsp_k230.h"
#endif
#include "../middleware/pid.h"
#include "../middleware/protocol.h"
#include "../middleware/telemetry.h"
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define APP_DEFAULT_MISSION         MISSION_H1

#define ODOM_PULSES_PER_CM          20.0f
#define ODOM_WHEEL_BASE_CM          13.5f
#define OPEN_LOOP_CM_PER_PWM_MS     0.00022f

#define SPEED_OUT_LIMIT             1000.0f
/* Encoder targets are pulses per 1 ms control tick.
 * With 20 pulse/cm, 0.50 pulse/ms is about 25 cm/s.
 */
#define SPEED_MIN_PULSE             0.18f
#define SPEED_MAX_PULSE             0.45f
#define ARC_CRUISE_PULSE            0.26f
#define SEEK_LINE_PULSE             0.20f
#define LINE_SEEK_DIFF              0.16f
#define FORWARD_MIN_PULSE           0.03f
#define POSE_TURN_RATIO             0.55f

#define POSE_BLEND_CM               20.0f
#define POSE_POS_TOL_CM             4.0f
#define POSE_HEADING_TOL_RAD        0.18f
#define POSE_KP_DIST                0.010f
#define POSE_KP_HEAD                0.18f
#define POSE_KP_FINAL               0.12f

#define ARC_RADIUS_CM               40.0f
#define ARC_LEN_CM                  125.7f
#define ARC_DONE_MIN_CM             95.0f
#define ARC_DONE_TOL_CM             10.0f
#define TRACK_WIDTH_CM              100.0f
#define TRACK_HEIGHT_CM             (2.0f * ARC_RADIUS_CM)
#define ARC_IMU_HOLD_KP             0.20f

#define SPEED_PID_KP                180.0f
#define SPEED_PID_KI                1.0f
#define SPEED_PID_KD                0.0f
#define LINE_PID_KP                 1.5f
#define LINE_PID_KI                 0.0f
#define LINE_PID_KD                 0.0f
#define ANG_PID_KP                  0.0f
#define ANG_PID_KI                  0.0f
#define ANG_PID_KD                  0.0f

typedef struct {
    float x;
    float y;
    float theta;
} Pose2D_t;

typedef enum {
    SEG_STOP = 0,
    SEG_GOTO_POSE,
    SEG_ARC_TRACK,
} SegmentType_t;

typedef enum {
    ARC_RIGHT_DOWN = 0,
    ARC_RIGHT_UP,
    ARC_LEFT_UP,
} ArcMode_t;

typedef struct {
    SegmentType_t type;
    float x;
    float y;
    float theta;
    float length_cm;
    ArcMode_t arc_mode;
    char point_name;
} RouteSegment_t;

static AppState_t    s_state = STATE_IDLE;
static AppMission_t  s_mission = APP_DEFAULT_MISSION;

static float         cfg_speed_min_pulse = SPEED_MIN_PULSE;
static float         cfg_speed_max_pulse = SPEED_MAX_PULSE;
static float         cfg_arc_cruise_pulse = ARC_CRUISE_PULSE;
static float         cfg_seek_line_pulse = SEEK_LINE_PULSE;
static float         cfg_line_seek_diff = LINE_SEEK_DIFF;
static float         cfg_pose_kp_dist = POSE_KP_DIST;
static float         cfg_pose_kp_head = POSE_KP_HEAD;
static float         cfg_pose_kp_final = POSE_KP_FINAL;
static float         cfg_arc_done_min_cm = ARC_DONE_MIN_CM;
static float         cfg_arc_done_tol_cm = ARC_DONE_TOL_CM;
static float         cfg_arc_imu_hold_kp = ARC_IMU_HOLD_KP;
static float         cfg_dead_zone_L     = 0.0f;   /* PWM feedforward (0-1000) */
static float         cfg_dead_zone_R     = 0.0f;
static float         cfg_pose_snap       = 1.0f;   /* >=0.5 → snap pose at every segment start */

static PID_t         pid_spd_L, pid_spd_R;
static PID_t         pid_line;
static PID_t         pid_ang;

static volatile uint8_t flag_1ms = 0;
static volatile uint8_t flag_5ms = 0;
static volatile uint8_t flag_50ms = 0;

static Pose2D_t      s_pose;
static float         s_pitch = 0.0f;
static float         s_roll = 0.0f;
static float         s_yaw_deg = 0.0f;
static float         s_arc_hold_yaw_deg = 0.0f;
static float         s_target_speed_L = 0.0f;
static float         s_target_speed_R = 0.0f;
static float         s_meas_speed_L   = 0.0f;   /* telemetry: 实测脉冲/控制周期 */
static float         s_meas_speed_R   = 0.0f;
static float         s_line_setpoint  = 0.0f;   /* telemetry: 循迹环目标恒为 0 */
static float         s_line_bias      = 0.0f;   /* telemetry: 当前偏差快照 */
static float         s_ang_setpoint_deg = 0.0f; /* telemetry: 航向目标 */
static float         s_ang_meas_deg     = 0.0f; /* telemetry: 当前航向 */
static float         s_seg_dist_cm = 0.0f;
static uint32_t      s_mission_time_ms = 0;
static uint32_t      s_notify_until_ms = 0;
static uint8_t       s_segment_started = 0;
static uint8_t       s_loop_goal = 1;
static uint8_t       s_loop_index = 0;
static uint8_t       s_seg_index = 0;
static uint8_t       s_line_lost_cnt = 0;

static uint8_t       s_debug_pwm_active = 0u;
static int16_t       s_debug_pwm_L      = 0;
static int16_t       s_debug_pwm_R      = 0;
static uint32_t      s_debug_pwm_deadline_ms = 0u;

extern volatile uint32_t g_tick_ms;

static const Pose2D_t kPointA = {  0.0f, TRACK_HEIGHT_CM, 0.0f };
static const Pose2D_t kPointC = { TRACK_WIDTH_CM, 0.0f, 0.0f };

static const RouteSegment_t kMissionH1[] = {
    { SEG_GOTO_POSE, TRACK_WIDTH_CM, TRACK_HEIGHT_CM, 0.0f,   0.0f, ARC_RIGHT_DOWN, 'B' },
    { SEG_STOP,      0.0f,           0.0f,            0.0f,   0.0f, ARC_RIGHT_DOWN, 0   },
};

static const RouteSegment_t kMissionH2[] = {
    { SEG_GOTO_POSE, TRACK_WIDTH_CM, TRACK_HEIGHT_CM, 0.0f,   0.0f, ARC_RIGHT_DOWN, 'B' },
    { SEG_ARC_TRACK, TRACK_WIDTH_CM, 0.0f,            (float)M_PI, ARC_LEN_CM, ARC_RIGHT_DOWN, 'C' },
    { SEG_GOTO_POSE, 0.0f,           0.0f,            (float)M_PI, 0.0f, ARC_RIGHT_DOWN, 'D' },
    { SEG_ARC_TRACK, 0.0f,           TRACK_HEIGHT_CM, 0.0f,   ARC_LEN_CM, ARC_LEFT_UP, 'A' },
    { SEG_STOP,      0.0f,           0.0f,            0.0f,   0.0f, ARC_RIGHT_DOWN, 0   },
};

static const RouteSegment_t kMissionH3Loop[] = {
    { SEG_GOTO_POSE, TRACK_WIDTH_CM, 0.0f,            0.0f,   0.0f, ARC_RIGHT_DOWN, 'C' },
    { SEG_ARC_TRACK, TRACK_WIDTH_CM, TRACK_HEIGHT_CM, (float)M_PI, ARC_LEN_CM, ARC_RIGHT_UP, 'B' },
    { SEG_GOTO_POSE, 0.0f,           0.0f,            (float)M_PI, 0.0f, ARC_RIGHT_DOWN, 'D' },
    { SEG_ARC_TRACK, 0.0f,           TRACK_HEIGHT_CM, 0.0f,   ARC_LEN_CM, ARC_LEFT_UP, 'A' },
    { SEG_STOP,      0.0f,           0.0f,            0.0f,   0.0f, ARC_RIGHT_DOWN, 0   },
};

__attribute__((weak)) void BSP_Notify_Set(uint8_t on)
{
    (void)on;
}

static float wrap_pi(float x)
{
    while (x > (float)M_PI)  x -= 2.0f * (float)M_PI;
    while (x < -(float)M_PI) x += 2.0f * (float)M_PI;
    return x;
}

static float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static void announce_event(const char *msg)
{
    BSP_Uart_Printf("\a%s\r\n", msg);
    s_notify_until_ms = g_tick_ms + 160u;
}

static void announce_point(char point)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "PASS %c", point);
    announce_event(buf);
}

static const RouteSegment_t *get_route(uint8_t *count, uint8_t *loop_goal, Pose2D_t *start_pose)
{
    if (s_mission == MISSION_H1) {
        *count = (uint8_t)(sizeof(kMissionH1) / sizeof(kMissionH1[0]));
        *loop_goal = 1u;
        *start_pose = kPointA;
        start_pose->theta = 0.0f;
        return kMissionH1;
    }
    if (s_mission == MISSION_H2) {
        *count = (uint8_t)(sizeof(kMissionH2) / sizeof(kMissionH2[0]));
        *loop_goal = 1u;
        *start_pose = kPointA;
        start_pose->theta = 0.0f;
        return kMissionH2;
    }

    *count = (uint8_t)(sizeof(kMissionH3Loop) / sizeof(kMissionH3Loop[0]));
    *loop_goal = (s_mission == MISSION_H4) ? 4u : 1u;
    *start_pose = kPointA;
    start_pose->theta = atan2f(kPointC.y - kPointA.y, kPointC.x - kPointA.x);
    return kMissionH3Loop;
}

static const RouteSegment_t *current_segment(uint8_t *count)
{
    uint8_t loops;
    static Pose2D_t ignored;
    const RouteSegment_t *route = get_route(count, &loops, &ignored);
    (void)loops;
    return &route[s_seg_index];
}

static void reset_motion_targets(void)
{
    s_target_speed_L = 0.0f;
    s_target_speed_R = 0.0f;
}

static void update_ang_telemetry(float target_rad, float measure_rad, float output)
{
    float err = wrap_pi(target_rad - measure_rad);

    s_ang_setpoint_deg = target_rad * 57.2957795f;
    s_ang_meas_deg = measure_rad * 57.2957795f;
    pid_ang.last_setpoint = s_ang_setpoint_deg;
    pid_ang.last_measure = s_ang_meas_deg;
    pid_ang.last_error = err * 57.2957795f;
    pid_ang.last_p = cfg_pose_kp_head * err;
    pid_ang.last_i = 0.0f;
    pid_ang.last_d = 0.0f;
    pid_ang.last_deriv = 0.0f;
    pid_ang.last_raw_output = output;
    pid_ang.last_output = output;
}

static void reset_odometry(Pose2D_t pose)
{
    s_pose = pose;
    s_seg_dist_cm = 0.0f;
#ifndef NUEDC_NO_ENCODER
    BSP_Encoder_Reset(0);
    BSP_Encoder_Reset(1);
#endif
}

static void begin_segment(void)
{
    s_segment_started = 1u;
    s_seg_dist_cm = 0.0f;
    s_line_lost_cnt = 0u;
    s_arc_hold_yaw_deg = s_yaw_deg;
    PID_Reset(&pid_line);

    /* Snap pose to the known waypoint at segment boundary so theta does not
     * drift across H3/H4 laps. Segment 0 → mission start pose; otherwise →
     * the previous segment's target pose.
     */
    if (cfg_pose_snap >= 0.5f) {
        uint8_t count;
        uint8_t loops_unused;
        Pose2D_t start_pose;
        const RouteSegment_t *route = get_route(&count, &loops_unused, &start_pose);
        if (s_seg_index == 0u) {
            s_pose = start_pose;
        } else if (s_seg_index <= count) {
            const RouteSegment_t *prev = &route[s_seg_index - 1u];
            s_pose.x = prev->x;
            s_pose.y = prev->y;
            s_pose.theta = prev->theta;
        }
    }
}

static void finish_mission(void)
{
    s_state = STATE_STOP;
    reset_motion_targets();
    BSP_Motor_Brake(0);
    BSP_Motor_Brake(1);
    announce_event("STOP");
}

static void stop_to_ready(void)
{
    s_state = STATE_READY;
    reset_motion_targets();
    BSP_Motor_Brake(0);
    BSP_Motor_Brake(1);
    s_segment_started = 0u;
    announce_event("STOP");
}

static void next_segment(void)
{
    uint8_t count;
    uint8_t loops;
    static Pose2D_t ignored;
    const RouteSegment_t *route = get_route(&count, &loops, &ignored);
    const RouteSegment_t *seg = &route[s_seg_index];
    (void)loops;

    if (seg->point_name != 0) announce_point(seg->point_name);

    s_seg_index++;
    s_segment_started = 0u;

    if (s_seg_index >= count || route[s_seg_index].type == SEG_STOP) {
        s_loop_index++;
        if (s_loop_index >= s_loop_goal) {
            finish_mission();
            return;
        }
        s_seg_index = 0u;
    }
}

static float pose_speed_cmd(float dist_cm, float heading_err, float final_err)
{
    float speed = cfg_pose_kp_dist * dist_cm + cfg_speed_min_pulse;
    if (dist_cm > 70.0f) speed = cfg_speed_max_pulse;
    speed = clampf(speed, cfg_speed_min_pulse, cfg_speed_max_pulse);

    if (fabsf(heading_err) > 0.75f) speed *= 0.55f;
    if (fabsf(final_err) > 0.50f && dist_cm < POSE_BLEND_CM) speed *= 0.75f;
    return speed;
}

static void control_goto_pose(const RouteSegment_t *seg)
{
    float dx = seg->x - s_pose.x;
    float dy = seg->y - s_pose.y;
    float dist = sqrtf(dx * dx + dy * dy);
    float path_heading = atan2f(dy, dx);
    float final_err = wrap_pi(seg->theta - s_pose.theta);
    float blend = clampf((POSE_BLEND_CM - dist) / POSE_BLEND_CM, 0.0f, 1.0f);
    float ref_heading = wrap_pi(path_heading + blend * wrap_pi(seg->theta - path_heading));
    float heading_err = wrap_pi(ref_heading - s_pose.theta);
    float turn = cfg_pose_kp_head * heading_err + cfg_pose_kp_final * blend * final_err;
    float speed = pose_speed_cmd(dist, heading_err, final_err);
    float max_turn = POSE_TURN_RATIO * speed;

    turn = clampf(turn, -max_turn, max_turn);

    update_ang_telemetry(ref_heading, s_pose.theta, turn);

    s_target_speed_L = clampf(speed - turn, FORWARD_MIN_PULSE, SPEED_OUT_LIMIT);
    s_target_speed_R = clampf(speed + turn, FORWARD_MIN_PULSE, SPEED_OUT_LIMIT);

    if (dist < POSE_POS_TOL_CM && fabsf(final_err) < POSE_HEADING_TOL_RAD) {
        next_segment();
    }
}

static void control_arc_track(const RouteSegment_t *seg)
{
    float diff = App_Tracking_LineControl();
    float base = cfg_arc_cruise_pulse;

    if (!App_Tracking_IsOnLine()) {
        float sign = (seg->arc_mode == ARC_RIGHT_DOWN || seg->arc_mode == ARC_LEFT_UP) ? -1.0f : 1.0f;
        float yaw_err = s_arc_hold_yaw_deg - s_yaw_deg;
        while (yaw_err > 180.0f) yaw_err -= 360.0f;
        while (yaw_err < -180.0f) yaw_err += 360.0f;
        if (App_Tracking_GetLastBias() > 0.05f) sign = 1.0f;
        if (App_Tracking_GetLastBias() < -0.05f) sign = -1.0f;
        diff = sign * cfg_line_seek_diff + cfg_arc_imu_hold_kp * (yaw_err / 57.2957795f);
        update_ang_telemetry(s_arc_hold_yaw_deg / 57.2957795f, s_yaw_deg / 57.2957795f, diff);
        base = cfg_seek_line_pulse;
        s_line_lost_cnt++;
    } else {
        s_arc_hold_yaw_deg = s_yaw_deg;
        update_ang_telemetry(s_pose.theta, s_pose.theta, diff);
        s_line_lost_cnt = 0u;
    }

    s_target_speed_L = clampf(base - diff, FORWARD_MIN_PULSE, SPEED_OUT_LIMIT);
    s_target_speed_R = clampf(base + diff, FORWARD_MIN_PULSE, SPEED_OUT_LIMIT);

    if (s_seg_dist_cm >= (seg->length_cm - cfg_arc_done_tol_cm)) {
        if (!App_Tracking_IsOnLine() || s_seg_dist_cm >= (seg->length_cm + cfg_arc_done_tol_cm)) {
            next_segment();
            return;
        }
    }

    if (s_seg_dist_cm >= cfg_arc_done_min_cm && s_line_lost_cnt >= 3u) {
        next_segment();
    }
}

static void control_outer(void)
{
    uint8_t count;
    const RouteSegment_t *seg;

    if (s_state != STATE_RUN) {
        reset_motion_targets();
        return;
    }

    seg = current_segment(&count);
    (void)count;

    if (!s_segment_started) begin_segment();

    if (seg->type == SEG_GOTO_POSE) {
        control_goto_pose(seg);
    } else if (seg->type == SEG_ARC_TRACK) {
        control_arc_track(seg);
    } else {
        finish_mission();
    }
}

static void control_speed_inner(void)
{
#ifdef NUEDC_NO_ENCODER
    int16_t uL = 0;
    int16_t uR = 0;

    if (s_state == STATE_RUN) {
        uL = (int16_t)clampf(s_target_speed_L, -SPEED_OUT_LIMIT, SPEED_OUT_LIMIT);
        uR = (int16_t)clampf(s_target_speed_R, -SPEED_OUT_LIMIT, SPEED_OUT_LIMIT);
    }

    float dsL = (float)uL * OPEN_LOOP_CM_PER_PWM_MS;
    float dsR = (float)uR * OPEN_LOOP_CM_PER_PWM_MS;
    float ds = 0.5f * (dsL + dsR);
    float dtheta = (dsR - dsL) / ODOM_WHEEL_BASE_CM;
    float mid = s_pose.theta + 0.5f * dtheta;

    s_pose.x += ds * cosf(mid);
    s_pose.y += ds * sinf(mid);
    s_pose.theta = wrap_pi(s_pose.theta + dtheta);
    s_seg_dist_cm += fabsf(ds);

    s_meas_speed_L = (float)uL;
    s_meas_speed_R = (float)uR;

    BSP_Motor_SetDuty(0, uL);
    BSP_Motor_SetDuty(1, uR);
#else
    int32_t dL = BSP_Encoder_GetDelta(0);
    int32_t dR = BSP_Encoder_GetDelta(1);
    int16_t uL;
    int16_t uR;

    float dsL = (float)dL / ODOM_PULSES_PER_CM;
    float dsR = (float)dR / ODOM_PULSES_PER_CM;
    float ds = 0.5f * (dsL + dsR);
    float dtheta = (dsR - dsL) / ODOM_WHEEL_BASE_CM;
    float mid = s_pose.theta + 0.5f * dtheta;

    s_pose.x += ds * cosf(mid);
    s_pose.y += ds * sinf(mid);
    s_pose.theta = wrap_pi(s_pose.theta + dtheta);
    s_seg_dist_cm += fabsf(ds);

    s_meas_speed_L = (float)dL;
    s_meas_speed_R = (float)dR;

    uL = (int16_t)PID_Update(&pid_spd_L, s_target_speed_L, (float)dL);
    uR = (int16_t)PID_Update(&pid_spd_R, s_target_speed_R, (float)dR);

    if (s_state == STATE_RUN) {
        /* Dead-zone feedforward: only when wheel is meant to move forward. */
        if (s_target_speed_L > FORWARD_MIN_PULSE && uL > 0) {
            int32_t ff = (int32_t)uL + (int32_t)cfg_dead_zone_L;
            if (ff > (int32_t)SPEED_OUT_LIMIT) ff = (int32_t)SPEED_OUT_LIMIT;
            uL = (int16_t)ff;
        }
        if (s_target_speed_R > FORWARD_MIN_PULSE && uR > 0) {
            int32_t ff = (int32_t)uR + (int32_t)cfg_dead_zone_R;
            if (ff > (int32_t)SPEED_OUT_LIMIT) ff = (int32_t)SPEED_OUT_LIMIT;
            uR = (int16_t)ff;
        }
        BSP_Motor_SetDuty(0, uL);
        BSP_Motor_SetDuty(1, uR);
    } else if (s_debug_pwm_active) {
        if (g_tick_ms >= s_debug_pwm_deadline_ms) {
            s_debug_pwm_active = 0u;
            BSP_Motor_SetDuty(0, 0);
            BSP_Motor_SetDuty(1, 0);
        } else {
            BSP_Motor_SetDuty(0, s_debug_pwm_L);
            BSP_Motor_SetDuty(1, s_debug_pwm_R);
        }
    } else {
        BSP_Motor_SetDuty(0, 0);
        BSP_Motor_SetDuty(1, 0);
    }
#endif
}

static void poll_fast_inputs(void)
{
#ifndef NUEDC_NO_ENCODER
    BSP_Encoder_Poll();
#endif
    BSP_IMU_Update(&s_pitch, &s_roll, &s_yaw_deg);
}

static void update_ui(void)
{
    const TrackInfo_t *trk = App_Tracking_GetInfo();
    BSP_OLED_Clear();
    BSP_OLED_ShowStr(0, 0, "H");
    BSP_OLED_ShowNum(8, 0, (int32_t)s_mission, 1);
    BSP_OLED_ShowNum(24, 0, (int32_t)s_state, 1);
    BSP_OLED_ShowNum(40, 0, (int32_t)s_loop_index + 1, 1);
    BSP_OLED_ShowNum(56, 0, (int32_t)s_seg_index, 1);
    BSP_OLED_ShowNum(0, 2, (int32_t)s_pose.x, 4);
    BSP_OLED_ShowNum(40, 2, (int32_t)s_pose.y, 4);
    BSP_OLED_ShowNum(80, 2, (int32_t)(s_pose.theta * 57.3f), 4);
    BSP_OLED_ShowNum(0, 4, (int32_t)trk->contrast, 4);
    BSP_OLED_ShowNum(40, 4, (int32_t)(trk->bias * 100.0f), 4);
    BSP_OLED_ShowNum(80, 4, (int32_t)(s_mission_time_ms / 1000u), 4);
    BSP_OLED_ShowNum(0, 6, (int32_t)trk->raw[0], 1);
    BSP_OLED_ShowNum(16, 6, (int32_t)trk->raw[1], 1);
    BSP_OLED_ShowNum(32, 6, (int32_t)trk->raw[2], 1);
    BSP_OLED_ShowNum(48, 6, (int32_t)trk->raw[3], 1);
    BSP_OLED_ShowNum(64, 6, (int32_t)trk->raw[4], 1);
    BSP_OLED_ShowNum(80, 6, (int32_t)trk->raw[5], 1);
    BSP_OLED_ShowNum(96, 6, (int32_t)trk->raw[6], 1);
    BSP_OLED_Refresh();
}

static void service_ready_state(void)
{
    uint8_t req = Proto_GetMissionRequest();

    if (req >= MISSION_H1 && req <= MISSION_H4) {
        s_mission = (AppMission_t)req;
    }

    if (Proto_GetStartFlag()) {
        uint8_t count;
        Pose2D_t start_pose;
        get_route(&count, &s_loop_goal, &start_pose);
        (void)count;
        s_loop_index = 0u;
        s_seg_index = 0u;
        s_mission_time_ms = 0u;
        reset_odometry(start_pose);
        announce_point('A');
        s_state = STATE_RUN;
        s_segment_started = 0u;
    }
}

void App_Init(void)
{
    BSP_Motor_Init();
#ifndef NUEDC_NO_ENCODER
    BSP_Encoder_Init();
#endif
    BSP_IMU_Init();
    BSP_Uart_Init();
    BSP_Adc_Init();
    BSP_OLED_Init();
#ifdef NUEDC_USE_ULTRASONIC
    BSP_Ultrasonic_Init();
#endif
#ifdef NUEDC_USE_K230
    BSP_K230_Init();
#endif

    PID_Init(&pid_spd_L, SPEED_PID_KP, SPEED_PID_KI, SPEED_PID_KD, -1000.0f, 1000.0f);
    PID_Init(&pid_spd_R, SPEED_PID_KP, SPEED_PID_KI, SPEED_PID_KD, -1000.0f, 1000.0f);
    PID_Init(&pid_line, LINE_PID_KP, LINE_PID_KI, LINE_PID_KD, -0.36f, 0.36f);
    PID_Init(&pid_ang, ANG_PID_KP, ANG_PID_KI, ANG_PID_KD, -1.0f, 1.0f);

    App_Tracking_Init(&pid_line);
    Proto_Init();

    Telem_Init();
    Telem_Bind(TELEM_CH_L,    &pid_spd_L, &s_target_speed_L, &s_meas_speed_L);
    Telem_Bind(TELEM_CH_R,    &pid_spd_R, &s_target_speed_R, &s_meas_speed_R);
    Telem_Bind(TELEM_CH_LINE, &pid_line,  &s_line_setpoint,  &s_line_bias);
    Telem_Bind(TELEM_CH_ANG,  &pid_ang,   &s_ang_setpoint_deg, &s_ang_meas_deg);
    Telem_BindAppState((const uint8_t *)&s_mission, (const uint8_t *)&s_state,
                       &s_loop_index, &s_seg_index,
                       &s_pose.x, &s_pose.y, &s_pose.theta,
                       &s_seg_dist_cm, &s_mission_time_ms);
    Telem_RegisterParam("spd_min", &cfg_speed_min_pulse, 0.0f, 1.5f);
    Telem_RegisterParam("spd_max", &cfg_speed_max_pulse, 0.0f, 2.0f);
    Telem_RegisterParam("arc_spd", &cfg_arc_cruise_pulse, 0.0f, 1.5f);
    Telem_RegisterParam("seek_spd", &cfg_seek_line_pulse, 0.0f, 1.0f);
    Telem_RegisterParam("seek_diff", &cfg_line_seek_diff, 0.0f, 0.8f);
    Telem_RegisterParam("pose_kd", &cfg_pose_kp_dist, 0.0f, 0.08f);
    Telem_RegisterParam("pose_kh", &cfg_pose_kp_head, 0.0f, 1.2f);
    Telem_RegisterParam("pose_kf", &cfg_pose_kp_final, 0.0f, 0.8f);
    Telem_RegisterParam("arc_min", &cfg_arc_done_min_cm, 0.0f, 160.0f);
    Telem_RegisterParam("arc_tol", &cfg_arc_done_tol_cm, 0.0f, 40.0f);
    Telem_RegisterParam("imu_hold", &cfg_arc_imu_hold_kp, 0.0f, 1.0f);
    Telem_RegisterParam("dead_L",   &cfg_dead_zone_L,    0.0f, 400.0f);
    Telem_RegisterParam("dead_R",   &cfg_dead_zone_R,    0.0f, 400.0f);
    Telem_RegisterParam("pose_snap",&cfg_pose_snap,      0.0f, 1.0f);
    {
        const TrackInfo_t *track = App_Tracking_GetInfo();
        Telem_BindLineSensors(track->raw, BSP_ADC_CCD_COUNT,
                              &track->contrast, &track->strength,
                              &track->bias, &track->on_line);
    }

    reset_odometry(kPointA);
    s_state = STATE_READY;
}

void App_Tick(void)
{
    static uint16_t cnt_5 = 0;
    static uint16_t cnt_50 = 0;

    flag_1ms = 1u;
    if (++cnt_5 >= 5u) {
        cnt_5 = 0u;
        flag_5ms = 1u;
    }
    if (++cnt_50 >= 50u) {
        cnt_50 = 0u;
        flag_50ms = 1u;
    }

    if (s_state == STATE_RUN) s_mission_time_ms++;

    if (g_tick_ms < s_notify_until_ms) BSP_Notify_Set(1u);
    else BSP_Notify_Set(0u);
}

void App_Loop(void)
{
    poll_fast_inputs();

    if (flag_1ms) {
        flag_1ms = 0u;
        control_speed_inner();
    }

    if (flag_5ms) {
        flag_5ms = 0u;
        control_outer();
    }

    if (flag_50ms) {
        flag_50ms = 0u;
        update_ui();
    }

    Proto_Poll();

    if (Proto_GetStopFlag()) {
        stop_to_ready();
    }

    s_line_bias = App_Tracking_GetInfo()->bias;
    if (s_state != STATE_RUN) update_ang_telemetry(s_pose.theta, s_pose.theta, 0.0f);
    Telem_Tick(g_tick_ms);

    if (s_state == STATE_READY || s_state == STATE_STOP) service_ready_state();
}

AppState_t App_GetState(void)
{
    return s_state;
}

AppMission_t App_GetMission(void)
{
    return s_mission;
}

void App_SetDebugDuty(int16_t left_pwm, int16_t right_pwm, uint32_t hold_ms)
{
    /* Only allow PWM override when no mission is running. */
    if (s_state == STATE_RUN) return;

    if (left_pwm  < 0)    left_pwm  = 0;
    if (left_pwm  > 1000) left_pwm  = 1000;
    if (right_pwm < 0)    right_pwm = 0;
    if (right_pwm > 1000) right_pwm = 1000;

    if (hold_ms == 0u)      hold_ms = 800u;
    if (hold_ms > 60000u)   hold_ms = 60000u;

    s_debug_pwm_L = left_pwm;
    s_debug_pwm_R = right_pwm;
    s_debug_pwm_deadline_ms = g_tick_ms + hold_ms;
    s_debug_pwm_active = 1u;
}

void App_ClearDebugDuty(void)
{
    s_debug_pwm_active = 0u;
    s_debug_pwm_L = 0;
    s_debug_pwm_R = 0;
    s_debug_pwm_deadline_ms = 0u;
}
