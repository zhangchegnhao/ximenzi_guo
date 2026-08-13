#ifndef SYS_SAMPLE_H
#define SYS_SAMPLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ADC 参考电压（V）。BSP 配置 ADC_VREF_SOURCE_PC2，但满量程基准按 VDDA=3.3V 处理。
   如硬件实际不同，修改此宏即可。 */
#ifndef SYS_SAMPLE_ADC_VREF
#define SYS_SAMPLE_ADC_VREF  3.3f
#endif

/* GD32F4xx 内置 ADC 12 位 */
#define SYS_SAMPLE_ADC_FULL_SCALE  4095.0f

/* 返回 CH0（板载电位器）当前测量值，单位 V，已乘上 CH0 变比 */
float sys_sample_get_ch0(void);

/* 返回 CH1（DAC 回读）当前测量值，单位 V，已乘上 CH1 变比 */
float sys_sample_get_ch1(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_SAMPLE_H */
