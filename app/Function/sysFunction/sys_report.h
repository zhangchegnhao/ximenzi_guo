#ifndef SYS_REPORT_H
#define SYS_REPORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 间隔映射码：01-1s，02-3s，03-5s */
#define SYS_REPORT_INTV_CODE_1S   0x01U
#define SYS_REPORT_INTV_CODE_3S   0x02U
#define SYS_REPORT_INTV_CODE_5S   0x03U
#define SYS_REPORT_INTV_CODE_DEFAULT  SYS_REPORT_INTV_CODE_1S

/* 设置上报间隔码（持久化）。无效码（非 01/02/03）回 -1，不修改。 */
int     sys_report_set_interval_code(uint8_t code);
uint8_t sys_report_get_interval_code(void);
uint32_t sys_report_get_interval_ms(void);

/* 启动/停止自动上报。start 同时把"上次发送时刻"重置为 now（首帧由命令应答返回）。 */
void sys_report_start(uint32_t now_ms);
void sys_report_stop(void);
uint8_t sys_report_is_enabled(void);

/* 周期调用：到时返回 1，否则 0；返回 1 时调用方应组帧、发送、随后调 mark_sent */
uint8_t sys_report_should_send(uint32_t now_ms);
void    sys_report_mark_sent(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* SYS_REPORT_H */
