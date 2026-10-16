#ifndef __BSP_ALARM_H
#define __BSP_ALARM_H

#include "main.h"

/* 三路执行器（全部低电平有效） */
void alarm_led_red(uint8_t on);      /* LED0 = PF9  = DS0 红色 */
void alarm_led_green(uint8_t on);    /* LED1 = PF10 = DS1 绿色 */
void alarm_beep(uint8_t on);         /* BEEP = PF8 */

/* 全部关闭（初始化/退出报警时用） */
void alarm_all_off(void);

#endif /* __BSP_ALARM_H */