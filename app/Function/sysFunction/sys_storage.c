/* Function/sysFunction/sys_storage.c
 * 外部 SPI Flash（GD25Qxx）持久化层。
 * 复用 spi_flash_* 已有封装，sector 大小 4KB。
 * 存储区起始地址：0x000000（test_spi_flash 已注释，sector 0 可用）
 * blob 含 magic + version + 所有持久化字段 + checksum，校验失败时回退默认。
 */
#include "sys_storage.h"
#include "sys_device.h"
#include "sys_param.h"
#include "sys_report.h"
#include "sys_alarm.h"
#include "sys_dac.h"
#include "mcu_cimc_gd32f470vet6.h"

#define STORAGE_FLASH_ADDR   0x000000U
#define STORAGE_MAGIC        0xCAFE5707U  /* v7：再次让旧 blob 失效（测评后 baud=115200 残留） */
#define STORAGE_VERSION      0x0010U

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t device_id;
    uint8_t  baud_code;
    uint8_t  report_intv_code;
    uint8_t  alarm_mode;
    uint8_t  reserved;
    float    ratio_ch0;
    float    ratio_ch1;
    float    thresh_ch0;
    float    thresh_ch1;
    uint16_t dac_raw;        /* v5：DAC0 输出 raw 值 (0~0x0FFF) */
    uint16_t reserved2;      /* 保留对齐 */
    uint32_t checksum;       /* 校验和：covers all preceding bytes */
} SysStorageBlob;
#pragma pack(pop)

/* 阻止 setter 在 init 阶段回写 Flash（避免恢复值时反复擦写） */
static uint8_t s_loading = 0U;

/* 简单字节和校验，覆盖除 checksum 字段外的所有字节 */
static uint32_t blob_checksum(const SysStorageBlob *b)
{
    const uint8_t *p = (const uint8_t *)b;
    uint32_t sum = 0U;
    uint32_t covered = sizeof(SysStorageBlob) - sizeof(uint32_t);
    for (uint32_t i = 0U; i < covered; i++) {
        sum += p[i];
    }
    return sum;
}

void sys_storage_save(void)
{
    /* 加载期间禁止回写 */
    if (s_loading) return;

    SysStorageBlob blob;
    blob.magic            = STORAGE_MAGIC;
    blob.version          = STORAGE_VERSION;
    blob.device_id        = sys_device_get_id();
    blob.baud_code        = sys_device_get_baudrate_code();
    blob.report_intv_code = sys_report_get_interval_code();
    blob.alarm_mode       = sys_alarm_get_mode();
    blob.reserved         = 0U;
    blob.ratio_ch0   = sys_param_get_ch0_ratio();
    blob.ratio_ch1   = sys_param_get_ch1_ratio();
    blob.thresh_ch0  = sys_param_get_ch0_thresh();
    blob.thresh_ch1  = sys_param_get_ch1_thresh();
    blob.dac_raw     = sys_dac_get_raw();
    blob.reserved2   = 0U;
    blob.checksum    = blob_checksum(&blob);

    /* 擦除 sector 后写入（GD25Qxx 单 sector 4KB，blob 远小于此） */
    spi_flash_sector_erase(STORAGE_FLASH_ADDR);
    spi_flash_buffer_write((uint8_t *)&blob, STORAGE_FLASH_ADDR, sizeof(blob));
}

void sys_storage_init(void)
{
    SysStorageBlob blob;
    spi_flash_buffer_read((uint8_t *)&blob, STORAGE_FLASH_ADDR, sizeof(blob));

    if (blob.magic == STORAGE_MAGIC
        && blob.version == STORAGE_VERSION
        && blob.checksum == blob_checksum(&blob)) {
        /* Flash 内有效 → 恢复到内存（s_loading 阻止 setter 回写） */
        s_loading = 1U;
        sys_device_set_id(blob.device_id);
        sys_device_set_baudrate_code(blob.baud_code);
        sys_param_set_ch0_ratio(blob.ratio_ch0);
        sys_param_set_ch1_ratio(blob.ratio_ch1);
        sys_param_set_ch0_thresh(blob.thresh_ch0);
        sys_param_set_ch1_thresh(blob.thresh_ch1);
        sys_report_set_interval_code(blob.report_intv_code);
        sys_alarm_set_mode(blob.alarm_mode);
        sys_dac_restore_raw(blob.dac_raw);
        s_loading = 0U;
    } else {
        /* 首次烧录 / 校验失败 → 写默认值（当前内存即默认值） */
        sys_storage_save();
    }
}
