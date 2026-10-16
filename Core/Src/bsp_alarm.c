#include "bsp_alarm.h"

/* ------------------------------------------------------------------
 * 为什么把写 GPIO 封装成函数？
 * ------------------------------------------------------------------
 *   ① 集中处理"低电平有效"这个反直觉的细节 —— 调用方只管传 1/0
 *   ② 将来换硬件（比如 LED 改成高电平有效、蜂鸣器换成 PWM 驱动）
 *      只改这一个文件，应用层不用动
 *   ③ 如果以后要给报警加"硬件自检"，也在这里加
 * ------------------------------------------------------------------ */

void alarm_led_red(uint8_t on)
{
    /* 低电平有效：on=1 → 输出低 → 亮 */
    HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin,
                      on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void alarm_led_green(uint8_t on)
{
    HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin,
                      on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void alarm_beep(uint8_t on)
{
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin,
                      on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void alarm_all_off(void)
{
    alarm_led_red(0);
    alarm_led_green(0);
    alarm_beep(0);
}