#include "bsp_gxht30.h"
#include "cmsis_os.h"
/* ------------------------------------------------------------------
 * GXHT30 / SHT30 命令表
 * ------------------------------------------------------------------
 * 数据手册里的"单次测量"命令，由"时钟拉伸"和"重复性"两个字段组合：
 *
 *   0x2C06 = 高重复性 + 不拉伸   ★ 最常用
 *   0x2C0D = 中重复性 + 不拉伸
 *   0x2C10 = 低重复性 + 不拉伸
 *
 * 重复性越高 → 越准，但测量时间越长（约 15ms / 6ms / 4ms）
 * ------------------------------------------------------------------ */
#define GXHT30_I2C_ADDR      (0x44 << 1)   /* 7位地址 0x44，HAL 要 8 位，所以左移 */
#define GXHT30_CMD_MEAS_HIGH 0x2C06        /* 高重复性单次测量 */

/* 测量耗时（高重复性典型 15ms），给足余量 */
#define GXHT30_MEAS_DELAY_MS 20

/**
  * @brief  GXHT30/SHT30 专用的 CRC-8 校验
  * @param  data  待校验数据首地址
  * @param  len   字节数
  * @retval 计算出的 CRC 值
  * @note   参数来自数据手册：
  *           多项式   = 0x31  (即 x^8 + x^5 + x^4 + 1)
  *           初始值   = 0xFF
  *           输入反射 = 无
  *           输出异或 = 无
  */
static uint8_t gxht30_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;                 /* 初始值 */

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];                 /* 先把字节异或进去 */

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)             /* 最高位是 1 */
            {
                crc = (uint8_t)((crc << 1) ^ 0x31);   /* 左移后异或多项式 */
            }
            else
            {
                crc = (uint8_t)(crc << 1);            /* 只左移 */
            }
        }
    }

    return crc;
}

/**
  * @brief  读取温湿度
  * @param  temp  输出：温度，单位 ℃
  * @param  humi  输出：相对湿度，单位 %RH
  * @retval GXHT30_OK / GXHT30_ERR_I2C / GXHT30_ERR_CRC
  */
int gxht30_read(float *temp, float *humi)
{
    uint8_t  buf[2];
    uint8_t  raw[6];
    uint16_t raw_t, raw_h;

    /* ---- 1) 发送测量命令 ---- */
    buf[0] = (uint8_t)(GXHT30_CMD_MEAS_HIGH >> 8);    /* 高字节 0x2C */
    buf[1] = (uint8_t)(GXHT30_CMD_MEAS_HIGH & 0xFF);  /* 低字节 0x06 */

    if (HAL_I2C_Master_Transmit(&hi2c1, GXHT30_I2C_ADDR, buf, 2, 100) != HAL_OK)
    {
        return GXHT30_ERR_I2C;
    }

    /* ---- 2) 等测量完成 ---- */
    osDelay(GXHT30_MEAS_DELAY_MS);

    /* ---- 3) 读回 6 字节 ---- */
    /* 注意：地址参数和 Transmit 一样，【不要】 | 1。HAL 内部会自动置 R/W 位 */
    if (HAL_I2C_Master_Receive(&hi2c1, GXHT30_I2C_ADDR, raw, 6, 100) != HAL_OK)
    {
        return GXHT30_ERR_I2C;
    }

    /* ---- 4) CRC 校验 ---- */
    /* raw[0..1] 是温度原始值，raw[2] 是它的 CRC */
    if (gxht30_crc8(&raw[0], 2) != raw[2]) return GXHT30_ERR_CRC;
    /* raw[3..4] 是湿度原始值，raw[5] 是它的 CRC */
    if (gxht30_crc8(&raw[3], 2) != raw[5]) return GXHT30_ERR_CRC;

    /* ---- 5) 拼装原始值（★ 大端！高字节在前）---- */
    raw_t = (uint16_t)((raw[0] << 8) | raw[1]);
    raw_h = (uint16_t)((raw[3] << 8) | raw[4]);

    /* ---- 6) 按数据手册公式转换 ---- */
    *temp = -45.0f + 175.0f * ((float)raw_t / 65535.0f);
    *humi =          100.0f * ((float)raw_h / 65535.0f);

    return GXHT30_OK;
}