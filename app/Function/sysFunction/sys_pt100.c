/* Function/sysFunction/sys_pt100.c
 * CH2 PT100 温度采样：移植自参考工程 Pt100/APP/adc_app.c。
 * 复用已封装的 GD30AD3344_AD_Read（Driver/Components/gd30ad3344），
 * 不再另写 SPI/外设访问代码（遵 confine.md §6.2）。
 *
 * 转换链路：
 *   GD30AD3344 CH4 (±2.048V) → V_diff
 *   恒流源 1.006 mA 注入 RTD，差分前端增益 G_DIFF=1.60，INA G_INA=10.022
 *   V_RTD = TLV431_Vol - V_diff / G_DIFF；R = V_RTD / (G_INA * I_RTD_mA) * 1000
 *   再过 11 点分段线性标定表 → Callendar-Van Dusen 方程 → 摄氏度
 *
 * §4.1：CH2 暂未实现变比命令（0x0243 未定义），默认变比 1.0f，等同直接返回温度。
 *        若后续引入 ratio_ch2 仅需在此处补乘即可。
 */
#include "sys_pt100.h"
#include <math.h>
#include "gd30ad3344.h"

/* 与参考工程一致的硬件参数（确认正确，勿改） */
static const float TLV431_Vol = 1.816f;
static const float G_DIFF     = 1.60f;
static const float G_INA      = 10.02f;
static const float I_RTD_mA   = 1.006f;

typedef struct {
    float measured;
    float actual;
} resistance_cal_point_t;

/* PT100 实测电阻 → 实际电阻 标定表（与参考工程一致） */
static const resistance_cal_point_t R_CAL_TABLE[] = {
    {  81.40f,  80.6f },
    {  83.32f,  82.5f },
    { 100.91f, 100.0f },
    { 108.77f, 107.79f },
    { 114.10f, 113.0f },
    { 116.63f, 115.54f },
    { 124.38f, 123.24f },
    { 132.17f, 130.9f },
    { 139.88f, 138.51f },
    { 151.44f, 150.0f },
    { 155.43f, 154.0f },
};
#define R_CAL_POINT_NUM (sizeof(R_CAL_TABLE) / sizeof(R_CAL_TABLE[0]))

/* Callendar-Van Dusen 反解（T ≥ 0 分支） */
static float pt100_resistance_to_temp(float R)
{
    const float R0 = 100.0f;
    const float A  = 3.9083e-3f;
    const float B  = -5.775e-7f;

    float ratio = R / R0;
    float discriminant = A * A - 4.0f * B * (1.0f - ratio);

    if (discriminant < 0.0f) {
        return -999.0f;
    }
    return (-A + sqrtf(discriminant)) / (2.0f * B);
}

/* 11 点分段线性标定 */
static float pt100_r_calibration(float R)
{
    uint32_t i;
    const resistance_cal_point_t *p1;
    const resistance_cal_point_t *p2;

    if (R_CAL_POINT_NUM < 2U) {
        return R;
    }

    if (R <= R_CAL_TABLE[0].measured) {
        p1 = &R_CAL_TABLE[0];
        p2 = &R_CAL_TABLE[1];
    } else if (R >= R_CAL_TABLE[R_CAL_POINT_NUM - 1U].measured) {
        p1 = &R_CAL_TABLE[R_CAL_POINT_NUM - 2U];
        p2 = &R_CAL_TABLE[R_CAL_POINT_NUM - 1U];
    } else {
        p1 = &R_CAL_TABLE[R_CAL_POINT_NUM - 2U];
        p2 = &R_CAL_TABLE[R_CAL_POINT_NUM - 1U];
        for (i = 0U; i < R_CAL_POINT_NUM - 1U; i++) {
            if ((R >= R_CAL_TABLE[i].measured) &&
                (R <= R_CAL_TABLE[i + 1U].measured)) {
                p1 = &R_CAL_TABLE[i];
                p2 = &R_CAL_TABLE[i + 1U];
                break;
            }
        }
    }

    if (p2->measured == p1->measured) {
        return p1->actual;
    }
    return p1->actual + (R - p1->measured)
                       * (p2->actual - p1->actual)
                       / (p2->measured - p1->measured);
}

float sys_pt100_get_temp(void)
{
    float v_diff = GD30AD3344_AD_Read(GD30AD3344_Channel_4,
                                      GD30AD3344_PGA_2V048);
    float R = (TLV431_Vol - v_diff / G_DIFF) / G_INA / I_RTD_mA * 1000.0f;
    R = pt100_r_calibration(R);
    return pt100_resistance_to_temp(R);
}
