#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include "cmsis_os.h"

/* ==================================================================
 *  日志专用的互斥量
 * ================================================================== */
static osMutexId_t s_log_mutex = NULL;

static const osMutexAttr_t s_log_mutex_attr = {
    .name      = "logMutex",
    .attr_bits = osMutexPrioInherit,   /* 优先级继承：防止优先级反转 */
};

void log_init(void)
{
    s_log_mutex = osMutexNew(&s_log_mutex_attr);
    if (s_log_mutex == NULL) {
        /* 堆不够，创建失败 —— 这时候什么都不对劲了 */
        Error_Handler();
    }
}

/* ------------------------------------------------------------------
 *  内部函数：把一段【已经格式化好】的数据独占串口发出去
 * ------------------------------------------------------------------
 *  ★ 这是整个工程里【唯一】访问 USART1 的地方。
 *    所有日志都经过它 → 只要它加锁，就绝对不会有交织。
 * ------------------------------------------------------------------ */
static void log_transmit(const char *s, uint16_t len)
{
    if ((s == NULL) || (len == 0U)) {
        return;
    }

    /* 如果还没初始化（调度器启动前的早期代码），直接发，不加锁 */
    if (s_log_mutex != NULL) {
        osMutexAcquire(s_log_mutex, osWaitForever);
    }

    /* ----------------------------------------------------------------
     * ★ 关键：一次性发送 len 字节
     *   HAL_UART_Transmit 是阻塞的，它会把 len 字节全部搬进硬件
     *   才返回。在这期间不允许别的任务碰 UART（因为有锁）。
     * ---------------------------------------------------------------- */
    HAL_UART_Transmit(&huart1, (uint8_t *)s, len, 200U);

    if (s_log_mutex != NULL) {
        osMutexRelease(s_log_mutex);
    }
}

void log_printf(const char *fmt, ...)
{
    char    buf[LOG_LINE_MAX];
    va_list ap;
    int     n;

    /* ---- 阶段 1：格式化到【本地】缓冲区（不需要锁，不碰 UART）---- */
    va_start(ap, fmt);//把 `ap` 初始化为指向**第一个可变参数**
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);//结束遍历，把 `ap` 置为无效状态

    if (n < 0) {
        return;                        /* 格式化失败 */
    }
    if (n >= (int)sizeof(buf)) {
        n = (int)sizeof(buf) - 1;      /* 被截断了，只发能装下的部分 */
    }

    /* ---- 阶段 2：独占 UART 一次性发送 ---- */
    log_transmit(buf, (uint16_t)n);
}

void log_hex(const char *tag, const uint8_t *data, uint16_t len)
{
    char   line[LOG_LINE_MAX];
    int    pos;
    size_t remain;

    if ((data == NULL) || (len == 0U)) {
        return;
    }

    /* 标题行 */
    pos = snprintf(line, sizeof(line), "%s (%u bytes):\r\n",
                   tag, (unsigned)len);
    if (pos > 0) {
        log_transmit(line, (uint16_t)pos);
    }

    /* 数据行：每行 LOG_HEX_COL 字节
     *
     * ★ 为什么分行？
     *   150 字节 × 3 字符 = 450 字符，一行的缓冲区装不下。
     *   分成每行 16 字节后，每行约 61 字符，固定可预估。
     *   副作用：终端上看起来也更清楚。 */
    for (uint16_t off = 0; off < len; off += LOG_HEX_COL)
    {
        uint16_t cnt = (uint16_t)(len - off);
        if (cnt > LOG_HEX_COL) {
            cnt = LOG_HEX_COL;
        }

        pos = snprintf(line, sizeof(line), "  %04X:", (unsigned)off);

        for (uint16_t i = 0; i < cnt; i++)
        {
            remain = sizeof(line) - (size_t)pos;
            if (remain < 4U) {          /* 放不下了，本行到此为止 */
                break;
            }
            pos += snprintf(line + pos, remain, " %02X", data[off + i]);
        }

        remain = sizeof(line) - (size_t)pos;
        if (remain >= 3U) {
            pos += snprintf(line + pos, remain, "\r\n");
        }

        log_transmit(line, (uint16_t)pos);
    }
}