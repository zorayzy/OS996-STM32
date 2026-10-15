#ifndef __BSP_PIR_H
#define __BSP_PIR_H

#include "main.h"

/* 返回值定义（和 bsp_gxht30.h 保持一致的风格） */
#define PIR_OK        0
#define PIR_ERR      -1

/* 状态定义 */
#define PIR_NO_MOTION 0
#define PIR_MOTION    1

/**
  * @brief  读取人体的稳定状态
  * @retval PIR_MOTION(1) 有人 / PIR_NO_MOTION(0) 无人
  * @note   内部做软件消抖：需要连续 PIR_DEBOUNCE_CNT 次读到相同电平才认账。
  *         所以必须【周期性调用】，调用间隔决定了消抖窗口的时长。
  *         例：每 50ms 调一次，连续 3 次 = 150ms 消抖窗口。
  */
int pir_read(void);

/**
  * @brief  复位消抖状态机（比如初始化期结束后调用）
  */
void pir_reset(void);

#endif /* __BSP_PIR_H */