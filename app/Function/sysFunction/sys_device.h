#ifndef SYS_DEVICE_H
#define SYS_DEVICE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_DEVICE_DEFAULT_ID  0x0001U

/* Firmware version 2.0.1.0 → 4 bytes: major.minor.patch.build */
#define SYS_DEVICE_FW_VER_MAJOR  0x02U
#define SYS_DEVICE_FW_VER_MINOR  0x00U
#define SYS_DEVICE_FW_VER_PATCH  0x01U
#define SYS_DEVICE_FW_VER_BUILD  0x00U
#define SYS_DEVICE_FW_VER_LEN    4U

uint16_t sys_device_get_id(void);
void     sys_device_set_id(uint16_t id);

/* Fill out[0..3] with firmware version bytes (big-endian: major,minor,patch,build) */
void sys_device_get_fw_version(uint8_t *out);

/* 波特率映射码：0x11=4800, 0x12=9600, 0x13=19200, 0x14=115200 */
#define SYS_DEVICE_BAUD_CODE_4800    0x11U
#define SYS_DEVICE_BAUD_CODE_9600    0x12U
#define SYS_DEVICE_BAUD_CODE_19200   0x13U
#define SYS_DEVICE_BAUD_CODE_115200  0x14U
#define SYS_DEVICE_BAUD_CODE_DEFAULT SYS_DEVICE_BAUD_CODE_19200  /* RS485 默认 19200 */

uint8_t sys_device_get_baudrate_code(void);
void    sys_device_set_baudrate_code(uint8_t code);

void sys_device_request_reboot(void);
void sys_device_reboot_if_pending(void);

uint8_t sys_device_startup_hb_pending(void);
void    sys_device_clear_startup_hb(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_DEVICE_H */
