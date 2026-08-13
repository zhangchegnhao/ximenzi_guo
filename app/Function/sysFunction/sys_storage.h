#ifndef SYS_STORAGE_H
#define SYS_STORAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 上电时调用一次：读 Flash → 恢复 sys_param/sys_device 字段；
   若 Flash 无效（首次烧录或损坏）→ 写默认值。 */
void sys_storage_init(void);

/* 把当前 sys_param/sys_device 状态写入 Flash。
   由 sys_param/sys_device 的 setter 自动调用，应用层无需直接调用。 */
void sys_storage_save(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_STORAGE_H */
