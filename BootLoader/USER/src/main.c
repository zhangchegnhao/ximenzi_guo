/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include <string.h>
#include "mcu_cimc_gd32f470vet6.h"
#include "oled.h"
#include "bl_core.h"
#include "bl_param.h"

#define TEAM_ID_STR  "2026834095"

/* baud_code → 实际波特率值；与 APP 端 sys_baudrate_code_to_value 严格一致。
   非法码 / 0 / 0xFFFFFFFF 都返回 0，调用方据此走默认。 */
static uint32_t bl_baud_code_to_value(uint32_t code)
{
    switch (code) {
        case 0x11U: return 4800U;
        case 0x12U: return 9600U;
        case 0x13U: return 19200U;
        case 0x14U: return 115200U;
        default:    return 0U;
    }
}

/* 从 BL 参数页主备两份中取出 app_baud_code，把 USART1 切到该波特率。
   - 参数页 magic/tail 任一不匹配 → 保持 bsp_usart_init 默认 19200
   - app_baud_code 非法 → 同上 */
static void bl_apply_persistent_baud(void)
{
    bl_param_t p;
    uint32_t   baud;

    memcpy(&p, (const void *)BL_PARAM_MAIN_ADDR, sizeof(p));
    if (p.magic != BL_PARAM_MAGIC || p.tail_magic != BL_PARAM_TAIL_MAGIC) {
        /* 主份失效尝试备份 */
        memcpy(&p, (const void *)BL_PARAM_BACKUP_ADDR, sizeof(p));
        if (p.magic != BL_PARAM_MAGIC || p.tail_magic != BL_PARAM_TAIL_MAGIC) {
            return;  /* 都无效 → 用默认 19200 */
        }
    }

    baud = bl_baud_code_to_value(p.app_baud_code);
    if (baud == 0U || baud == 19200U) {
        return;  /* 非法码或本来就是 19200 → 不动 */
    }

    usart_disable(DEBUG_USART);
    usart_baudrate_set(DEBUG_USART, baud);
    usart_enable(DEBUG_USART);
}

int main(void)
{
    systick_config();
    bsp_usart_init();
    /* N-全套：读 BL 参数页 app_baud_code，把 USART1 切到 APP 当前波特率，
       让 PC↔BL 通讯不受 M-01 影响。必须在 bsp_oled_init 之前完成，避免
       USART 切换过程中夹杂其它干扰。 */
    bl_apply_persistent_baud();

    /* OLED：BL 阶段始终显示 "队伍号 + Bootloader"。
       SSD1306 上电后需要完整 init 序列，复位（非掉电）情况下 APP 已经写过
       显存可保留，但 BL 自己 init 一次更稳健，避免冷启动黑屏。 */
    bsp_oled_init();
    OLED_Init();
    OLED_Clear();
    OLED_ShowStr(0, 0, TEAM_ID_STR, 16);
    OLED_ShowStr(0, 2, "Bootloader", 16);

    bootloader_run();

    while(1) {
    }
}
