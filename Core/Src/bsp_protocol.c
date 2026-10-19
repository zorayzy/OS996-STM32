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
 *  3) 逐位处理而不是查表
 *     —— 查表法快 8 倍但要占 512 字节 Flash；
 *        我们的帧只有几十字节、每 2 秒发一次，
 *        逐位法完全够用，省下的 Flash 更有价值
 * ================================================================== */

/* 单字节的 CRC 更新（核心运算）
 *
 * ★ 为什么单独抽出来？
 *   因为 CRC 是【可增量计算】的 —— 处理完的字节不必保存，
 *   下个字节可以接着上一个 crc 值继续算。
 *   接收时我们是"一个字节一个字节"到的，没法一次性拿到整帧数据，
 *   有了这个函数就能边收边算，不用先把整帧缓存起来再算。
 *   这就是"流式计算"的思想。 */
static uint16_t crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte;

    for (uint8_t bit = 0; bit < 8; bit++)
    {
        if (crc & 0x0001U)
        {
            crc = (uint16_t)((crc >> 1) ^ 0xA001U);
        }
        else
        {
            crc >>= 1;
        }
    }

    return crc;
}

uint16_t proto_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFU;

    for (uint16_t i = 0; i < len; i++)
    {
        crc = crc16_update(crc, data[i]);
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


/* ==================================================================
 *  接收部分（阶段 2）
 * ================================================================== */

/* ------------------------------------------------------------------
 *  DMA 环形缓冲区
 * ------------------------------------------------------------------
 * 大小选 256 的考虑：
 *   · 一帧最大 206 字节（FRAME_MAX_TOTAL）
 *   · 256 能装一整帧还有 50 字节余量
 *   · 是 2 的幂 → "% 256" 会被编译器优化成 "& 255"，更快
 *
 * ★ volatile 必须加：
 *     DMA 在后台写这个数组，CPU 只读。不加的话编译器
 *     可能认为"这变量没人改过"，把读取优化掉。
 *
 * ★ 位置必须在普通 SRAM（.bss 默认就是）：
 *     STM32F407 的 CCM RAM（0x10000000）不经过总线矩阵，
 *     DMA 访问不到，放进去会静默失败。
 * ------------------------------------------------------------------ */
#define PROTO_RX_BUF_SIZE   256U

static volatile uint8_t s_rx_buf[PROTO_RX_BUF_SIZE];
static uint16_t         s_rx_consume_pos = 0;   /* 已消费到的下标 */

/* ------------------------------------------------------------------
 *  解析状态机
 * ------------------------------------------------------------------
 *  一个字节一个字节地推进，任何时候都知道"我期待下一个字节是什么"。
 *  好处：不需要先攒够一整帧再解析，内存占用固定且极小。
 * ------------------------------------------------------------------ */
typedef enum {
    RX_ST_HEAD1 = 0,    /* 等 0xAA */
    RX_ST_HEAD2,        /* 等 0x55 */
    RX_ST_CMD,          /* 收 CMD */
    RX_ST_LEN,          /* 收 LEN */
    RX_ST_PAYLOAD,      /* 收载荷 */
    RX_ST_CRC_L,        /* 收 CRC 低字节 */
    RX_ST_CRC_H,        /* 收 CRC 高字节 */
} rx_state_t;

static rx_state_t s_rx_state = RX_ST_HEAD1;
static uint8_t    s_rx_cmd;
static uint8_t    s_rx_len;
static uint16_t   s_rx_fill;                     /* 载荷已收字节数 */
static uint8_t    s_rx_payload[FRAME_MAX_PAYLOAD];
static uint16_t   s_rx_crc;                      /* 边收边算的 CRC */
static uint8_t    s_rx_crc_low;                  /* 收到的 CRC 低字节 */

static proto_rx_stats_t      s_rx_stats;
static proto_frame_handler_t s_rx_handler = NULL;

/* ------------------------------------------------------------------
 *  对外接口
 * ------------------------------------------------------------------ */
void proto_set_handler(proto_frame_handler_t h)
{
    s_rx_handler = h;
}

const proto_rx_stats_t *proto_get_rx_stats(void)
{
    return &s_rx_stats;
}

void proto_rx_init(void)
{
    memset(&s_rx_stats, 0, sizeof(s_rx_stats));

    s_rx_state       = RX_ST_HEAD1;
    s_rx_crc         = 0xFFFFU;
    s_rx_fill        = 0;
    s_rx_consume_pos = 0;

    /* ---- 启动 DMA 接收 ----
     *
     * 第 2 个参数强转为普通指针：去掉 volatile 限定（HAL 签名要 uint8_t*）
     * 第 3 个参数 = 缓冲区大小；由于 CubeMX 里 hdma_usart2_rx 配成
     *              DMA_CIRCULAR，DMA 填满后自动回到开头，永不停止。
     *
     * ★ 一定要检查返回值！
     *   HAL 可能因为 RxState 不是 READY（比如之前启动过一次）
     *   而直接返回 HAL_BUSY，此时 DMA 根本没跑，表现为"永远收不到数据"。
     *   这种情况和"接线断了"的现象完全一样，极难区分，
     *   所以必须把状态打印出来。 */
    HAL_StatusTypeDef st = HAL_UART_Receive_DMA(&huart2,
                                                (uint8_t *)s_rx_buf,
                                                PROTO_RX_BUF_SIZE);

    if (st == HAL_OK)
    {
        log_printf("[PROTO] RX armed (DMA circular, %u bytes)\r\n",
                   (unsigned)PROTO_RX_BUF_SIZE);
    }
    else
    {
        /* HAL_ERROR / HAL_BUSY / HAL_TIMEOUT */
        log_printf("[PROTO] *** RX arm FAILED, HAL status = %d ***\r\n",
                   (int)st);
        log_printf("[PROTO]     (0=OK 1=ERROR 2=BUSY 3=TIMEOUT)\r\n");
    }
}

/* ------------------------------------------------------------------
 *  状态机：喂一个字节进去
 * ------------------------------------------------------------------ */
static void proto_rx_feed(uint8_t b)
{
    switch (s_rx_state)
    {
    /* ---------- 找帧头 ---------- */
    case RX_ST_HEAD1:
        if (b == FRAME_HEAD1) {
            s_rx_state = RX_ST_HEAD2;
        }
        /* 不是 0xAA 就继续等 —— 失步时多余字节被自然丢弃 */
        break;

    case RX_ST_HEAD2:
        if (b == FRAME_HEAD2) {
            s_rx_state = RX_ST_CMD;
        }
        else if (b == FRAME_HEAD1) {
            /* 收到 "AA AA"：第二个 AA 可能是新帧头的开始，继续等 55 */
            s_rx_state = RX_ST_HEAD2;
        }
        else {
            s_rx_state = RX_ST_HEAD1;   /* 假的帧头，重新找 */
        }
        break;

    /* ---------- 收 CMD ---------- */
    case RX_ST_CMD:
        s_rx_cmd   = b;
        s_rx_crc   = crc16_update(0xFFFFU, b);   /* ★ CRC 从 CMD 开始算 */
        s_rx_state = RX_ST_LEN;
        break;

    /* ---------- 收 LEN ---------- */
    case RX_ST_LEN:
        s_rx_len = b;

        /* ★ 关键防护：先检查长度是否合法，再继续收
         *   如果不检查就拿去做循环计数，超长的 LEN 会写爆缓冲区。 */
        if (s_rx_len > FRAME_MAX_PAYLOAD) {
            s_rx_stats.err_len++;
            log_printf("[PROTO] RX bad LEN %u (>%u), resync\r\n",
                       (unsigned)s_rx_len, (unsigned)FRAME_MAX_PAYLOAD);
            s_rx_state = RX_ST_HEAD1;
            break;
        }

        s_rx_crc   = crc16_update(s_rx_crc, b);
        s_rx_fill  = 0;
        s_rx_state = (s_rx_len == 0U) ? RX_ST_CRC_L : RX_ST_PAYLOAD;
        break;

    /* ---------- 收载荷 ---------- */
    case RX_ST_PAYLOAD:
        s_rx_payload[s_rx_fill] = b;
        s_rx_fill++;
        s_rx_crc = crc16_update(s_rx_crc, b);

        if (s_rx_fill >= s_rx_len) {
            s_rx_state = RX_ST_CRC_L;
        }
        break;

    /* ---------- 收 CRC（小端：低字节先到）---------- */
    case RX_ST_CRC_L:
        s_rx_crc_low = b;
        s_rx_state   = RX_ST_CRC_H;
        break;

    case RX_ST_CRC_H:
    {
        uint16_t crc_recv = (uint16_t)s_rx_crc_low
                          | (uint16_t)((uint16_t)b << 8);

        if (crc_recv == s_rx_crc)
        {
            s_rx_stats.frames_ok++;
            if (s_rx_handler != NULL) {
                s_rx_handler(s_rx_cmd, s_rx_payload, s_rx_len);
            }
        }
        else
        {
            s_rx_stats.err_crc++;
            log_printf("[PROTO] RX CRC bad: recv=%04X calc=%04X (cmd=%02X len=%u)\r\n",
                       (unsigned)crc_recv, (unsigned)s_rx_crc,
                       (unsigned)s_rx_cmd, (unsigned)s_rx_len);
        }

        s_rx_state = RX_ST_HEAD1;    /* 无论成败，准备接下一帧 */
        break;
    }

    default:
        s_rx_state = RX_ST_HEAD1;
        break;
    }
}

/* ------------------------------------------------------------------
 *  轮询：把 DMA 新收到的字节喂给状态机
 * ------------------------------------------------------------------ */
void proto_rx_poll(void)
{
    /* DMA 的 NDTR 寄存器 = "还剩多少字节可写"
     * 所以 DMA 已写到的位置 = 缓冲区大小 - NDTR
     *
     * 注意：读 NDTR 的瞬间 DMA 可能正好在写，
     * 读到的位置可能略微"过时" —— 没关系，下一轮补上，
     * 最坏只是晚一个轮询周期处理，不会丢数据。
     *
     * 唯一的风险是"追不上"：如果两轮轮询之间 DMA 写满
     * 了整个缓冲区（256 字节），旧数据会被覆盖。
     * 我们的帧速率是 147~150 字节/2 秒，轮询周期 20ms，
     * 追不上的可能性为零。 */
    uint16_t dma_pos = (uint16_t)(PROTO_RX_BUF_SIZE
                                  - __HAL_DMA_GET_COUNTER(huart2.hdmarx));

    while (s_rx_consume_pos != dma_pos)
    {
        uint8_t b = s_rx_buf[s_rx_consume_pos];

        s_rx_consume_pos++;
        if (s_rx_consume_pos >= PROTO_RX_BUF_SIZE) {
            s_rx_consume_pos = 0;            /* 环形回绕 */
        }

        s_rx_stats.bytes_in++;
        proto_rx_feed(b);
    }
}
