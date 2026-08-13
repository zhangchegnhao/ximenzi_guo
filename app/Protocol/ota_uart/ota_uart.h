#ifndef OTA_UART_H
#define OTA_UART_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 复位 OTA 串口接收状态。
 *
 * 清空 OTA 解析状态和软件环形缓冲区，但不会擦除已经写入下载缓存区的固件数据。
 *
 * @return 无
 */
void ota_uart_reset_state(void);

/**
 * @brief 向 OTA 接收器喂入串口原始数据。
 *
 * UART DMA 轮询层收到新字节后调用本函数。数据会先写入 OTA 软件环形缓冲区，
 * 后续由 ota_uart_task() 解析和写 Flash。
 *
 * @param[in] data 新收到的串口数据指针
 * @param[in] len data 中的数据长度，单位字节
 * @return 无
 */
void ota_uart_process_frame(const uint8_t *data, uint32_t len);

/**
 * @brief 执行 OTA 接收状态机。
 *
 * 本函数负责查找 ota_stream_header_t、校验 OTA 头、擦除下载缓存区、将后续
 * raw App.bin 写入 Flash、校验整包 CRC、提交 BootLoader 参数，并复位进入
 * BootLoader 完成安装。
 *
 * @return 无
 */
void ota_uart_task(void);

/**
 * @brief 标记"APP 主动触发升级请求"。N-01 (0x0501) 应答 OK 后调用。
 *
 * 仅设置内部 pending 标志，不立即写 Flash。真正擦写参数页由
 * ota_apply_fast_upgrade_mark_if_pending() 在 uart_task 应答发完之后执行，
 * 保证 OK 帧已经完整发出，再做 ~30ms 的 4KB Flash 擦写，最后软复位。
 */
void ota_request_fast_upgrade_mark(void);

/**
 * @brief 若 pending，则擦写 BL 参数页设 update_flag = BL_UPDATE_FLAG_FAST_UPGRADE。
 *
 * 必须在 OK 帧发完之后、NVIC_SystemReset() 之前调用。
 *
 * @return true  表示本次刚刚应用了一次 pending（且写入成功），调用方可据此
 *               触发"即将进入 BL"的副作用（如刷 OLED 为 Bootloader 画面）。
 * @return false 表示当前无 pending 或写入失败。
 */
bool ota_apply_fast_upgrade_mark_if_pending(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_UART_H */
