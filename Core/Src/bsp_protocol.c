#include "bsp_protocol.h"
#include <stdio.h>
#include <string.h>
#include "log.h"

/* ==================================================================
 *  调试开关
 * ================================================================== */
#define PROTO_DEBUG_HEX     1     /* 1 = 把发出的帧打印成十六进制 */

/* 发送缓冲区：最大帧长 */
static uint8_t s_frame[FRAME_MAX_TOTAL];

/* ==================================================================
 *  CRC16-MODBUS
 * ==================================================================
 *  算法说明（这是面试常考点）：
 *
 *  1) 初值 0xFFFF（不是 0）—— 保证"开头有若干个 0x00"这种情况也能被检出
 *  2) 多项式用 0xA001 而不是 0x8005
 *     —— 因为 0x8005 是"正向"表示（最高位对齐），
 *        0xA001 是它的"反转"形式（最低位对齐），
 *        逆推时用 0xA001 才能配合"右移"操作，省一次镜像翻转
 *  3) 每处理完一个字节，把 crc 和 0xFF 异或
 *     —— 这一步叫"输出反转"（output reflection），
 *        MODBUS 变种不做这一操作，但很多库会做；这里采用不反转的版本
 *  4) 逐位处理而不是查表
 *     —— 查表法快 8 倍但要占 512 字节 Flash；
 *        我们的帧只有几十字节、每 2 秒发一次，
 *        逐位法完全够用，省下的 Flash 更有价值
 * ================================================================== */
uint16_t proto_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i];

        for (uint8_t bit = 0; bit < 8; bit++)
        {
            if (crc & 0x0001U)
            {
                crc = (crc >> 1) ^ 0xA001U;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* ==================================================================
 *  组帧 + 发送
 * ================================================================== */
int proto_send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len,
                     uint32_t timeout)
{
    /* ---- 参数检查 ---- */
    if (len > FRAME_MAX_PAYLOAD)
    {
        return -1;
    }
    if ((payload == NULL) && (len != 0U))
    {
        return -1;
    }

    /* ---- 组装帧 ---- */
    s_frame[0] = FRAME_HEAD1;
    s_frame[1] = FRAME_HEAD2;
    s_frame[2] = cmd;
    s_frame[3] = len;

    if (len > 0U)
    {
        memcpy(&s_frame[4], payload, len);
    }

    /* ---- 计算 CRC：范围是 [CMD][LEN][PAYLOAD] ---- */
    uint16_t crc = proto_crc16(&s_frame[2], (uint16_t)(len + 2U));

    /* 小端写入：先低字节 */
    s_frame[4U + len]      = (uint8_t)(crc & 0xFFU);
    s_frame[4U + len + 1U] = (uint8_t)((crc >> 8) & 0xFFU);

    uint16_t total = (uint16_t)(FRAME_OVERHEAD + len);

#if PROTO_DEBUG_HEX
    /* 用 log_hex：内部会分行、加锁、一次性发送 */
    log_hex("[PROTO] TX", s_frame, total);
#endif

    /* ---- 发送 ---- */
    if (HAL_UART_Transmit(&huart2, s_frame, total, timeout) != HAL_OK)
    {
        log_printf("[PROTO] UART2 transmit FAILED\r\n");
        return -2;
    }

    return 0;
}

/* ==================================================================
 *  便捷接口：直接发 JSON 字符串
 * ================================================================== */
int proto_send_json(uint8_t cmd, const char *json, uint32_t timeout)
{
    if (json == NULL)
    {
        return -1;
    }

    size_t n = strlen(json);
    if (n > FRAME_MAX_PAYLOAD)
    {
        log_printf("[PROTO] JSON too long (%u > %u)\r\n",
               (unsigned)n, (unsigned)FRAME_MAX_PAYLOAD);
        return -1;
    }

    return proto_send_frame(cmd, (const uint8_t *)json, (uint8_t)n, timeout);
}
