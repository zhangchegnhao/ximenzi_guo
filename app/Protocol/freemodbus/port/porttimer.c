#include "port.h"
#include "mb.h"
#include "mbport.h"
#include "mcu_cimc_gd32f470vet6.h"

#define MB_TIMER_TICK_HZ  1000000UL

static uint32_t mb_timer6_clock_get(void)
{
    uint32_t ahb_hz = rcu_clock_freq_get(CK_AHB);
    uint32_t apb1_hz = rcu_clock_freq_get(CK_APB1);
    uint32_t apb1_prescaler = (RCU_CFG0 & RCU_CFG0_APB1PSC) >> 10U;

    if ((RCU_CFG1 & RCU_CFG1_TIMERSEL) != 0U) {
        return (apb1_prescaler <= 5U) ? ahb_hz : (apb1_hz * 4U);
    }
    return (apb1_prescaler <= 4U) ? ahb_hz : (apb1_hz * 2U);
}

BOOL xMBPortTimersInit(USHORT timeout_50us)
{
    timer_parameter_struct timer_initpara;
    uint32_t timer_clock_hz;
    uint32_t prescaler;
    uint32_t period_us;

    timer_clock_hz = mb_timer6_clock_get();
    if (timer_clock_hz < MB_TIMER_TICK_HZ || timeout_50us == 0U) {
        return FALSE;
    }

    prescaler = timer_clock_hz / MB_TIMER_TICK_HZ;
    period_us = (uint32_t)timeout_50us * 50UL;
    if (prescaler == 0U || prescaler > 65536UL || period_us == 0U) {
        return FALSE;
    }

    rcu_periph_clock_enable(RCU_TIMER6);
    timer_deinit(TIMER6);
    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler = (uint16_t)(prescaler - 1U);
    timer_initpara.period = period_us - 1U;
    timer_initpara.alignedmode = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection = TIMER_COUNTER_UP;
    timer_initpara.clockdivision = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0U;
    timer_init(TIMER6, &timer_initpara);

    nvic_irq_enable(TIMER6_IRQn, 4U, 0U);
    return TRUE;
}

void vMBPortTimersEnable(void)
{
    timer_disable(TIMER6);
    timer_counter_value_config(TIMER6, 0U);
    timer_interrupt_flag_clear(TIMER6, TIMER_INT_FLAG_UP);
    timer_interrupt_enable(TIMER6, TIMER_INT_UP);
    timer_enable(TIMER6);
}

void vMBPortTimersDisable(void)
{
    timer_interrupt_disable(TIMER6, TIMER_INT_UP);
    timer_interrupt_flag_clear(TIMER6, TIMER_INT_FLAG_UP);
    timer_disable(TIMER6);
}

void mb_port_timer_irq_handler(void)
{
    if (timer_interrupt_flag_get(TIMER6, TIMER_INT_FLAG_UP) != RESET) {
        timer_interrupt_flag_clear(TIMER6, TIMER_INT_FLAG_UP);
        (void)pxMBPortCBTimerExpired();
    }
}
