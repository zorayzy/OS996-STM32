#ifndef __BSP_PROTOCOL_H
#define __BSP_PROTOCOL_H

#include "main.h"

/* ==================================================================
 *  STM32 <-> ESP32 串口帧协议
 * ==================================================================
 *
 *  为什么不用"直接发 JSON 字符串 + \n 结尾"？
 *  ------------------------------------------------------------------
 *    ① 没有边界标志：如果丢字节，会把两帧拼成一帧，解析出错也不知道
 *    ② payload 里不能出现 \n（JSON 里不会，但二进制数据会）
 *    ③ 没有校验：收到错的数据也无法发现
 *
 *  帧格式（固定 6 + LEN 字节）
 *  ------------------------------------------------------------------
 *    字节偏移   长度   字段      说明
 *    --------   ----   -------   -----------------------------------
 *      0         1     HEAD1     0xAA
 *      1         1     HEAD2     0x55
 *      2         1     CMD       指令类型
 *      3         1     LEN       载荷长度（0 ~ 128）
 *      4        LEN    PAYLOAD   载荷（本项目中是 JSON 字符串）
 *    4+LEN       2     CRC16     校验（小端：先低字节，后高字节）
 *
 *    总长度 = 6 + LEN
 *
 *  CRC 计算范围：[CMD] [LEN] [PAYLOAD...]
 *  （不包含帧头，也不包含 CRC 自身 —— 帧头只用来找边界）
 * ================================================================== */

/* ---------------- 帧字段定义 ---------------- */
#define FRAME_HEAD1          0xAAU
#define FRAME_HEAD2          0x55U
#define FRAME_HEAD_SIZE      2U      /* 帧头 2 字节 */
#define FRAME_OVERHEAD       6U      /* 帧头2 + CMD1 + LEN1 + CRC2 */

/* ------------------------------------------------------------------
 * 载荷上限
 * ------------------------------------------------------------------
 * 取值依据（实测，不是估的）：
 *   传感器 JSON 约 142 字节：
 *     {"t":25.57,"h":72.55,"pir":0,"gas":1.137,"raw":702,"flt":706,
 *      "warm":1,"seq":0,"ok":1,"ierr":0,"cerr":0}
 *
 * ★ 最初设 128 太小 —— 犯了"按整数估长度"的错。
 *   浮点数（25.57 / 72.55 / 1.137）每个占 5 个字符，
 *   比按整数估的多出十几字节。
 *
 * 现在设 200，留 40% 余量：以后加字段不用再改这里。
 * 代价：帧缓冲区从 134 变成 206 字节（多 72 B RAM，可忽略）
 * ------------------------------------------------------------------ */
#define FRAME_MAX_PAYLOAD    200U
#define FRAME_MAX_TOTAL      (FRAME_OVERHEAD + FRAME_MAX_PAYLOAD)

/* ---------------- 指令类型 ---------------- */
#define CMD_SENSOR_UP        0x01U   /* STM32 -> ESP32：传感器数据（JSON） */
#define CMD_CTRL_DOWN        0x02U   /* ESP32 -> STM32：控制指令（JSON） */
#define CMD_ACK              0x03U   /* 双向：确认 */
#define CMD_HEARTBEAT        0x04U   /* 双向：心跳 */

/* ---------------- API ---------------- */

/**
  * @brief  计算 CRC16-MODBUS 校验值
  * @param  data 数据首地址
  * @param  len  数据长度（字节）
  * @retval 16 位校验值
  * @note   生成多项式 0xA001（0x8005 的反转形式），初值 0xFFFF，
  *         输入/输出均不反转。这是嵌入式领域最通用的 CRC16 变种。
  */
uint16_t proto_crc16(const uint8_t *data, uint16_t len);

/**
  * @brief  组帧并发送（阻塞式）
  * @param  cmd      指令类型（CMD_xxx）
  * @param  payload  载荷数据（可为 NULL，此时 len 必须为 0）
  * @param  len      载荷长度，必须 <= FRAME_MAX_PAYLOAD
  * @param  timeout  HAL 发送超时（ms）
  * @retval 0 成功；-1 参数错误；-2 串口发送失败
  * @note   调试期会同时把帧内容以十六进制打印到 USART1，方便对照。
  */
int proto_send_frame(uint8_t cmd, const uint8_t *payload, uint8_t len,
                     uint32_t timeout);

/**
  * @brief  发送一帧 JSON 载荷（自动计算长度）
  * @param  cmd  CMD_SENSOR_UP 或 CMD_ACK 等
  * @param  json 以 '\0' 结尾的 JSON 字符串
  * @retval 同 proto_send_frame()
  */
int proto_send_json(uint8_t cmd, const char *json, uint32_t timeout);

#endif /* __BSP_PROTOCOL_H */
