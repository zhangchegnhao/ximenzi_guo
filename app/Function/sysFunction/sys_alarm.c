/* Function/sysFunction/sys_alarm.c
 * 告警检测、Flash 持久化、主动上报。
 *
 * Flash 布局：
 *   0x000000 sector 0 → sys_storage blob
 *   0x001000 sector 1 → 告警记录区（4KB / 24B = 170 条上限）
 *
 * 单条记录 24 字节（与 AlarmFlashEntry 一致）：
 *   magic(4) + utc(4) + ch(1) + reserved(3) + threshold(4) + value(4) + pad(4)
 * 写入规则：sector 顺序追加；首字段为 magic（valid 标记），扫描遇 magic 错误即视为末尾。
 *
 * 主动告警上报格式（非帧封装，遵 confine.md §2 直接 ASCII）：
 *   "YYYY-MM-DD HH:MM:SS | CHx | <threshold> | <value>\n"
 *
 * 通道扩展：现支持 CH0/CH1；CH2 接入位置已在 sys_alarm_tick 标记。
 */
#include "sys_alarm.h"
#include "sys_time.h"
#include "sys_sample.h"
#include "sys_param.h"
#include "sys_storage.h"
#include "usart_app.h"
#include "mcu_cimc_gd32f470vet6.h"
#include <stdio.h>
#include <string.h>

#define ALARM_FLASH_ADDR   0x001000U
#define ALARM_SECTOR_SIZE  4096U
#define ALARM_RECORD_SIZE  24U
#define ALARM_MAX_RECORDS  (ALARM_SECTOR_SIZE / ALARM_RECORD_SIZE)  /* 170 */
#define ALARM_MAGIC        0xA5C0FFEEU

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;            /* ALARM_MAGIC if entry valid */
    uint32_t utc_seconds;
    uint8_t  channel;
    uint8_t  reserved[3];
    float    threshold;
    float    value;
    uint32_t pad;              /* 对齐到 24 字节 */
} AlarmFlashEntry;
#pragma pack(pop)

/* 模块状态 */
static uint8_t  s_mode    = SYS_ALARM_MODE_DEFAULT;
static uint8_t  s_was_over[SYS_ALARM_CH_NUM] = {0U, 0U, 0U};
static uint16_t s_count   = 0U;

/* ── 内部辅助 ────────────────────────────────────────────────────────── */

static uint32_t addr_of_record(uint16_t idx)
{
    return ALARM_FLASH_ADDR + (uint32_t)idx * ALARM_RECORD_SIZE;
}

/* 扫描 Flash，确定当前已有的有效记录条数 */
static void rescan_count(void)
{
    s_count = 0U;
    for (uint16_t i = 0U; i < ALARM_MAX_RECORDS; i++) {
        uint32_t magic = 0U;
        spi_flash_buffer_read((uint8_t *)&magic, addr_of_record(i), sizeof(magic));
        if (magic != ALARM_MAGIC) break;
        s_count++;
    }
}

/* 写一条记录到 Flash 末尾；若满则擦整段重写 */
static void append_record(const AlarmFlashEntry *entry)
{
    if (s_count >= ALARM_MAX_RECORDS) {
        spi_flash_sector_erase(ALARM_FLASH_ADDR);
        s_count = 0U;
    }
    spi_flash_buffer_write((uint8_t *)entry, addr_of_record(s_count),
                           sizeof(AlarmFlashEntry));
    s_count++;
}

/* 手工把 float 按 X.XX 格式写入 dst，返回写入字符数。
   微lib snprintf 对 %.2f 支持不完整，绕开。 */
static uint16_t fmt_float_2dec(char *dst, float v)
{
    uint16_t n = 0U;

    if (v < 0.0f) { dst[n++] = '-'; v = -v; }

    /* 整数部分 + 半 LSB 四舍五入到 2 位小数 */
    int32_t  int_part  = (int32_t)v;
    float    frac      = v - (float)int_part;
    int32_t  frac_part = (int32_t)(frac * 100.0f + 0.5f);
    if (frac_part >= 100) { int_part += 1; frac_part -= 100; }

    /* 整数部分倒序入栈再正序出 */
    char tmp[12];
    uint8_t tlen = 0U;
    if (int_part == 0) {
        tmp[tlen++] = '0';
    } else {
        while (int_part > 0) {
            tmp[tlen++] = (char)('0' + (int_part % 10));
            int_part /= 10;
        }
    }
    while (tlen > 0U) {
        dst[n++] = tmp[--tlen];
    }

    dst[n++] = '.';
    dst[n++] = (char)('0' + (frac_part / 10) % 10);
    dst[n++] = (char)('0' + (frac_part % 10));
    return n;
}

/* 手工拼装告警 ASCII 行：YYYY-MM-DD HH:MM:SS | CHx | th | val\n
   绕开 microlib 的 printf 宽度/精度限制。
   时间戳来自记录字段 utc_seconds（反推自 Unix epoch），非当前 RTC。 */
uint16_t sys_alarm_format_record(const SysAlarmRecord *rec, char *buf, uint16_t size)
{
    if (rec == 0 || buf == 0 || size < 64U) return 0U;

    uint16_t n = 0U;

    /* 时间戳 19 字符（占 buf[0..18]） */
    sys_time_format_utc(rec->utc_seconds, buf, size);
    n = 19U;

    buf[n++] = ' '; buf[n++] = '|'; buf[n++] = ' ';
    buf[n++] = 'C'; buf[n++] = 'H';
    buf[n++] = (char)('0' + (rec->channel % 10U));
    buf[n++] = ' '; buf[n++] = '|'; buf[n++] = ' ';
    n += fmt_float_2dec(&buf[n], rec->threshold);
    buf[n++] = ' '; buf[n++] = '|'; buf[n++] = ' ';
    n += fmt_float_2dec(&buf[n], rec->value);
    buf[n++] = '\n';

    return n;
}

static void emit_active_alarm(const AlarmFlashEntry *e)
{
    SysAlarmRecord rec;
    rec.utc_seconds = e->utc_seconds;
    rec.channel     = e->channel;
    rec.threshold   = e->threshold;
    rec.value       = e->value;

    char msg[80];
    uint16_t n = sys_alarm_format_record(&rec, msg, sizeof(msg));
    if (n > 0U) {
        rs_usart_send(msg, n);
    }
}

/* 检查单个通道（上升沿触发记录 + 必要时上报） */
static void check_channel(uint8_t ch, float value, float threshold)
{
    if (ch >= SYS_ALARM_CH_NUM) return;

    uint8_t is_over = (value > threshold) ? 1U : 0U;
    if (is_over && !s_was_over[ch]) {
        AlarmFlashEntry entry;
        memset(&entry, 0, sizeof(entry));
        entry.magic       = ALARM_MAGIC;
        entry.utc_seconds = sys_time_get_utc_seconds();
        entry.channel     = ch;
        entry.threshold   = threshold;
        entry.value       = value;

        append_record(&entry);

        if (s_mode == SYS_ALARM_MODE_ACTIVE) {
            emit_active_alarm(&entry);
        }
    }
    s_was_over[ch] = is_over;
}

/* ── 对外 API ────────────────────────────────────────────────────────── */

void sys_alarm_init(void)
{
    rescan_count();
}

int sys_alarm_set_mode(uint8_t mode)
{
    if (mode != SYS_ALARM_MODE_ACTIVE && mode != SYS_ALARM_MODE_PASSIVE) {
        return -1;
    }
    s_mode = mode;
    sys_storage_save();
    return 0;
}

uint8_t sys_alarm_get_mode(void)
{
    return s_mode;
}

void sys_alarm_tick(void)
{
    check_channel(SYS_ALARM_CH0, sys_sample_get_ch0(), sys_param_get_ch0_thresh());
    check_channel(SYS_ALARM_CH1, sys_sample_get_ch1(), sys_param_get_ch1_thresh());
    /* TODO: CH2 在 PT100 接入后加：
       check_channel(SYS_ALARM_CH2, sys_sample_get_ch2(), sys_param_get_ch2_thresh()); */
}

uint16_t sys_alarm_count(void)
{
    return s_count;
}

int sys_alarm_get_record(uint16_t idx, SysAlarmRecord *out)
{
    if (out == 0 || idx >= s_count) return -1;
    AlarmFlashEntry entry;
    spi_flash_buffer_read((uint8_t *)&entry, addr_of_record(idx), sizeof(entry));
    if (entry.magic != ALARM_MAGIC) return -1;
    out->utc_seconds = entry.utc_seconds;
    out->channel     = entry.channel;
    out->threshold   = entry.threshold;
    out->value       = entry.value;
    return 0;
}

void sys_alarm_clear_all(void)
{
    /* I-04 修复：仅清空 Flash 记录，保留当前通道"是否已过阈"防抖状态。
       若清零 s_was_over[]，那么已经过阈的通道在下一个 tick 会被视为新上升沿，
       立刻把刚清掉的记录重新写回，导致 0x0602 查询又看见告警。 */
    spi_flash_sector_erase(ALARM_FLASH_ADDR);
    s_count = 0U;
}
