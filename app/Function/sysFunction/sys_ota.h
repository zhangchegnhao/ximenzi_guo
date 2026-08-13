/* Function/sysFunction/sys_ota.h
 *
 * N-02 OTA 准备阶段（0x0502）的"接收 + 魔术字验证 + 应答" 状态机。
 *
 * 仅在收到 0x0502 命令后短暂介入：抢占 RS485 字节流，缓存前 4B 用于校验
 * 升级包魔术字（5A A5 C3 3C），按空闲超时判定整包接收完成，由 uart_task
 * 取出 OK / ERROR 应答发出。不在 N-02 阶段执行真正的升级写 Flash。
 */
#ifndef SYS_OTA_H
#define SYS_OTA_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 上电初始化，把状态机置为 IDLE。 */
void sys_ota_init(void);

/* 进入魔术字验证态。dispatch 收 0x0502（payload_len=0）时调。 */
void sys_ota_request_verify(void);

/* 是否处于验证态。ota_uart_process_frame 入口守卫用，
   验证态下抢占字节流，不进入流式 OTA 接收。 */
bool sys_ota_is_verifying(void);

/* 喂入 RS485 IDLE 切出的一段原始字节。
   仅缓存前 4 字节做魔术字校验；其余字节仅计数 + 刷新最近接收时刻。 */
void sys_ota_feed(const uint8_t *data, uint16_t len);

/* uart_task 周期调用，检测 100ms 空闲超时（且至少收到 1B）→ 触发魔术字
   校验 → 切到"应答待发"态。 */
void sys_ota_tick(uint32_t now_ms);

/* uart_task 取出待发应答 ASCII 帧；返回 0 表示当前无应答。
   取走后状态机自动回 IDLE。 */
uint16_t sys_ota_take_pending_response(char *buf, uint16_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* SYS_OTA_H */
