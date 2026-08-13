#ifndef SYS_PARAM_H
#define SYS_PARAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 默认变比（上报值 = 实测电压 × 变比） */
#define SYS_PARAM_RATIO_DEFAULT     1.0f
/* 默认阈值（告警判定用，运行时可通过 0x0411/0x0412 改写）
   选 21.59f 与协议文档示例一致（0x41ACB852），上电首次查询可对应文档帧 */
#define SYS_PARAM_THRESH_DEFAULT    21.59f

float sys_param_get_ch0_ratio(void);
void  sys_param_set_ch0_ratio(float ratio);

float sys_param_get_ch1_ratio(void);
void  sys_param_set_ch1_ratio(float ratio);

float sys_param_get_ch0_thresh(void);
void  sys_param_set_ch0_thresh(float th);

float sys_param_get_ch1_thresh(void);
void  sys_param_set_ch1_thresh(float th);

#ifdef __cplusplus
}
#endif

#endif /* SYS_PARAM_H */
