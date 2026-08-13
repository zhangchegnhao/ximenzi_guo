/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/06/05
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"
#include "sys_report.h"

/**
 * @brief LED 任务，由调度器以 500ms 周期调用。
 *
 * LED1 - 系统状态指示灯：1s 周期闪烁（每次 toggle = 500ms × 2）。
 *        scheduler 启动即 = 进入 APP 区域起算，无需额外触发。
 * LED2 - 采集工作指示灯：自动采集上报启用期间常亮，其余时刻熄灭。
 * LED3~LED6：未指派功能，全部熄灭。
 */
void led_ctrl_task(void)
{
    LED1_TOGGLE;

    if (sys_report_is_enabled()) {
        LED2_ON;
    } else {
        LED2_OFF;
    }

    LED3_OFF;
    LED4_OFF;
    LED5_OFF;
    LED6_OFF;
}
