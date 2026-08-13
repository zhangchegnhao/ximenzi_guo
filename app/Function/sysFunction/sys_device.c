#include "sys_device.h"
#include "sys_storage.h"
#include "mcu_cimc_gd32f470vet6.h"

static uint16_t s_device_id       = SYS_DEVICE_DEFAULT_ID;
static uint8_t  s_reboot_pending  = 0U;
static uint8_t  s_startup_hb_pend = 1U;  /* send HB once after power-on/reset */
static uint8_t  s_baud_code       = SYS_DEVICE_BAUD_CODE_DEFAULT;

uint16_t sys_device_get_id(void)
{
    return s_device_id;
}

void sys_device_set_id(uint16_t id)
{
    s_device_id = id;
    sys_storage_save();
}

void sys_device_get_fw_version(uint8_t *out)
{
    if (out == 0) return;
    out[0] = SYS_DEVICE_FW_VER_MAJOR;
    out[1] = SYS_DEVICE_FW_VER_MINOR;
    out[2] = SYS_DEVICE_FW_VER_PATCH;
    out[3] = SYS_DEVICE_FW_VER_BUILD;
}

uint8_t sys_device_get_baudrate_code(void)
{
    return s_baud_code;
}

void sys_device_set_baudrate_code(uint8_t code)
{
    s_baud_code = code;
    sys_storage_save();
}

void sys_device_request_reboot(void)
{
    s_reboot_pending = 1U;
}

void sys_device_reboot_if_pending(void)
{
    if (s_reboot_pending) {
        /* M-01 加固：复位前显式关 RS485 发送驱动（DE=0 接收模式），
           避免 NVIC_SystemReset 引起 TX 翻转通过浮空 DE 漏到总线，
           PC 端收到杂波显示 '?'。 */
        RS485_CS_SET(0);
        NVIC_SystemReset();
    }
}

uint8_t sys_device_startup_hb_pending(void)
{
    return s_startup_hb_pend;
}

void sys_device_clear_startup_hb(void)
{
    s_startup_hb_pend = 0U;
}
