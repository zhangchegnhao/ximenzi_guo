/* Function/sysFunction/sys_baudrate.c
 * 波特率码 ↔ 实际波特率，覆盖 BSP 默认 19200。
 * 复用 GD32 标准库 usart_baudrate_set（confine.md §6.2）。
 */
#include "sys_baudrate.h"
#include "sys_device.h"
#include "mcu_cimc_gd32f470vet6.h"

uint32_t sys_baudrate_code_to_value(uint8_t code)
{
    switch (code) {
    case SYS_DEVICE_BAUD_CODE_4800:   return 4800U;
    case SYS_DEVICE_BAUD_CODE_9600:   return 9600U;
    case SYS_DEVICE_BAUD_CODE_19200:  return 19200U;
    case SYS_DEVICE_BAUD_CODE_115200: return 115200U;
    default: return 0U;
    }
}

int sys_baudrate_apply(uint8_t code)
{
    uint32_t baud = sys_baudrate_code_to_value(code);
    if (baud == 0U) return -1;

    /* GD32 规范：修改 BAUD 寄存器前必须先 disable USART，避免正在传输的字节 corrupt。
       配置完毕后重新 enable，并复位 IDLE 中断状态防止误触发。 */
    usart_disable(RS232_RS485_USART);
    usart_baudrate_set(RS232_RS485_USART, baud);
    usart_flag_clear(RS232_RS485_USART, USART_FLAG_IDLE);
    usart_enable(RS232_RS485_USART);
    return 0;
}
