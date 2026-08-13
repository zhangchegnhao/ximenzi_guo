/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/05/15
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"
#include "sys_storage.h"
#include "sys_alarm.h"
#include "sys_baudrate.h"
#include "sys_device.h"
#include "sys_ota.h"
#include "bl_partition.h"
#include "ota_uart.h"

int main(void)
{
#ifdef __FIRMWARE_VERSION_DEFINE
    uint32_t fw_ver = 0;
#endif
    /* APP 运行在 APP1 分区，需把向量表偏移指到 APP1 起始地址，
       否则启动后中断仍跳到 0x08000000 的 BootLoader 向量表。 */
    SCB->VTOR = BL_APP1_START_ADDR;
    __DSB();

    systick_config();
    init_cycle_counter(false);
    delay_ms(200); // Wait download if SWIO be set to GPIO

#ifdef __FIRMWARE_VERSION_DEFINE
    fw_ver = gd32f4xx_firmware_version_get();
#endif /* __FIRMWARE_VERSION_DEFINE */

    bsp_led_init();
    bsp_btn_init();
    bsp_oled_init();
    bsp_gd25qxx_init();
    bsp_usart_all_init();

    /* OTA 接收状态与 32KB ringbuffer 必须在 USART1 IRQ 喂数据前初始化，
       否则首次 IDLE 中断的 ring_buffer_write 会越界访问。 */
    ota_uart_reset_state();

    my_printf(DEBUG_USART, "BOOT: start\r\n");

    my_printf(DEBUG_USART, "BOOT: gd30 init...\r\n");
    bsp_gd30ad3344_init();
    my_printf(DEBUG_USART, "BOOT: gd30 done\r\n");

    my_printf(DEBUG_USART, "BOOT: adc init...\r\n");
    bsp_adc_init();
    my_printf(DEBUG_USART, "BOOT: adc done\r\n");

    my_printf(DEBUG_USART, "BOOT: dac init...\r\n");
    bsp_dac_init();
    my_printf(DEBUG_USART, "BOOT: dac done\r\n");

    my_printf(DEBUG_USART, "BOOT: adc1 init...\r\n");
    bsp_adc1_init();
    my_printf(DEBUG_USART, "BOOT: adc1 done\r\n");

    my_printf(DEBUG_USART, "BOOT: rtc init...\r\n");
    bsp_rtc_init();
    my_printf(DEBUG_USART, "BOOT: rtc done\r\n");

    sd_fatfs_init();
    app_btn_init();

    my_printf(DEBUG_USART, "BOOT: oled init...\r\n");
    OLED_Init();
    my_printf(DEBUG_USART, "BOOT: oled done\r\n");

    // test_spi_flash();  /* 调试用，注释以释放 sector 0；持久化区改用 0x000000 起始 */
#if SD_FATFS_DEMO_ENABLE
    sd_fatfs_test();
#else
    my_printf(DEBUG_USART, "BOOT: sd_fatfs_test skipped (SD_FATFS_DEMO_ENABLE=0)\r\n");
#endif

    sys_storage_init();
    sys_alarm_init();
    sys_ota_init();
    /* M-01: 用持久化 baud_code 覆盖 BSP 默认 19200（bsp_usart_all_init 已先执行） */
    sys_baudrate_apply(sys_device_get_baudrate_code());
    /* M-01 加固：等待 RS485 总线 idle 沉淀 + 让 PC 端有时间切换串口波特率，
       防止 A-03 上电心跳赶在 PC 切完前发出，导致 PC 收到 '?' 杂波。 */
    delay_ms(100);

    scheduler_init();
    while(1) {
        scheduler_run();
    }
}

#ifdef GD_ECLIPSE_GCC
/* retarget the C library printf function to the USART, in Eclipse GCC environment */
int __io_putchar(int ch)
{
    usart_data_transmit(EVAL_COM0, (uint8_t)ch);
    while(RESET == usart_flag_get(EVAL_COM0, USART_FLAG_TBE));
    return ch;
}
#else
/* retarget the C library printf function to the USART */
int fputc(int ch, FILE *f)
{
    usart_data_transmit(USART0, (uint8_t)ch);
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    return ch;
}
#endif /* GD_ECLIPSE_GCC */
