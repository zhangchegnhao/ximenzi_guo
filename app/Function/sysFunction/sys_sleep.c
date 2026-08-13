/* Function/sysFunction/sys_sleep.c
 * 配置 RTC ALARM0 在 N 秒后触发，调用 BSP 已封装的 bsp_enter_deepsleep()
 * 进入深度睡眠；唤醒后回 ASCII 字符串（非帧封装，confine.md §2）。
 *
 * 注意：RTC_Alarm_IRQHandler 必须存在以让 NVIC 接到中断后能从 WFI 返回。
 *      为避免修改 USER/src/gd32f4xx_it.c，定义在本文件，覆盖 startup .s 中的弱符号。
 */
#include "sys_sleep.h"
#include "sys_time.h"
#include "usart_app.h"
#include "mcu_cimc_gd32f470vet6.h"

#ifndef SYS_SLEEP_WAKE_STRING
#define SYS_SLEEP_WAKE_STRING  "instrument wakeup"
#endif

extern rtc_parameter_struct rtc_initpara;
extern rtc_alarm_struct     rtc_alarm;

static uint8_t  s_pending  = 0U;
static uint32_t s_seconds  = SYS_SLEEP_DEFAULT_SECONDS;

void sys_sleep_request(uint32_t seconds)
{
    s_seconds = (seconds == 0U) ? SYS_SLEEP_DEFAULT_SECONDS : seconds;
    s_pending = 1U;
}

/* 把当前 RTC 时间加 N 秒，按 BCD 写入 alarm 结构（h/m/s 字段） */
static void compute_target_bcd(uint32_t seconds_to_add,
                               uint8_t *out_hh, uint8_t *out_mm, uint8_t *out_ss)
{
    rtc_current_time_get(&rtc_initpara);

    uint32_t s = (uint32_t)sys_time_bcd_to_dec(rtc_initpara.second);
    uint32_t m = (uint32_t)sys_time_bcd_to_dec(rtc_initpara.minute);
    uint32_t h = (uint32_t)sys_time_bcd_to_dec(rtc_initpara.hour);

    s += seconds_to_add;
    m += s / 60U;
    s = s % 60U;
    h += m / 60U;
    m = m % 60U;
    h = h % 24U;

    *out_ss = sys_time_dec_to_bcd((uint8_t)s);
    *out_mm = sys_time_dec_to_bcd((uint8_t)m);
    *out_hh = sys_time_dec_to_bcd((uint8_t)h);
}

static void configure_rtc_alarm(uint32_t seconds_from_now)
{
    uint8_t hh_bcd, mm_bcd, ss_bcd;
    compute_target_bcd(seconds_from_now, &hh_bcd, &mm_bcd, &ss_bcd);

    /* 必须先禁用闹钟才能改配置 */
    rtc_alarm_disable(RTC_ALARM0);

    /* 屏蔽日期，匹配 h/m/s 即触发；10s 跨日的边角情况按 mod24 处理 */
    rtc_alarm.alarm_mask      = RTC_ALARM_DATE_MASK;
    rtc_alarm.weekday_or_date = RTC_ALARM_DATE_SELECTED;
    rtc_alarm.alarm_day       = 0x01U;   /* 被 mask 忽略 */
    rtc_alarm.alarm_hour      = hh_bcd;
    rtc_alarm.alarm_minute    = mm_bcd;
    rtc_alarm.alarm_second    = ss_bcd;
    rtc_alarm.am_pm           = RTC_AM;

    rtc_alarm_config(RTC_ALARM0, &rtc_alarm);

    rtc_flag_clear(RTC_FLAG_ALRM0);
    rtc_interrupt_enable(RTC_INT_ALARM0);
    rtc_alarm_enable(RTC_ALARM0);

    /* EXTI line 17 = RTC ALARM；深度睡眠下需要 EXTI + NVIC 才能唤醒 */
    exti_init(EXTI_17, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    exti_interrupt_flag_clear(EXTI_17);
    nvic_irq_enable(RTC_Alarm_IRQn, 1U, 0U);
}

void sys_sleep_execute_if_pending(void)
{
    if (!s_pending) return;

    uint32_t secs = s_seconds;
    s_pending = 0U;

    configure_rtc_alarm(secs);

    /* 进入深度睡眠，唤醒后内部已重新初始化外设 */
    bsp_enter_deepsleep();

    /* 唤醒后清闹钟，避免下一次睡眠时残留状态 */
    rtc_alarm_disable(RTC_ALARM0);
    rtc_interrupt_disable(RTC_INT_ALARM0);
    rtc_flag_clear(RTC_FLAG_ALRM0);
    exti_interrupt_flag_clear(EXTI_17);

    /* 唤醒应答：纯 ASCII 字符串（非帧封装） */
    rs_usart_send(SYS_SLEEP_WAKE_STRING, (uint16_t)strlen(SYS_SLEEP_WAKE_STRING));
}

/* RTC ALARM0 中断处理（覆盖 startup_*.s 中的弱符号）。
   只需要清除标志让 NVIC 复位 pending，CPU 自动从 WFI 返回。 */
void RTC_Alarm_IRQHandler(void)
{
    if (rtc_flag_get(RTC_FLAG_ALRM0) != RESET) {
        rtc_flag_clear(RTC_FLAG_ALRM0);
    }
    exti_interrupt_flag_clear(EXTI_17);
}
