/* Function/sysFunction/sys_dac.c
 * 业务层封装 GD32 库 dac_data_set()。DAC0 已由 BSP 配置 TIMER5 触发，
 * 这里只负责把 raw 写入保持寄存器，下一次 TIMER5 update 即推到 PA4。
 */
#include "sys_dac.h"
#include "sys_storage.h"
#include "mcu_cimc_gd32f470vet6.h"

static uint16_t s_dac_raw = 0U;

void sys_dac_set_raw(uint16_t raw)
{
    if (raw > SYS_DAC_RAW_MAX) raw = SYS_DAC_RAW_MAX;
    s_dac_raw = raw;
    dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, raw);
    /* F-02：DAC 持久化。sys_storage 加载阶段的 restore 用 sys_dac_restore_raw，
       内部 s_loading 守卫阻止反向回写，不会无限递归。 */
    sys_storage_save();
}

uint16_t sys_dac_get_raw(void)
{
    return s_dac_raw;
}

void sys_dac_restore_raw(uint16_t raw)
{
    if (raw > SYS_DAC_RAW_MAX) raw = SYS_DAC_RAW_MAX;
    s_dac_raw = raw;
    dac_data_set(DAC0, DAC_OUT0, DAC_ALIGN_12B_R, raw);
    /* 不调 sys_storage_save：本函数仅在 storage_init 加载阶段调用，
       避免在 s_loading=1 状态下再次触发写入逻辑。 */
}
