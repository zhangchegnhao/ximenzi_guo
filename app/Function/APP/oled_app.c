/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/06/05
* Note:
*/

#include "mcu_cimc_gd32f470vet6.h"
#include "sys_report.h"

extern uint16_t adc_value[2];

#define TEAM_ID_STR  "2026834095"

/**
 * @brief	使用类似printf的方式显示字符串，显示8x16大小的ASCII字符
 * @param x  Character position on the X-axis  range：0 - 127
 * @param y  Character position on the Y-axis  range：0 or 2 (128x32屏仅2行)
 * 例如：oled_printf(0, 0, "Data = %d", dat);
 **/
int oled_printf(uint8_t x, uint8_t y, const char *format, ...)
{
  char buffer[512]; // 临时存储格式化后的字符串
  va_list arg;      // 处理可变参数
  int len;          // 最终字符串长度

  va_start(arg, format);
  // 安全地格式化字符串到 buffer
  len = vsnprintf(buffer, sizeof(buffer), format, arg);
  va_end(arg);

  OLED_ShowStr(x, y, buffer, 16);
  return len;
}

/* 双行显示规范：
   第 1 行 (y=0)：队伍编号
   第 2 行 (y=2)：状态指示
     - 自动采集上报中 (sys_report_is_enabled) → "AutoSample"
     - 其余时刻                                 → "IDLE"
   工作在 Bootloader 区域的"Bootloader" 由 APP 在 0x0501 应答后、reset 前
   主动写入 OLED；SSD1306 显存复位不丢，BL 阶段自动保留该画面。 */
void oled_task(void)
{
    oled_printf(0, 0, TEAM_ID_STR);
    if (sys_report_is_enabled()) {
        oled_printf(0, 2, "AutoSample");
    } else {
        oled_printf(0, 2, "IDLE      "); /* 尾部空格覆盖上一帧"AutoSample"残留 */
    }
}

/* 在即将进入 BootLoader 之前由 uart_task 调用一次：
   把 OLED 显示成 BL 阶段所需的"队伍号 + Bootloader"。
   reset 不重置 OLED 控制器显存，BL 阶段无需自行驱动 OLED。 */
void oled_show_bootloader_screen(void)
{
    OLED_Clear();
    oled_printf(0, 0, TEAM_ID_STR);
    oled_printf(0, 2, "Bootloader");
}

/* CUSTOM EDIT */
