#include "bsp_oled.h"
#include "../pin_map.h"

/*
 * SSD1306 via I²C 框架。
 * 建议使用成熟的 u8g2/gMUI 开源库，此处仅给出接口桩。
 * 快速实现：移植一份 SSD1306 I2C 驱动，把 i2c_write_bytes 替换为本工程的 I2C API。
 */

#define I2C_WAIT_LIMIT 100000u

static uint8_t s_framebuf[1024];   /* 128×64 / 8 */

static int i2c_wait_idle(void)
{
    uint32_t timeout = I2C_WAIT_LIMIT;
    while ((DL_I2C_getControllerStatus(I2C_INST) &
            DL_I2C_CONTROLLER_STATUS_BUSY_BUS) != 0u) {
        if (timeout-- == 0u) {
            DL_I2C_resetControllerTransfer(I2C_INST);
            return -1;
        }
    }
    return 0;
}

static void ssd1306_cmd(uint8_t cmd)
{
    uint8_t buf[2] = { 0x00, cmd };
    DL_I2C_fillControllerTXFIFO(I2C_INST, buf, 2);
    DL_I2C_startControllerTransfer(I2C_INST, SSD1306_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 2);
    (void)i2c_wait_idle();
}

void BSP_OLED_Init(void)
{
    static const uint8_t init_seq[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00,
        0x40, 0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8,
        0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40,
        0xA4, 0xA6, 0xAF
    };
    for (uint32_t i = 0; i < sizeof(init_seq); ++i) ssd1306_cmd(init_seq[i]);
    BSP_OLED_Clear();
    BSP_OLED_Refresh();
}

void BSP_OLED_Clear(void)
{
    for (uint32_t i = 0; i < sizeof(s_framebuf); ++i) s_framebuf[i] = 0;
}

void BSP_OLED_ShowStr(uint8_t x, uint8_t y, const char *s)
{
    /* TODO: 6×8 字库绘制到 framebuf */
    (void)x; (void)y; (void)s;
}

void BSP_OLED_ShowNum(uint8_t x, uint8_t y, int32_t num, uint8_t len)
{
    char buf[12];
    int  i = 0;
    uint8_t neg = 0;
    if (num < 0) { neg = 1; num = -num; }
    do { buf[i++] = '0' + (num % 10); num /= 10; } while (num && i < (int)sizeof(buf) - 1);
    if (neg) buf[i++] = '-';
    while (i < len) buf[i++] = ' ';
    buf[i] = 0;
    /* reverse */
    for (int a = 0, b = i - 1; a < b; ++a, --b) { char t = buf[a]; buf[a] = buf[b]; buf[b] = t; }
    BSP_OLED_ShowStr(x, y, buf);
}

void BSP_OLED_ShowFloat(uint8_t x, uint8_t y, float v)
{
    int32_t iv = (int32_t)(v * 100.0f);
    BSP_OLED_ShowNum(x, y, iv, 6);
}

void BSP_OLED_Refresh(void)
{
    /* SSD1306 页寻址：把 framebuf 每 128 字节写到一页 */
    /* TODO: for (page = 0..7) 发送 0xB0|page、0x00、0x10 后连发 128 字节 data */
}
