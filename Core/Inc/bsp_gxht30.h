#ifndef __BSP_GXHT30_H
#define __BSP_GXHT30_H

#include "main.h"

/* 返回值定义 */
#define GXHT30_OK          0     /* 成功 */
#define GXHT30_ERR_I2C    -1     /* I2C 通信失败 */
#define GXHT30_ERR_CRC    -2     /* CRC 校验失败 */

/* 函数声明 */
int gxht30_read(float *temp, float *humi);

#endif /* __BSP_GXHT30_H */