/* Function/sysFunction/sys_sample.c
 * CH0 = adc_value[0]（ADC0_CH10, PC0, 板载电位器，DMA 持续刷新）
 * CH1 = bsp_adc1_read_ch1()（ADC1_CH11, PC1, DAC 回读，按需阻塞读取）
 * 所有上报值 = 实测电压 × 当前变比（遵 confine.md §4.1）。
 */
#include "sys_sample.h"
#include "sys_param.h"
#include "mcu_cimc_gd32f470vet6.h"

extern uint16_t adc_value[2];

static float adc_raw_to_volt(uint16_t raw)
{
    return ((float)raw / SYS_SAMPLE_ADC_FULL_SCALE) * SYS_SAMPLE_ADC_VREF;
}

float sys_sample_get_ch0(void)
{
    return adc_raw_to_volt(adc_value[0]) * sys_param_get_ch0_ratio();
}

float sys_sample_get_ch1(void)
{
    return adc_raw_to_volt(bsp_adc1_read_ch1()) * sys_param_get_ch1_ratio();
}
