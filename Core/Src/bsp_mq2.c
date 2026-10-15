#include "bsp_mq2.h"

/* ------------------------------------------------------------------
 * ADC + DMA 目标缓冲区
 * ------------------------------------------------------------------
 * 元素个数 = ADC 的 Rank 数（你的配置里 NbrOfConversion = 2）
 *
 *   [0] = Rank 1 = ADC_CHANNEL_4 = PA4 = ★ MQ-2 的分压输出
 *   [1] = Rank 2 = ADC_CHANNEL_5 = PA5 = 备用（未接）
 *
 * ★★ volatile 必须加！
 *    这个数组由 DMA 在【后台】改写，CPU 从不写它。
 *    不加 volatile，编译器会认为"没人改过这个变量"，
 *    从而把你对它的读取优化成常量 —— 读数就永远不变了。
 *
 * ★★ 位置：默认放在 .bss（普通 SRAM，DMA 可访问）
 *    绝对不要把这个数组放到 CCM RAM（0x10000000）——
 *    F407 的 CCM RAM 不经过总线矩阵，DMA 访问不到，会静默失败！
 * ------------------------------------------------------------------ */
static volatile uint16_t adc_dma_buf[2];

void mq2_init(void)
{
    /* 启动 ADC 的 DMA 转换
     *
     * 第 2 个参数强转成 uint32_t*：HAL 的函数签名就是这么定义的，
     *   但 ADC 是 12 位，实际只用到低 16 位，所以用 uint16_t 数组是安全的。
     *
     * 第 3 个参数 = 要传输的数据个数 = Rank 数 = 2
     *
     * CubeMX 里已经配成 Circular（循环）模式 + Continuous Conversion，
     * 所以调用一次后，DMA 会永远自动地把最新转换结果搬进这个数组。
     */
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_dma_buf, 2);
}

uint16_t mq2_read_raw(void)
{
    return adc_dma_buf[0];
}

/* EMA 滤波器的累加器
 *
 * 用 float 而不是 uint16_t：EMA 是小数运算，用整数会累积舍入误差
 * static：跨调用保持（这正是 static 的正确用法）
 * volatile：虽然只在任务里访问，但加上无害，防止编译器过度优化 */
static volatile float mq2_ema_acc = 0.0f;

/**
  * @brief  读滤波后的 ADC 值（指数移动平均）
  * @retval 0 ~ 4095
  */
uint16_t mq2_read(void)
{
    uint16_t raw = adc_dma_buf[0];

    if (mq2_ema_acc == 0.0f) {
        /* 第一次调用：直接用原始值初始化，避免从 0 慢慢爬上来 */
        mq2_ema_acc = (float)raw;
    } else {
        mq2_ema_acc = MQ2_EMA_ALPHA * (float)raw
                    + (1.0f - MQ2_EMA_ALPHA) * mq2_ema_acc;
    }

    return (uint16_t)mq2_ema_acc;
}

float mq2_read_voltage(void)
{
    /* 用【滤波后】的值换算电压 */
    float v_pin = ((float)mq2_read() / MQ2_ADC_RESOLUTION) * MQ2_ADC_VREF;
    return v_pin / MQ2_DIVIDER_RATIO;
}