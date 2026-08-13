/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/06/05
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"
#include "proto_dispatch.h"
#include "proto_frame.h"
#include "sys_device.h"
#include "sys_report.h"
#include "sys_time.h"
#include "sys_sample.h"
#include "sys_alarm.h"
#include "sys_sleep.h"
#include "sys_ota.h"
#include "oled_app.h"
#include "ota_uart.h"

#define RS_RESP_BUF_SIZE  (PROTO_ASCII_MAX_LEN)

__IO uint16_t tx_count = 0;
__IO uint8_t rx_flag = 0;
uint8_t uart_dma_buffer[512] = {0};

volatile uint8_t  rs_rx_flag = 0;
volatile uint16_t rs_rx_len  = 0;
extern uint8_t rs_rxbuffer[AUX_USART_RXBUFFER_SIZE];

int my_printf(uint32_t usart_periph, const char *format, ...)
{
    char buffer[512];
    va_list arg;
    int len;
    // Initialize variable argument list.
    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);
    
    for(tx_count = 0; tx_count < len; tx_count++){
        usart_data_transmit(usart_periph, buffer[tx_count]);
        while(RESET == usart_flag_get(usart_periph, USART_FLAG_TBE));
    }
    
    return len;
}

void rs_usart_send(const char *str, uint16_t len)
{
    RS485_CS_SET(1);
    /* RS485 DE 切换后给收发器留稳定时间，避免连续两次发送时第一字节被吞 */
    delay_us(50);

    for (uint16_t i = 0; i < len; i++) {
        usart_data_transmit(RS232_RS485_USART, (uint8_t)str[i]);
        while (RESET == usart_flag_get(RS232_RS485_USART, USART_FLAG_TBE));
    }
    while (RESET == usart_flag_get(RS232_RS485_USART, USART_FLAG_TC));

    /* 拉低 CS 前再给一点保留，确保最后一位完整推出总线 */
    delay_us(50);
    RS485_CS_SET(0);
}

void uart_task(void)
{
    /* Send startup heartbeat once after power-on / reset */
    if (sys_device_startup_hb_pending()) {
        sys_device_clear_startup_hb();
        char hb_buf[PROTO_ASCII_MIN_LEN + 1U];
        uint16_t hb_len = proto_build_hb(sys_device_get_id(), hb_buf, sizeof(hb_buf));
        if (hb_len > 0U) {
            rs_usart_send(hb_buf, hb_len);
        }
    }

    if (rx_flag) {
        my_printf(DEBUG_USART, "%s", uart_dma_buffer);
        memset(uart_dma_buffer, 0, sizeof(uart_dma_buffer));
        rx_flag = 0;
    }

    if (rs_rx_flag) {
        uint16_t len = rs_rx_len;
        rs_rx_flag = 0;
        rs_rx_len  = 0;

        char resp[RS_RESP_BUF_SIZE];
        uint16_t resp_len = proto_dispatch((const char *)rs_rxbuffer, len,
                                           resp, sizeof(resp));
        if (resp_len > 0U) {
            rs_usart_send(resp, resp_len);
        }

        memset(rs_rxbuffer, 0, len);

        /* N-01：reboot 前若有 fast-upgrade pending 标记，先擦写 BL 参数页。
           ~30ms 操作；放在 OK 帧已经发出之后、NVIC_SystemReset() 之前。
           写参数页成功后立刻把 OLED 刷成 "Bootloader" 画面，
           利用 SSD1306 显存复位不丢的特性，让 BL 阶段无需自驱 OLED。 */
        if (ota_apply_fast_upgrade_mark_if_pending()) {
            oled_show_bootloader_screen();
        }

        /* Execute any deferred action (e.g. reboot, sleep) after response is sent */
        sys_device_reboot_if_pending();
        sys_sleep_execute_if_pending();
    }

    /* Auto-report periodic check（首帧由 dispatch 应答返回，这里发后续帧） */
    {
        uint32_t now = get_system_ms();
        if (sys_report_should_send(now)) {
            char rpt[RS_RESP_BUF_SIZE];
            uint16_t rpt_len = proto_build_auto_report(
                sys_device_get_id(),
                sys_time_get_utc_seconds(),
                sys_sample_get_ch0(),
                sys_sample_get_ch1(),
                rpt, sizeof(rpt));
            if (rpt_len > 0U) {
                rs_usart_send(rpt, rpt_len);
                sys_report_mark_sent(now);
            }
        }
    }

    /* Alarm threshold check（每周期检查，仅上升沿触发记录 + 主动模式上报） */
    sys_alarm_tick();

    /* N-02: OTA 准备阶段验证状态机；空闲超时后取出 OK/ERROR 应答发出。
       必须在 ota_uart_task() 前调，确保字节流仍被 sys_ota 抢占期间不让流式 OTA 跑。 */
    {
        uint32_t ota_now = get_system_ms();
        sys_ota_tick(ota_now);
        char ota_resp[RS_RESP_BUF_SIZE];
        uint16_t ota_resp_len = sys_ota_take_pending_response(ota_resp,
                                                              sizeof(ota_resp));
        if (ota_resp_len > 0U) {
            rs_usart_send(ota_resp, ota_resp_len);
        }
    }

    /* OTA：识别 RS485 流中的 OTA 头并把后续字节写入下载缓存；
       接收完整后会自动提交 Boot 参数并复位，跳入 BootLoader 完成 APP1 更新。 */
    ota_uart_task();
}
