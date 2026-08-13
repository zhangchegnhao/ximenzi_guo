#include "mcu_cimc_gd32f470vet6.h"
#include "usart_app.h"
#include "ota_uart.h"
#include "ring_buffer.h"
#include "bl_partition.h"
#include "bl_param.h"
#include "ota_protocol.h"
#include "sys_ota.h"
#include "sys_device.h"
#include "perf_counter.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* 1 工程统一使用 perf_counter 的 get_system_ms() 取得毫秒时间戳 */
#define systick_get_ms()  ((uint32_t)get_system_ms())

#define OTA_RING_BUFFER_SIZE        (32 * 1024U)
#define OTA_STREAM_WRITE_CHUNK      512U
#define OTA_CONTEST_MAGIC           0x3CC3A55AUL
#define OTA_CONTEST_IDLE_DONE_MS    1500U

typedef enum
{
    OTA_STATE_WAIT_HEADER = 0,
    OTA_STATE_RECV_STREAM
} ota_state_t;

typedef enum
{
    OTA_PACKAGE_WITH_HEADER = 0,
    OTA_PACKAGE_CONTEST_RAW
} ota_package_type_t;

typedef struct
{
    ota_state_t state;
    ota_package_type_t package_type;
    uint32_t app_version;
    uint32_t expected_size;
    uint32_t expected_crc32;
    uint32_t recv_size;
    uint32_t crc_acc;
    uint32_t last_rx_ms;
    bool rx_overflow;
} ota_context_t;

static ota_context_t s_ota_ctx;
static uint8_t s_param_page_cache[BL_PARAM_PAGE_SIZE];
static uint8_t s_ring_buffer_storage[OTA_RING_BUFFER_SIZE];
static ring_buffer_t s_ring_buffer;

/* N-01 / fast upgrade 跨复位标志的 pending 位。dispatch 收 0x0501 时置 1，
   uart_task 在 OK 帧发出之后、reboot 之前调 apply 真正擦写参数页。 */
static volatile bool s_fast_upgrade_mark_pending = false;

/* 增量计算 CRC32：OTA 头校验、固件整包校验、Boot 参数校验都复用这个算法。 */
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t j;

    for(i = 0U; i < len; i++) {
        crc ^= data[i];
        for(j = 0U; j < 8U; j++) {
            crc = ((crc & 1U) != 0U) ? ((crc >> 1U) ^ 0xEDB88320UL) : (crc >> 1U);
        }
    }

    return crc;
}

/* 结束 CRC32 计算：对累计值取反，得到最终 CRC32。 */
static uint32_t crc32_finalize(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFUL;
}

/* 一次性计算一段数据的 CRC32，主要用于 OTA 头和参数页校验。 */
static uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    return crc32_finalize(crc32_update(0xFFFFFFFFUL, data, len));
}

/* 计算 BootLoader 参数结构体 CRC，不包含 param_crc32 字段本身。 */
static uint32_t param_calc_crc(const bl_param_t *param)
{
    return crc32_calc((const uint8_t *)param, (uint32_t)offsetof(bl_param_t, param_crc32));
}

/* 等待片内 Flash 控制器空闲，擦写 Flash 前后都要确认 BUSY 已释放。 */
__attribute__((section(".ramfunc"), noinline))
static bool flash_wait_ready(void)
{
    uint32_t timeout = 0x3FFFFFUL;

    while((RESET != fmc_flag_get(FMC_FLAG_BUSY)) && (timeout > 0U)) {
        timeout--;
    }

    return (timeout > 0U);
}

/* 清除 Flash 控制器状态标志，避免上一次擦写错误影响下一次操作。 */
__attribute__((section(".ramfunc"), noinline))
static void flash_clear_flags(void)
{
    fmc_flag_clear(FMC_FLAG_END);
    fmc_flag_clear(FMC_FLAG_WPERR);
    fmc_flag_clear(FMC_FLAG_PGSERR);
    fmc_flag_clear(FMC_FLAG_PGMERR);
}

/* 按页擦除片内 Flash，OTA 开始时擦下载缓存区，提交参数时擦参数页。 */
__attribute__((section(".ramfunc"), noinline))
static bool flash_erase_pages(uint32_t start_addr, uint32_t size)
{
    uint32_t end_addr;
    uint32_t page_addr;

    if((size == 0U) || (start_addr < BL_FLASH_BASE_ADDR)) {
        return false;
    }

    end_addr = start_addr + size - 1U;
    if(end_addr > BL_FLASH_END_ADDR) {
        return false;
    }

    page_addr = start_addr - (start_addr % BL_FLASH_PAGE_SIZE);
    end_addr = end_addr - (end_addr % BL_FLASH_PAGE_SIZE);

    fmc_unlock();
    flash_clear_flags();

    while(page_addr <= end_addr) {
        fmc_page_erase(page_addr);
        if(!flash_wait_ready()) {
            fmc_lock();
            return false;
        }
        flash_clear_flags();
        page_addr += BL_FLASH_PAGE_SIZE;
    }

    fmc_lock();
    return true;
}

/* 向片内 Flash 写入字节流，优先按 word 写，末尾不对齐部分按 byte 写。 */
__attribute__((section(".ramfunc"), noinline))
static bool flash_program_bytes(uint32_t addr, const uint8_t *data, uint32_t size)
{
    uint32_t i;
    uint32_t word_value;

    if((data == NULL) || (size == 0U)) {
        return false;
    }

    if((addr < BL_FLASH_BASE_ADDR) || ((addr + size - 1U) > BL_FLASH_END_ADDR)) {
        return false;
    }

    fmc_unlock();
    flash_clear_flags();

    while(((addr & 3UL) == 0UL) && (size >= 4U)) {
        memcpy(&word_value, data, sizeof(word_value));
        fmc_word_program(addr, word_value);
        if(!flash_wait_ready()) {
            fmc_lock();
            return false;
        }
        addr += 4UL;
        data += 4U;
        size -= 4U;
    }

    for(i = 0U; i < size; i++) {
        fmc_byte_program(addr + i, data[i]);
        if(!flash_wait_ready()) {
            fmc_lock();
            return false;
        }
    }

    flash_clear_flags();
    fmc_lock();
    return true;
}

static bool ota_vector_is_valid(uint32_t addr)
{
    uint32_t stack;
    uint32_t reset;

    memcpy(&stack, (const void *)addr, sizeof(stack));
    memcpy(&reset, (const void *)(addr + 4UL), sizeof(reset));

    if((stack & 0x2FFE0000UL) != 0x20000000UL) {
        return false;
    }

    return (reset >= BL_APP1_START_ADDR) && (reset <= BL_APP1_END_ADDR);
}

/* 当主/备参数都无效时，生成一份最小可用的默认 Boot 参数。 */
static void param_set_default(bl_param_t *param)
{
    memset(param, 0, sizeof(bl_param_t));
    param->magic = BL_PARAM_MAGIC;
    param->version = BL_PARAM_VERSION;
    param->update_flag = BL_UPDATE_FLAG_IDLE;
    param->app1_addr = BL_APP1_START_ADDR;
    param->app2_addr = BL_APP2_START_ADDR;
    param->tail_magic = BL_PARAM_TAIL_MAGIC;
    param->param_crc32 = param_calc_crc(param);
}

/* 只检查参数页关键字段是否像一份有效参数，后续会重新计算并写入 CRC。 */
static bool param_is_min_valid(const bl_param_t *param)
{
    return (param->magic == BL_PARAM_MAGIC) &&
           (param->tail_magic == BL_PARAM_TAIL_MAGIC) &&
           (param->version == BL_PARAM_VERSION);
}

/* OTA 完成后写 Boot 参数：标记 PENDING，让 BootLoader 复位后安装缓存区固件。 */
static bool param_commit_update(uint32_t app_size, uint32_t app_crc32)
{
    bl_param_t main_param;
    bl_param_t backup_param;

    memcpy(s_param_page_cache, (const void *)BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE);
    memcpy(&main_param, (const void *)BL_PARAM_MAIN_ADDR, sizeof(bl_param_t));
    memcpy(&backup_param, (const void *)BL_PARAM_BACKUP_ADDR, sizeof(bl_param_t));

    if(!param_is_min_valid(&main_param)) {
        if(param_is_min_valid(&backup_param)) {
            main_param = backup_param;
        } else {
            param_set_default(&main_param);
        }
    }

    main_param.app_size = app_size;
    main_param.app_crc32 = app_crc32;
    main_param.app1_addr = BL_APP1_START_ADDR;
    main_param.app2_addr = BL_APP2_START_ADDR;
    main_param.update_flag = BL_UPDATE_FLAG_PENDING;
    main_param.last_error = BL_ERR_NONE;
    main_param.param_crc32 = param_calc_crc(&main_param);
    backup_param = main_param;

    memcpy(&s_param_page_cache[BL_PARAM_MAIN_ADDR - BL_PARAM_PAGE_ADDR], &main_param, sizeof(bl_param_t));
    memcpy(&s_param_page_cache[BL_PARAM_BACKUP_ADDR - BL_PARAM_PAGE_ADDR], &backup_param, sizeof(bl_param_t));

    if(!flash_erase_pages(BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE)) {
        return false;
    }

    return flash_program_bytes(BL_PARAM_PAGE_ADDR, s_param_page_cache, BL_PARAM_PAGE_SIZE);
}

/* 复位 OTA 接收状态机和软件环形缓冲区，不擦除已下载到 Flash 的内容。 */
void ota_uart_reset_state(void)
{
    memset(&s_ota_ctx, 0, sizeof(s_ota_ctx));
    s_ota_ctx.state = OTA_STATE_WAIT_HEADER;
    s_ota_ctx.crc_acc = 0xFFFFFFFFUL;
    ring_buffer_init(&s_ring_buffer, s_ring_buffer_storage, OTA_RING_BUFFER_SIZE);
}

/* 校验串口文件最前面的 OTA 头：魔术字、版本、目标地址、镜像类型、大小和头 CRC。 */
static bool ota_parse_stream_header(const ota_stream_header_t *stream)
{
    uint32_t crc_expect;

    if(stream == NULL) {
        return false;
    }

    if((stream->magic != OTA_FRAME_MAGIC) ||
       (stream->header_version != OTA_STREAM_HEADER_VERSION) ||
       (stream->header_size != sizeof(ota_stream_header_t)) ||
       (stream->target_addr != BL_APP1_START_ADDR) ||
       (stream->image_type != OTA_IMAGE_TYPE_APP1) ||
       (stream->app_size == 0U) ||
       (stream->app_size > BL_APP2_SIZE)) {
        return false;
    }

    crc_expect = crc32_calc((const uint8_t *)stream,
                            (uint32_t)offsetof(ota_stream_header_t, header_crc32));
    return (crc_expect == stream->header_crc32);
}

/* OTA 头合法后进入接收状态，并先擦除下载缓存区，准备写入后续 raw BIN。 */
static bool ota_begin_stream(const ota_stream_header_t *stream)
{
    uint32_t erase_size;

    erase_size = (stream->app_size + BL_FLASH_PAGE_SIZE - 1U) & ~(BL_FLASH_PAGE_SIZE - 1U);
    if(!flash_erase_pages(BL_APP2_START_ADDR, erase_size)) {
        return false;
    }

    s_ota_ctx.state = OTA_STATE_RECV_STREAM;
    s_ota_ctx.package_type = OTA_PACKAGE_WITH_HEADER;
    s_ota_ctx.app_version = stream->app_version;
    s_ota_ctx.expected_size = stream->app_size;
    s_ota_ctx.expected_crc32 = stream->app_crc32;
    s_ota_ctx.recv_size = 0U;
    s_ota_ctx.crc_acc = 0xFFFFFFFFUL;
    s_ota_ctx.last_rx_ms = systick_get_ms();
    s_ota_ctx.rx_overflow = false;
    return true;
}

static bool ota_begin_contest_stream(void)
{
    if(!flash_erase_pages(BL_APP2_START_ADDR, BL_APP2_SIZE)) {
        return false;
    }

    s_ota_ctx.state = OTA_STATE_RECV_STREAM;
    s_ota_ctx.package_type = OTA_PACKAGE_CONTEST_RAW;
    s_ota_ctx.app_version = 0U;
    s_ota_ctx.expected_size = BL_APP2_SIZE;
    s_ota_ctx.expected_crc32 = 0U;
    s_ota_ctx.recv_size = 0U;
    s_ota_ctx.crc_acc = 0xFFFFFFFFUL;
    s_ota_ctx.last_rx_ms = systick_get_ms();
    s_ota_ctx.rx_overflow = false;
    return true;
}

/* 将一段固件数据写入下载缓存区，同时累计整包 CRC 和已接收大小。 */
static bool ota_write_payload(const uint8_t *payload, uint32_t len)
{
    uint32_t flash_addr;

    if((payload == NULL) || (len == 0U)) {
        return true;
    }

    if((s_ota_ctx.recv_size + len) > s_ota_ctx.expected_size) {
        my_printf(DEBUG_USART, "OTA: size overflow\r\n");
        return false;
    }

    flash_addr = BL_APP2_START_ADDR + s_ota_ctx.recv_size;
    if(!flash_program_bytes(flash_addr, payload, len)) {
        my_printf(DEBUG_USART, "OTA: flash write failed\r\n");
        return false;
    }

    s_ota_ctx.crc_acc = crc32_update(s_ota_ctx.crc_acc, payload, len);
    s_ota_ctx.recv_size += len;
    s_ota_ctx.last_rx_ms = systick_get_ms();
    return true;
}

/* 固件接收完成后做最终检查：大小、溢出标志、整包 CRC，然后提交 Boot 参数。 */
static bool ota_finalize_if_done(void)
{
    uint32_t crc_value;

    if((s_ota_ctx.package_type == OTA_PACKAGE_WITH_HEADER) &&
       (s_ota_ctx.recv_size != s_ota_ctx.expected_size)) {
        return false;
    }

    if((s_ota_ctx.package_type == OTA_PACKAGE_CONTEST_RAW) &&
       (s_ota_ctx.recv_size < 8U)) {
        my_printf(DEBUG_USART, "OTA: contest image too small\r\n");
        return false;
    }

    if(s_ota_ctx.rx_overflow) {
        my_printf(DEBUG_USART, "OTA: rx overflow\r\n");
        return false;
    }

    crc_value = crc32_finalize(s_ota_ctx.crc_acc);
    if((s_ota_ctx.package_type == OTA_PACKAGE_WITH_HEADER) &&
       (crc_value != s_ota_ctx.expected_crc32)) {
        my_printf(DEBUG_USART, "OTA: crc mismatch, exp=0x%08X got=0x%08X\r\n",
                  s_ota_ctx.expected_crc32, crc_value);
        return false;
    }

    if(!ota_vector_is_valid(BL_APP2_START_ADDR)) {
        my_printf(DEBUG_USART, "OTA: invalid app vector\r\n");
        return false;
    }

    if(!param_commit_update(s_ota_ctx.recv_size, crc_value)) {
        my_printf(DEBUG_USART, "OTA: param commit failed\r\n");
        return false;
    }

    return true;
}

/* 从 ringbuffer 连续取出 BIN 数据写 Flash；写满声明大小后复位进入 BootLoader。 */
static bool ota_stream_consume_data(void)
{
    uint8_t payload[OTA_STREAM_WRITE_CHUNK];
    uint32_t available;
    uint32_t remain;
    uint32_t chunk_size;

    while(s_ota_ctx.state == OTA_STATE_RECV_STREAM) {
        available = ring_buffer_available(&s_ring_buffer);
        if(available == 0U) {
            if((s_ota_ctx.package_type == OTA_PACKAGE_CONTEST_RAW) &&
               (s_ota_ctx.recv_size > 0U) &&
               ((systick_get_ms() - s_ota_ctx.last_rx_ms) >= OTA_CONTEST_IDLE_DONE_MS)) {
                if(!ota_finalize_if_done()) {
                    return false;
                }
                my_printf(DEBUG_USART, "OTA: contest done, reset to bootloader\r\n");
                __disable_irq();
                NVIC_SystemReset();
            }
            return true;
        }

        remain = s_ota_ctx.expected_size - s_ota_ctx.recv_size;
        if(remain == 0U) {
            my_printf(DEBUG_USART, "OTA: size limit reached\r\n");
            return false;
        }

        chunk_size = (available < remain) ? available : remain;
        if(chunk_size > OTA_STREAM_WRITE_CHUNK) {
            chunk_size = OTA_STREAM_WRITE_CHUNK;
        }

        (void)ring_buffer_read(&s_ring_buffer, payload, chunk_size);
        if(!ota_write_payload(payload, chunk_size)) {
            my_printf(DEBUG_USART, "OTA: write failed at %u\r\n", s_ota_ctx.recv_size);
            return false;
        }

        if((s_ota_ctx.package_type == OTA_PACKAGE_WITH_HEADER) &&
           (s_ota_ctx.recv_size == s_ota_ctx.expected_size)) {
            if(!ota_finalize_if_done()) {
                return false;
            }
            my_printf(DEBUG_USART, "OTA: done, reset to bootloader\r\n");
            __disable_irq();
            NVIC_SystemReset();
        }
    }

    return true;
}

/* UART DMA 轮询层把新收到的原始字节交给这里，先只放入软件 ringbuffer。 */
void ota_uart_process_frame(const uint8_t *data, uint32_t len)
{
    if((data == NULL) || (len == 0U)) {
        return;
    }

    /* N-02：0x0502 验证态下，sys_ota 抢占字节流，不进入流式 OTA 接收。
       避免错误固件包前 4B（魔术字校验对象）反而被流式接收识别成 contest 头。 */
    if (sys_ota_is_verifying()) {
        sys_ota_feed(data, (uint16_t)len);
        return;
    }

    if(!ring_buffer_write(&s_ring_buffer, data, len)) {
        s_ota_ctx.rx_overflow = true;
        my_printf(DEBUG_USART, "OTA: ring buffer overflow\r\n");
    }
}

/* N-01 触发：dispatch 收 0x0501 时调用，仅置 pending，不立即写 Flash。
   真正擦写在 OK 应答帧发出之后由 ota_apply_fast_upgrade_mark_if_pending 完成。 */
void ota_request_fast_upgrade_mark(void)
{
    s_fast_upgrade_mark_pending = true;
}

/* uart_task 应答发完后调用：若 pending，则把 BL 参数页 update_flag 改成
   BL_UPDATE_FLAG_FAST_UPGRADE，主备两份同步更新；不破坏其它字段。
   复用 main_param 现有内容（含 update_counter / app_size 等），只改 flag。 */
bool ota_apply_fast_upgrade_mark_if_pending(void)
{
    bl_param_t main_param;
    bl_param_t backup_param;

    if (!s_fast_upgrade_mark_pending) {
        return false;   /* 无 pending → 调用方不必触发"进 BL"副作用 */
    }
    s_fast_upgrade_mark_pending = false;

    memcpy(s_param_page_cache, (const void *)BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE);
    memcpy(&main_param,   (const void *)BL_PARAM_MAIN_ADDR,   sizeof(bl_param_t));
    memcpy(&backup_param, (const void *)BL_PARAM_BACKUP_ADDR, sizeof(bl_param_t));

    /* 任一份有效就用它做底；都失效则用默认（与 param_commit_update 同策略）。 */
    if (!param_is_min_valid(&main_param)) {
        if (param_is_min_valid(&backup_param)) {
            main_param = backup_param;
        } else {
            param_set_default(&main_param);
        }
    }

    /* 已经 PENDING（正在升级中）就不再覆盖，避免丢失安装请求。 */
    if (main_param.update_flag == BL_UPDATE_FLAG_PENDING) {
        return true;
    }

    main_param.update_flag    = BL_UPDATE_FLAG_FAST_UPGRADE;
    main_param.last_error     = BL_ERR_NONE;
    main_param.app1_addr      = BL_APP1_START_ADDR;
    main_param.app2_addr      = BL_APP2_START_ADDR;
    /* N-全套：把 APP 当前 baud_code 写进参数页，让 BL 启动后切到同一波特率。
       BL 是看到 magic/version/tail 都对就信任 app_baud_code，无效码兜底 19200。 */
    main_param.app_baud_code  = (uint32_t)sys_device_get_baudrate_code();
    main_param.param_crc32    = param_calc_crc(&main_param);
    backup_param = main_param;

    memcpy(&s_param_page_cache[BL_PARAM_MAIN_ADDR   - BL_PARAM_PAGE_ADDR], &main_param,   sizeof(bl_param_t));
    memcpy(&s_param_page_cache[BL_PARAM_BACKUP_ADDR - BL_PARAM_PAGE_ADDR], &backup_param, sizeof(bl_param_t));

    if (!flash_erase_pages(BL_PARAM_PAGE_ADDR, BL_PARAM_PAGE_SIZE)) {
        return false;
    }
    return flash_program_bytes(BL_PARAM_PAGE_ADDR, s_param_page_cache, BL_PARAM_PAGE_SIZE);
}

/* OTA 主任务：找 OTA 头、启动接收、持续消费 BIN 数据，是整个 OTA 接收状态机入口。 */
void ota_uart_task(void)
{
    ota_stream_header_t stream_header;
    uint32_t available;

    while(1) {
        if(s_ota_ctx.state == OTA_STATE_RECV_STREAM) {
            if(!ota_stream_consume_data()) {
                ota_uart_reset_state();
            }
            return;
        }

        available = ring_buffer_available(&s_ring_buffer);
        if(available < sizeof(uint32_t)) {
            return;
        }

        (void)ring_buffer_peek(&s_ring_buffer, (uint8_t *)&stream_header.magic, sizeof(stream_header.magic));
        if(stream_header.magic == OTA_CONTEST_MAGIC) {
            ring_buffer_drop(&s_ring_buffer, sizeof(stream_header.magic));
            if(!ota_begin_contest_stream()) {
                my_printf(DEBUG_USART, "OTA: contest begin failed\r\n");
                ota_uart_reset_state();
                return;
            }
            continue;
        }

        if(stream_header.magic != OTA_FRAME_MAGIC) {
            ring_buffer_drop(&s_ring_buffer, 1U);
            continue;
        }

        if(available < sizeof(ota_stream_header_t)) {
            return;
        }

        (void)ring_buffer_peek(&s_ring_buffer, (uint8_t *)&stream_header, sizeof(stream_header));
        if(!ota_parse_stream_header(&stream_header)) {
            ring_buffer_drop(&s_ring_buffer, 1U);
            continue;
        }

        ring_buffer_drop(&s_ring_buffer, (uint32_t)sizeof(ota_stream_header_t));
        if(!ota_begin_stream(&stream_header)) {
            my_printf(DEBUG_USART, "OTA: begin failed\r\n");
            ota_uart_reset_state();
            return;
        }
    }
}
