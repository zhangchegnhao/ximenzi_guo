/************************************************************
 * 版权：2025CIMC Copyright。
 * 文件：timer.c
 * 作者: Jialei Zhao
 * 平台: 2025CIMC IHD-V04
 * 版本: Jialei Zhao     2026/1/4     V0.01    original
************************************************************/


/************************* 头文件 *************************/

#include "timer.h"

/************************* 宏定义 *************************/


/************************ 变量定义 ************************/

/************************ 函数定义 ************************/
/************************************************************
 * Function :       my_timer_init
 * Comment  :       初始化基本定时器 TIM6
 * Parameter:       null
 * Return   :       null
 * Author   :       Jialei Zhao
 * Date     :       2026-01-04 V0.1 original
************************************************************/
void my_timer_init(uint32_t period)
{
	nvic_irq_enable(TIMER6_IRQn , 4 , 0);

	rcu_timer_clock_prescaler_config(RCU_TIMER_PSC_MUL4);

	timer_parameter_struct timer_initpara;

	rcu_periph_clock_enable(RCU_TIMER6);

	timer_deinit(TIMER6);

	timer_initpara.prescaler = 240 - 1;			//分频240
	timer_initpara.alignedmode = TIMER_COUNTER_EDGE;	//边沿对齐模式
	timer_initpara.counterdirection = TIMER_COUNTER_UP;	//向上计数模式
	timer_initpara.period = period - 1;			//自动重装载值35
	timer_initpara.clockdivision = TIMER_CKDIV_DIV1;	//时钟分频1
	timer_initpara.repetitioncounter = 0;		//重复计数器0
	timer_init(TIMER6 , &timer_initpara);			//初始化定时器6


	// timer_enable(TIMER6);

}

/****************************End*****************************/
