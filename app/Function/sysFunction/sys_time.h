#ifndef SYS_TIME_H
#define SYS_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 从硬件 RTC 读取当前时间并转换为 UTC Unix 秒级时间戳（1970-01-01 起）。
   RTC 年存储为 BCD，代表 20xx 年。 */
uint32_t sys_time_get_utc_seconds(void);

/* 把 UTC Unix 秒级时间戳写入硬件 RTC。返回 0 成功，-1 失败。
   仅支持 2000-01-01 之后的时间（RTC 年范围 0-99 → 2000-2099）。 */
int sys_time_set_utc_seconds(uint32_t ts);

/* 读取 RTC，格式化为 "YYYY-MM-DD HH:MM:SS"（共 19 字符 + '\0'）。
   buf 至少 20 字节。 */
void sys_time_format_datetime(char *buf, uint16_t size);

/* 把 UTC Unix 秒级时间戳反推为日期格式 "YYYY-MM-DD HH:MM:SS"（共 19 字符 + '\0'）。
   buf 至少 20 字节。 */
void sys_time_format_utc(uint32_t ts, char *buf, uint16_t size);

/* BCD ↔ 十进制 互转辅助（对外暴露给 sys_sleep 等模块使用） */
uint8_t sys_time_bcd_to_dec(uint8_t bcd);
uint8_t sys_time_dec_to_bcd(uint8_t dec);

#ifdef __cplusplus
}
#endif

#endif /* SYS_TIME_H */
