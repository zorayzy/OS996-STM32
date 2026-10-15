#ifndef __BSP_MQ2_H
#define __BSP_MQ2_H

#include "main.h"

/* ------------------------------------------------------------------
 * MQ-2 模拟输出分压参数
 * ------------------------------------------------------------------
 * 硬件分压电路：
 *
 *   MQ-2 AO (0~5V) ──[R1 500Ω]──┬──> PA4 (ADC1_IN4)
 *                               │
 *                           [R2 500Ω]
 *                               │
 *                              GND
 *
 * 分压比 = R2 / (R1 + R2) = 500 / 1000 = 0.5
 * 5V × 0.5 = 2.5V   ← 安全（ADC 上限 3.3V）
 *
 * ★ 如果你以后换了电阻，必须同步修改这个比值
 * ------------------------------------------------------------------ */
#define MQ2_DIVIDER_RATIO     0.5f
#define MQ2_EMA_ALPHA         0.1f

/* ------------------------------------------------------------------
 * ADC 参数（对应 CubeMX 里 ADC1 的配置）
 * ------------------------------------------------------------------ */
#define MQ2_ADC_RESOLUTION    4095.0f   /* 12 位：0 ~ 4095 */
#define MQ2_ADC_VREF          3.3f      /* ADC 参考电压 = VDDA（单位 V） */

/* 连续采样的平均次数（软件滤波，抑制噪声） */
#define MQ2_AVG_SAMPLES       8

/**
  * @brief  初始化：启动 ADC + DMA
  * @note   必须在 osKernelStart() 【之前】调用一次，之后永久自动运行
  */
void mq2_init(void);

/**
  * @brief  读取 ADC 原始值（瞬时值，未平均）
  * @retval 0 ~ 4095
  */
uint16_t mq2_read_raw(void);

/**
  * @brief  读取 ADC 原始值（连续采样取平均，更稳定）
  * @retval 0 ~ 4095
  */
uint16_t mq2_read(void);

/**
  * @brief  读取 AO 引脚上的真实电压（已还原分压）
  * @retval 单位 V，范围约 0 ~ 5V
  */
float mq2_read_voltage(void);

#endif /* __BSP_MQ2_H */