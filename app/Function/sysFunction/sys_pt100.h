#ifndef SYS_PT100_H
#define SYS_PT100_H

#ifdef __cplusplus
extern "C" {
#endif

/* CH2 PT100 温度采样模块
 * - 数据源：外部 ADC GD30AD3344（通道 4，PGA = ±2.048V）
 * - 移植自 Pt100 参考工程 APP/adc_app.c，公式 / 标定表参数原样保留
 * - 调用一次执行一次同步采样，无后台任务（GD30AD3344_AD_Read 内部阻塞）
 * 返回值：摄氏度（°C）；判别式 < 0 时返回 -999.0f 表示异常
 */
float sys_pt100_get_temp(void);

#ifdef __cplusplus
}
#endif

#endif /* SYS_PT100_H */
