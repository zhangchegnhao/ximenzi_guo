/* Function/sysFunction/sys_report.c
 * 自动上报状态管理：间隔码、启停标志、上次发送时刻。
 * 不涉及帧格式（协议层负责），不涉及发送（APP 层负责）。
 */
#include "sys_report.h"
#include "sys_storage.h"

static uint8_t  s_enabled       = 0U;
static uint8_t  s_interval_code = SYS_REPORT_INTV_CODE_DEFAULT;
static uint32_t s_interval_ms   = 1000UL;
static uint32_t s_last_send_ms  = 0UL;

static uint32_t code_to_ms(uint8_t code)
{
    switch (code) {
        case SYS_REPORT_INTV_CODE_1S: return 1000UL;
        case SYS_REPORT_INTV_CODE_3S: return 3000UL;
        case SYS_REPORT_INTV_CODE_5S: return 5000UL;
        default:                      return 0UL;
    }
}

int sys_report_set_interval_code(uint8_t code)
{
    uint32_t ms = code_to_ms(code);
    if (ms == 0UL) return -1;
    s_interval_code = code;
    s_interval_ms   = ms;
    sys_storage_save();
    return 0;
}

uint8_t  sys_report_get_interval_code(void) { return s_interval_code; }
uint32_t sys_report_get_interval_ms(void)   { return s_interval_ms; }

void sys_report_start(uint32_t now_ms)
{
    s_enabled      = 1U;
    s_last_send_ms = now_ms;
}

void sys_report_stop(void)
{
    s_enabled = 0U;
}

uint8_t sys_report_is_enabled(void) { return s_enabled; }

uint8_t sys_report_should_send(uint32_t now_ms)
{
    if (!s_enabled) return 0U;
    return (uint8_t)((now_ms - s_last_send_ms) >= s_interval_ms);
}

void sys_report_mark_sent(uint32_t now_ms)
{
    s_last_send_ms = now_ms;
}
