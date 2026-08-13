#ifndef SYS_DAC_H
#define SYS_DAC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_DAC_RAW_MAX  0x0FFFU  /* 12 位 DAC 上限 */

/* 写入 DAC 数据保持寄存器；TIMER5 触发后该值出现在 PA4。
   raw 超过 0x0FFF 时夹到上限。setter 末尾自动持久化到 SPI Flash。 */
void sys_dac_set_raw(uint16_t raw);

/* 返回上次写入的 raw 值（已持久化，重启 restore） */
uint16_t sys_dac_get_raw(void);

/* sys_storage 加载阶段调用：把持久化的 raw 写回 DAC，不触发再次持久化。 */
void sys_dac_restore_raw(uint16_t raw);

#ifdef __cplusplus
}
#endif

#endif /* SYS_DAC_H */
