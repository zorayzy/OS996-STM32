#include "bsp_pir.h"

/* ------------------------------------------------------------------
 * 消抖参数
 * ------------------------------------------------------------------
 * PIR_DEBOUNCE_CNT = 需要连续读到几次相同电平才承认状态改变
 *
 * 为什么需要消抖：
 *   PIR 的输出不干净 —— 上电初始化期间会抖、人走动时会抖、
 *   风/灯光/温度变化都会造成干扰。直接用电平会产生大量误报。
 *
 * 窗口时长 = PIR_DEBOUNCE_CNT × 调用周期
 *          = 3 × 50ms = 150ms
 * ------------------------------------------------------------------ */
#define PIR_DEBOUNCE_CNT    3

/* 内部状态机变量（static = 只在本文件可见，外部无法误改） */
static uint8_t pir_stable  = PIR_NO_MOTION;   /* 消抖后的稳定状态 */
static uint8_t pir_cnt     = 0;               /* 连续读到"不同值"的次数 */

/**
  * @brief  读一次引脚原始电平（不做消抖）
  */
static uint8_t pir_read_raw(void)
{
    return (HAL_GPIO_ReadPin(PIR_GPIO_Port, PIR_Pin) == GPIO_PIN_SET)
           ? PIR_MOTION : PIR_NO_MOTION;
}

void pir_reset(void)
{
    pir_stable = PIR_NO_MOTION;
    pir_cnt    = 0;
}

int pir_read(void)
{
    uint8_t raw = pir_read_raw();

    if (raw == pir_stable)
    {
        /* 和当前稳定状态一致 —— 计数清零 */
        pir_cnt = 0;
    }
    else
    {
        /* 不一致 —— 累计次数 */
        pir_cnt++;

        if (pir_cnt >= PIR_DEBOUNCE_CNT)
        {
            /* 连续 N 次都和稳定状态不同 → 状态真的变了 */
            pir_stable = raw;
            pir_cnt    = 0;
        }
    }

    return pir_stable;
}
