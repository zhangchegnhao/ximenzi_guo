/**
 * @FilePath: adc_app.c
 * @Author: Ahypnis
 * @Date: 2026-04-18 23:46:48
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2026-04-19 01:35:51
 * @Copyright: 2026 0668STO CO.,LTD. All Rights Reserved.
*/
#include "mcu_cimc_gd32f470vet6.h"
#include "sys_pt100.h"

extern uint16_t adc_value[2];
extern uint16_t convertarr[CONVERT_NUM];

/* CH2 PT100 周期采集
 * 直接复用 sys_pt100_get_temp（confine §6.2）；保持 PGA = 2.048V 与协议端一致，
 * 让芯片在持续转换模式下始终用 2.048V 量程，0x0221 实时查询拿到的 raw 才有意义。
 * 原来这里写 PGA_6V144，与协议端 2.048V 冲突，导致 raw 按错误量程系数换算。 */
void adc_task(void)
{
    (void)sys_pt100_get_temp();
    convertarr[0] = adc_value[0];
}

