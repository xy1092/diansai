#include "bsp_imu.h"
#include "../pin_map.h"
#include <math.h>

/* MPU6050 寄存器 */
#define MPU_PWR_MGMT_1      0x6B
#define MPU_SMPLRT_DIV      0x19
#define MPU_CONFIG          0x1A
#define MPU_GYRO_CONFIG     0x1B
#define MPU_ACCEL_CONFIG    0x1C
#define MPU_ACCEL_XOUT_H    0x3B

static float s_pitch = 0, s_roll = 0, s_yaw = 0;
static float s_gx = 0,    s_gy = 0,   s_gz = 0;
static float s_ax = 0,    s_ay = 0,   s_az = 0;

static int i2c_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    DL_I2C_fillControllerTXFIFO(I2C_INST, buf, 2);
    DL_I2C_startControllerTransfer(I2C_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 2);
    while (DL_I2C_getControllerStatus(I2C_INST) & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {}
    return 0;
}

static int i2c_read_regs(uint8_t reg, uint8_t *buf, uint8_t len)
{
    DL_I2C_fillControllerTXFIFO(I2C_INST, &reg, 1);
    DL_I2C_startControllerTransfer(I2C_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1);
    while (DL_I2C_getControllerStatus(I2C_INST) & DL_I2C_CONTROLLER_STATUS_BUSY_BUS) {}

    DL_I2C_startControllerTransfer(I2C_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);
    uint8_t i = 0;
    while (i < len) {
        if (!DL_I2C_isControllerRXFIFOEmpty(I2C_INST)) {
            buf[i++] = DL_I2C_receiveControllerData(I2C_INST);
        }
    }
    return 0;
}

void BSP_IMU_Init(void)
{
    i2c_write_reg(MPU_PWR_MGMT_1,   0x00);   /* wakeup */
    i2c_write_reg(MPU_SMPLRT_DIV,   0x07);   /* 1 kHz / (1+7) = 125Hz */
    i2c_write_reg(MPU_CONFIG,       0x03);   /* DLPF 44Hz */
    i2c_write_reg(MPU_GYRO_CONFIG,  0x18);   /* ±2000 dps */
    i2c_write_reg(MPU_ACCEL_CONFIG, 0x10);   /* ±8 g */
}

/* 简化版互补滤波，实际可替换为 Mahony/Madgwick */
void BSP_IMU_Update(float *pitch, float *roll, float *yaw)
{
    uint8_t raw[14];
    if (i2c_read_regs(MPU_ACCEL_XOUT_H, raw, 14) != 0) return;

    int16_t ax_r = (raw[0] << 8) | raw[1];
    int16_t ay_r = (raw[2] << 8) | raw[3];
    int16_t az_r = (raw[4] << 8) | raw[5];
    int16_t gx_r = (raw[8] << 8) | raw[9];
    int16_t gy_r = (raw[10] << 8) | raw[11];
    int16_t gz_r = (raw[12] << 8) | raw[13];

    /* LSB/g = 4096 @ ±8g;  LSB/(°/s) = 16.4 @ ±2000 */
    s_ax = (float)ax_r / 4096.0f;
    s_ay = (float)ay_r / 4096.0f;
    s_az = (float)az_r / 4096.0f;
    s_gx = (float)gx_r / 16.4f;
    s_gy = (float)gy_r / 16.4f;
    s_gz = (float)gz_r / 16.4f;

    float acc_pitch = atan2f(s_ay, s_az) * 57.2957795f;
    float acc_roll  = atan2f(-s_ax, sqrtf(s_ay * s_ay + s_az * s_az)) * 57.2957795f;

    const float dt    = 0.005f;
    const float alpha = 0.98f;
    s_pitch = alpha * (s_pitch + s_gx * dt) + (1.0f - alpha) * acc_pitch;
    s_roll  = alpha * (s_roll  + s_gy * dt) + (1.0f - alpha) * acc_roll;
    s_yaw  += s_gz * dt;

    if (pitch) *pitch = s_pitch;
    if (roll)  *roll  = s_roll;
    if (yaw)   *yaw   = s_yaw;
}

void BSP_IMU_GetGyro(float *gx, float *gy, float *gz)
{ if (gx) *gx = s_gx; if (gy) *gy = s_gy; if (gz) *gz = s_gz; }

void BSP_IMU_GetAccel(float *ax, float *ay, float *az)
{ if (ax) *ax = s_ax; if (ay) *ay = s_ay; if (az) *az = s_az; }
