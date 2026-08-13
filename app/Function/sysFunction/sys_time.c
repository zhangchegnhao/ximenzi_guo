/* Function/sysFunction/sys_time.c
 * 调用已封装的 rtc_current_time_get() 读取 RTC，做 BCD→十进制 + 日期→Unix 时间换算。
 * RTC 年范围 0x00-0x99 BCD，代表 2000-2099 年。
 */
#include "sys_time.h"
#include "mcu_cimc_gd32f470vet6.h"

extern rtc_parameter_struct rtc_initpara;

/* BCD 字节转十进制 */
static uint8_t bcd_to_dec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) & 0x0FU) * 10U + (bcd & 0x0FU));
}

uint8_t sys_time_bcd_to_dec(uint8_t bcd) { return bcd_to_dec(bcd); }

/* 公历闰年判定 */
static uint8_t is_leap_year(uint16_t y)
{
    return (uint8_t)((((y % 4U) == 0U) && ((y % 100U) != 0U)) || ((y % 400U) == 0U));
}

uint32_t sys_time_get_utc_seconds(void)
{
    /* 读取当前 RTC（GD32 已封装接口，输出为 BCD） */
    rtc_current_time_get(&rtc_initpara);

    uint16_t year = 2000U + (uint16_t)bcd_to_dec(rtc_initpara.year);
    uint8_t  mon  = bcd_to_dec(rtc_initpara.month);
    uint8_t  day  = bcd_to_dec(rtc_initpara.date);
    uint8_t  hh   = bcd_to_dec(rtc_initpara.hour);
    uint8_t  mm   = bcd_to_dec(rtc_initpara.minute);
    uint8_t  ss   = bcd_to_dec(rtc_initpara.second);

    /* 每月起始累计天数（非闰年） */
    static const uint16_t mdays_cumulative[12] = {
        0U, 31U, 59U, 90U, 120U, 151U, 181U, 212U, 243U, 273U, 304U, 334U
    };

    uint32_t days = 0U;

    /* 1970 → 当前年累计天数 */
    for (uint16_t y = 1970U; y < year; y++) {
        days += is_leap_year(y) ? 366U : 365U;
    }

    /* 当年 1 月 → 当前月起累计天数 */
    if (mon >= 1U && mon <= 12U) {
        days += mdays_cumulative[mon - 1U];
        if (mon > 2U && is_leap_year(year)) {
            days += 1U;
        }
    }

    /* 当月日数（date 字段从 1 开始） */
    if (day >= 1U) {
        days += (uint32_t)(day - 1U);
    }

    return days * 86400UL
         + (uint32_t)hh * 3600UL
         + (uint32_t)mm * 60UL
         + (uint32_t)ss;
}

/* 十进制 → BCD（仅适用 0-99） */
static uint8_t dec_to_bcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10U) << 4) | (dec % 10U));
}

uint8_t sys_time_dec_to_bcd(uint8_t dec) { return dec_to_bcd(dec); }

/* 当前月份的天数（考虑闰年） */
static uint8_t days_in_month(uint8_t mon, uint16_t year)
{
    static const uint8_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (mon < 1U || mon > 12U) return 0U;
    if (mon == 2U && is_leap_year(year)) return 29U;
    return mdays[mon - 1U];
}

int sys_time_set_utc_seconds(uint32_t ts)
{
    /* 1. 秒拆出时/分/秒 */
    uint32_t sec_of_day = ts % 86400UL;
    uint32_t days       = ts / 86400UL;

    uint8_t hh = (uint8_t)(sec_of_day / 3600U);
    uint8_t mm = (uint8_t)((sec_of_day % 3600U) / 60U);
    uint8_t ss = (uint8_t)(sec_of_day % 60U);

    /* 2. 天数累减算出年/月/日（基准 1970-01-01） */
    uint16_t year = 1970U;
    while (1) {
        uint16_t y_days = is_leap_year(year) ? 366U : 365U;
        if (days < y_days) break;
        days -= y_days;
        year++;
    }

    /* RTC 年存 BCD 0-99，限制 2000-2099 */
    if (year < 2000U || year > 2099U) return -1;

    uint8_t mon = 1U;
    while (mon <= 12U) {
        uint8_t m_days = days_in_month(mon, year);
        if (days < m_days) break;
        days -= m_days;
        mon++;
    }
    uint8_t day = (uint8_t)(days + 1U);   /* 天序号 1-31 */

    /* 3. 星期：1970-01-01 是周四（RTC_THURSDAY=4） */
    uint8_t dow = (uint8_t)(((ts / 86400UL) + 3U) % 7U + 1U);

    /* 4. 复用 BSP 已配的 rtc_initpara（保留分频系数 / 24 小时制 / 当前 AM 标志） */
    rtc_initpara.year        = dec_to_bcd((uint8_t)(year - 2000U));
    rtc_initpara.month       = mon;            /* RTC 月份与十进制等效（1-12） */
    rtc_initpara.date        = dec_to_bcd(day);
    rtc_initpara.day_of_week = dow;
    rtc_initpara.hour        = dec_to_bcd(hh);
    rtc_initpara.minute      = dec_to_bcd(mm);
    rtc_initpara.second      = dec_to_bcd(ss);

    /* 5. 写入硬件 */
    if (ERROR == rtc_init(&rtc_initpara)) {
        return -1;
    }
    return 0;
}

/* 写两位十进制数到 dst[0..1]（高位在前），不写终止符 */
static void put2(char *dst, uint8_t v)
{
    dst[0] = (char)('0' + (v / 10U) % 10U);
    dst[1] = (char)('0' + (v % 10U));
}

/* 手工组装 "YYYY-MM-DD HH:MM:SS"，绕开 microlib 不支持宽度修饰符的问题 */
void sys_time_format_datetime(char *buf, uint16_t size)
{
    if (buf == 0 || size < 20U) return;

    rtc_current_time_get(&rtc_initpara);

    uint16_t year = 2000U + (uint16_t)bcd_to_dec(rtc_initpara.year);
    uint8_t  mon  = bcd_to_dec(rtc_initpara.month);
    uint8_t  day  = bcd_to_dec(rtc_initpara.date);
    uint8_t  hh   = bcd_to_dec(rtc_initpara.hour);
    uint8_t  mm   = bcd_to_dec(rtc_initpara.minute);
    uint8_t  ss   = bcd_to_dec(rtc_initpara.second);

    buf[0]  = (char)('0' + (year / 1000U) % 10U);
    buf[1]  = (char)('0' + (year / 100U)  % 10U);
    buf[2]  = (char)('0' + (year / 10U)   % 10U);
    buf[3]  = (char)('0' + (year % 10U));
    buf[4]  = '-';
    put2(&buf[5],  mon);
    buf[7]  = '-';
    put2(&buf[8],  day);
    buf[10] = ' ';
    put2(&buf[11], hh);
    buf[13] = ':';
    put2(&buf[14], mm);
    buf[16] = ':';
    put2(&buf[17], ss);
    buf[19] = '\0';
}

void sys_time_format_utc(uint32_t ts, char *buf, uint16_t size)
{
    if (buf == 0 || size < 20U) return;

    /* 拆秒/分/时 */
    uint32_t sec_of_day = ts % 86400UL;
    uint32_t days       = ts / 86400UL;
    uint8_t  hh = (uint8_t)(sec_of_day / 3600U);
    uint8_t  mm = (uint8_t)((sec_of_day % 3600U) / 60U);
    uint8_t  ss = (uint8_t)(sec_of_day % 60U);

    /* 累减天数算年/月/日 */
    uint16_t year = 1970U;
    while (1) {
        uint16_t yd = is_leap_year(year) ? 366U : 365U;
        if (days < yd) break;
        days -= yd;
        year++;
    }
    uint8_t mon = 1U;
    while (mon <= 12U) {
        uint8_t md = days_in_month(mon, year);
        if (days < md) break;
        days -= md;
        mon++;
    }
    uint8_t day = (uint8_t)(days + 1U);

    buf[0]  = (char)('0' + (year / 1000U) % 10U);
    buf[1]  = (char)('0' + (year / 100U)  % 10U);
    buf[2]  = (char)('0' + (year / 10U)   % 10U);
    buf[3]  = (char)('0' + (year % 10U));
    buf[4]  = '-';
    put2(&buf[5],  mon);
    buf[7]  = '-';
    put2(&buf[8],  day);
    buf[10] = ' ';
    put2(&buf[11], hh);
    buf[13] = ':';
    put2(&buf[14], mm);
    buf[16] = ':';
    put2(&buf[17], ss);
    buf[19] = '\0';
}
