/* Function/sysFunction/sys_ota.c - N-02 OTA 准备阶段状态机
 *
 * 状态流转：
 *   IDLE ──0x0502──▶ VERIFYING ──100ms idle──▶ RESP_PENDING ──take──▶ IDLE
 *
 * 复用：
 *   - get_system_ms()   (USER/systick)            ：毫秒戳
 *   - proto_build_ok()  (Protocol/proto_frame)    ：OK 帧 (FF)
 *   - proto_build_err() (Protocol/proto_frame)    ：FF + 0x0502 错误帧
 *   - sys_device_get_id()                          ：device_id
 */
#include "sys_ota.h"
#include <stddef.h>
#include "proto_frame.h"
#include "proto_cmd.h"
#include "sys_device.h"

/* get_system_ms 在 USER/systick 声明，外部可见 */
extern uint32_t get_system_ms(void);

#define SYS_OTA_IDLE_TIMEOUT_MS  100U
#define SYS_OTA_MAGIC_LEN        4U

/* BootLoader bl_package.h BL_FW_PACKAGE_MAGIC = 0x5AA5C33C，字节流前 4B */
#define SYS_OTA_MAGIC_B0  0x5AU
#define SYS_OTA_MAGIC_B1  0xA5U
#define SYS_OTA_MAGIC_B2  0xC3U
#define SYS_OTA_MAGIC_B3  0x3CU

typedef enum {
    SYS_OTA_STATE_IDLE = 0,
    SYS_OTA_STATE_VERIFYING,
    SYS_OTA_STATE_RESP_PENDING
} sys_ota_state_t;

static sys_ota_state_t s_state         = SYS_OTA_STATE_IDLE;
static uint8_t         s_magic_buf[SYS_OTA_MAGIC_LEN];
static uint16_t        s_magic_filled  = 0U;
static uint32_t        s_rx_count      = 0U;
static uint32_t        s_last_rx_ms    = 0U;
static bool            s_resp_is_ok    = false;

void sys_ota_init(void)
{
    s_state        = SYS_OTA_STATE_IDLE;
    s_magic_filled = 0U;
    s_rx_count     = 0U;
    s_last_rx_ms   = 0U;
    s_resp_is_ok   = false;
}

void sys_ota_request_verify(void)
{
    s_state        = SYS_OTA_STATE_VERIFYING;
    s_magic_filled = 0U;
    s_rx_count     = 0U;
    /* 起算时刻；尚未收到字节时 tick 不计超时（rx_count==0 守卫） */
    s_last_rx_ms   = get_system_ms();
    s_resp_is_ok   = false;
}

bool sys_ota_is_verifying(void)
{
    return (s_state == SYS_OTA_STATE_VERIFYING);
}

void sys_ota_feed(const uint8_t *data, uint16_t len)
{
    if (s_state != SYS_OTA_STATE_VERIFYING) return;
    if (data == NULL || len == 0U) return;

    for (uint16_t i = 0U; i < len; i++) {
        if (s_magic_filled < SYS_OTA_MAGIC_LEN) {
            s_magic_buf[s_magic_filled++] = data[i];
        }
    }
    s_rx_count   += len;
    s_last_rx_ms  = get_system_ms();
}

void sys_ota_tick(uint32_t now_ms)
{
    if (s_state != SYS_OTA_STATE_VERIFYING) return;
    /* 还没收到任何字节 → 不计超时（避免命令一进来立刻误判完成） */
    if (s_rx_count == 0U) return;
    if ((uint32_t)(now_ms - s_last_rx_ms) < SYS_OTA_IDLE_TIMEOUT_MS) return;

    bool magic_ok = (s_magic_filled >= SYS_OTA_MAGIC_LEN)
                 && (s_magic_buf[0] == SYS_OTA_MAGIC_B0)
                 && (s_magic_buf[1] == SYS_OTA_MAGIC_B1)
                 && (s_magic_buf[2] == SYS_OTA_MAGIC_B2)
                 && (s_magic_buf[3] == SYS_OTA_MAGIC_B3);

    s_resp_is_ok = magic_ok;
    s_state      = SYS_OTA_STATE_RESP_PENDING;
}

uint16_t sys_ota_take_pending_response(char *buf, uint16_t buf_size)
{
    if (s_state != SYS_OTA_STATE_RESP_PENDING) return 0U;
    if (buf == NULL || buf_size == 0U) return 0U;

    uint16_t my_id = sys_device_get_id();
    uint16_t len;
    if (s_resp_is_ok) {
        /* OK：A5B6 <id> 02 0502 01 02 FF <crc> B6A5 */
        len = proto_build_ok(my_id, CMD_OTA_PREPARE, buf, buf_size);
    } else {
        /* ERROR：A5B6 <id> FF 0502 00 02 <crc> B6A5（保留 orig_cmd） */
        len = proto_build_err(my_id, CMD_OTA_PREPARE, buf, buf_size);
    }

    /* 应答取走后恢复 IDLE，准备下一轮 */
    s_state        = SYS_OTA_STATE_IDLE;
    s_magic_filled = 0U;
    s_rx_count     = 0U;
    s_resp_is_ok   = false;
    return len;
}
