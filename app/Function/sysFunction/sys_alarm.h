#ifndef SYS_ALARM_H
#define SYS_ALARM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_ALARM_MODE_ACTIVE     0x01U   /* 主动上报 + Flash 存储 */
#define SYS_ALARM_MODE_PASSIVE    0x02U   /* 仅 Flash 存储，等待查询 */
#define SYS_ALARM_MODE_DEFAULT    SYS_ALARM_MODE_PASSIVE

/* 通道号：CH2 预留（外部 ADC PT100，后续任务接入） */
#define SYS_ALARM_CH0   0U
#define SYS_ALARM_CH1   1U
#define SYS_ALARM_CH2   2U
#define SYS_ALARM_CH_NUM 3U

/* Flash 中一条告警记录（与 sys_alarm.c 内部布局一致，对外只暴露 getter） */
typedef struct {
    uint32_t utc_seconds;
    uint8_t  channel;
    float    threshold;
    float    value;
} SysAlarmRecord;

/* 上电时调一次：扫描 Flash 已存记录数，初始化通道防抖状态。 */
void sys_alarm_init(void);

/* 设置/查询主动上报模式；返回 0 成功，-1 模式无效。设置后会持久化。 */
int     sys_alarm_set_mode(uint8_t mode);
uint8_t sys_alarm_get_mode(void);

/* 周期调用：检查 CH0/CH1（CH2 预留）当前采样值是否超过对应阈值，
   仅在"未超→超"上升沿触发记录与上报。 */
void sys_alarm_tick(void);

/* 后续任务 0x0602 用：返回当前 Flash 中告警记录数。 */
uint16_t sys_alarm_count(void);

/* 后续任务 0x0602 用：读取第 idx 条记录，返回 0 成功 -1 越界。 */
int sys_alarm_get_record(uint16_t idx, SysAlarmRecord *out);

/* 后续任务 0x0603 用：清空 Flash 告警区。 */
void sys_alarm_clear_all(void);

/* 把一条记录格式化为告警 ASCII 行（"YYYY-MM-DD HH:MM:SS | CHx | th | val\n"）。
   buf 建议 ≥ 64 字节。返回写入字节数（不含末尾 '\0'）。 */
uint16_t sys_alarm_format_record(const SysAlarmRecord *rec, char *buf, uint16_t size);

#ifdef __cplusplus
}
#endif

#endif /* SYS_ALARM_H */
