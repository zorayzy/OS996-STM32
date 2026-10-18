/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>          /* strlen()，栈溢出钩子里要用 */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
   /* ==================================================================
    * 栈溢出钩子
    * ==================================================================
    * configCHECK_FOR_STACK_OVERFLOW = 2 时会调用到这里。
    *
    * 检查方式（方法 2 的原理）：
    *   每个任务创建时，栈顶 16 个字节被填成魔数 0xA5A5A5A5。
    *   任务切换时检查这 16 字节是否还完好 —— 被改写说明栈用超了。
    *
    * ★ 这里【不能用 printf】：
    *     printf 自身要压约 100~200 字节栈，
    *     在栈已经溢出的情况下调用它 → 二次溢出 → 可能 HardFault 死循环。
    *     所以必须用最原始的阻塞发送。
    *
    * ★ 这里【不能 return】：
    *     栈已经坏了，继续运行行为不可预测（可能破坏其他任务的栈）。
    *     死循环是"安全失败"——问题被锁在原地，方便用调试器查看现场。
    *     调试器此时可以暂停，查看 xTask 和 pcTaskName 找到是哪个任务。
    * ================================================================== */
   const char *msg1 = "\r\n*** STACK OVERFLOW! ***\r\n";
   const char *msg2 = "*** task: ";
   const char *msg3 = " ***\r\n*** system halted ***\r\n";

   HAL_UART_Transmit(&huart1, (uint8_t *)msg1, 28, 100);
   HAL_UART_Transmit(&huart1, (uint8_t *)msg2, 10, 100);
   if (pcTaskName != NULL)
   {
      HAL_UART_Transmit(&huart1, (uint8_t *)pcTaskName,
                        (uint16_t)strlen((const char *)pcTaskName), 100);
   }
   HAL_UART_Transmit(&huart1, (uint8_t *)msg3, 33, 100);

   /* 死循环：让调试器能停在这里看现场 */
   taskDISABLE_INTERRUPTS();
   for (;;)
   {
      /* 什么也不做 */
   }
}
/* USER CODE END 4 */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

