#ifndef __OLED_APP_H__
#define __OLED_APP_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

int oled_printf(uint8_t x, uint8_t y, const char *format, ...);
void oled_task(void);

/* 进入 BootLoader 区域前刷一次 OLED：第 1 行队伍编号，第 2 行 "Bootloader"。
   由 uart_task 在 N-01 应答发出之后、reset 之前调用。 */
void oled_show_bootloader_screen(void);
/* CUSTOM EDIT */

#ifdef __cplusplus
}
#endif

#endif


