#ifndef __LOG_H
#define __LOG_H

#include "main.h"

/* ------------------------------------------------------------------
 * 单条日志的最大字符数（不含结尾 '\0'）
 * ------------------------------------------------------------------
 * 取值依据（实测，不是估的）：
 *   · 最长的是 [DATA] 那行        = 90 字符
 *   · 十六进制每行 16 字节时       = 61 字符
 *   · 取 160 留了 70% 余量
 * ------------------------------------------------------------------ */
#define LOG_LINE_MAX     160

/* 十六进制打印时每行显示多少字节 */
#define LOG_HEX_COL      16

/**
  * @brief  初始化日志系统（创建互斥量）
  * @note   ★ 必须在 osKernelInitialize() 之后、
  *             且在任何任务开始使用 log_printf() 之前调用。
  *             本工程放在 main.c 的 USER CODE BEGIN RTOS_MUTEX 区。
  */
void log_init(void);

/**
  * @brief  线程安全的格式化输出（printf 的替代品）
  * @note   内部先把内容格式化到本地缓冲区，再独占 UART 一次性发送。
  *         不可在中断服务函数中调用（会在互斥量上阻塞）。
  */
void log_printf(const char *fmt, ...);

/**
  * @brief  以十六进制打印一段内存，每行 LOG_HEX_COL 字节
  * @param  tag  标题（如 "[PROTO] TX"）
  * @param  data 数据首地址
  * @param  len  字节数
  */
void log_hex(const char *tag, const uint8_t *data, uint16_t len);

#endif /* __LOG_H */