#ifndef SYS_SLEEP_H
#define SYS_SLEEP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_SLEEP_DEFAULT_SECONDS  10U

/* 标记进入睡眠：在 OK 应答发完后由 uart_task 触发执行 */
void sys_sleep_request(uint32_t seconds);

/* 若有挂起的睡眠请求：配置 RTC 闹钟 → 进入深度睡眠 → 唤醒后回 ASCII 字符串。
   未挂起则立即返回。由 uart_task 在应答发送完成后调用。 */
void sys_sleep_execute_if_pending(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_SLEEP_H */
