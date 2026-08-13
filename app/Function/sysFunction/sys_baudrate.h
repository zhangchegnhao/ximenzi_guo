/* Function/sysFunction/sys_baudrate.h
 * 波特率码 ↔ 实际波特率映射与应用。
 *   不动 BSP（BSP 默认初始化为 19200），由本模块在 BSP 之后覆盖。
 *   复用 GD32 标准库 usart_baudrate_set。
 *
 * 映射关系（与 0x01A2 / 0x0112 协议码一致）：
 *   0x11=4800, 0x12=9600, 0x13=19200, 0x14=115200
 */
#ifndef SYS_BAUDRATE_H
#define SYS_BAUDRATE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 将协议码映射为实际波特率。无效码返回 0。 */
uint32_t sys_baudrate_code_to_value(uint8_t code);

/* 将协议码应用到 RS485 USART（必须在 bsp_rs_usart_init 之后调用）。
 * 返回 0 成功，-1 无效码。 */
int sys_baudrate_apply(uint8_t code);

#ifdef __cplusplus
}
#endif

#endif /* SYS_BAUDRATE_H */
