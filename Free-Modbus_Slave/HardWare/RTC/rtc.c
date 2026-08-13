#include "rtc.h"
#include "time.h"

#define BKP_VALUE 0x32F1
#define Alarm_Time 0x10


//!========================================================
// 如下是RTC外部时钟32.768kHZ的分频设置

// 闹钟预分频器
uint8_t prescaler_a;
// 秒预分频器
uint8_t prescaler_s;

rtc_alarm_struct  rtc_alarm;
// RTC参数结构体 , 后续获取当前时间有需要
rtc_parameter_struct rtc_initpara;

//!========================================================

// 配置RTC前的准备
void rtc_pre_config(void);

void my_rtc_init(void)
{
	//! 启用备份域中RTC寄存器的访问
	// PMU --> 是电源管理单元，负责芯片电源的相关功能（包含着RTC）
	rcu_periph_clock_enable(RCU_PMU);
	pmu_backup_write_enable();

	//! 配置RTC前的准备
	rtc_pre_config();

	if ((uint32_t)(RTC_BKP0) != BKP_VALUE)
	{
		// 直接重新进行RTC配置
		rtc_setup(NULL);
	}
	//! 下面是中断的配置
	rcu_all_reset_flag_clear(); // 清除所有复位标志位

}


void rtc_pre_config(void)
{

	//!外部低速晶振32.768kHz
	rcu_osci_on(RCU_LXTAL);
	rcu_osci_stab_wait(RCU_LXTAL);
	rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);

	//! LXTAL / (0xFF+1) /(0x7F+1) = 1Hz    也就是1S RTC走一次(1S多走一次)
	prescaler_s = 0xFF;
	prescaler_a = 0x7F;

	//! 此时才算真正的开启RTC时钟
	rcu_periph_clock_enable(RCU_RTC);
	//! 等待RTC_TIME和RTC_DATE寄存器与APB时钟同步，并且更新阴影寄存器
	if (rtc_register_sync_wait() != SUCCESS)
	{
		printf("RTC register sync wait false\r\n");
		return;
	}
	else
	{
		// printf("RTC register sync wait success\r\n");
	}
}

const uint8_t time_buf_cur[9] = "00:00:00";
void rtc_setup(uint8_t* time_config)
{

	//! 设置RTC参数

	rtc_initpara.factor_asyn = prescaler_a;
	rtc_initpara.factor_syn = prescaler_s;

	rtc_initpara.year = 0x00;
	rtc_initpara.day_of_week = RTC_SATURDAY;
	rtc_initpara.month = RTC_APR;
	rtc_initpara.date = 0x30;
	rtc_initpara.display_format = RTC_24HOUR;
	rtc_initpara.am_pm = RTC_AM;

	unsigned int tmp_year = 0x00, tmp_month = 0x00, tmp_date = 0x00, tmp_hh = 0x00, tmp_mm = 0x00, tmp_ss = 0x00;
	sscanf((char*)time_buf_cur, "%2x:%2x:%2x", &tmp_hh, &tmp_mm, &tmp_ss);
	// printf("tmp_hh = %0.2x, tmp_mm = %0.2x, tmp_ss = %0.2x\r\n" , tmp_hh , tmp_mm , tmp_ss);
	rtc_initpara.hour = tmp_hh;
	rtc_initpara.minute = tmp_mm;
	rtc_initpara.second = tmp_ss;

	if (time_config)
	{
		if (strlen((char*)time_config) <= 14)
		{
			printf("RTC Config error\n\r");
			return;
		}
		else
		{
			sscanf((char*)time_config, "%*2x%2x%*1c%2x%*1c%2x%*1c%2x%*1c%2x%*1c%2x", \
				& tmp_year, &tmp_month, &tmp_date, &tmp_hh, &tmp_mm, &tmp_ss);
			// printf("RTC Config: %02x-%02x-%02x %0.2x:%0.2x:%0.2x \n\r" , \
			// 	tmp_year , tmp_month , tmp_date , \
			// 	tmp_hh , tmp_mm , tmp_ss);
			rtc_initpara.year = tmp_year;
			rtc_initpara.month = tmp_month;
			rtc_initpara.date = tmp_date;
			rtc_initpara.hour = tmp_hh;
			rtc_initpara.minute = tmp_mm;
			rtc_initpara.second = tmp_ss;
		}

		if (rtc_initpara.year > 0x99)
		{
			printf("RTC Config error: year > 0x99\r\n");
			printf("RTC Config error: year\r\n");
			return;
		}
		if (rtc_initpara.month > 0x12)
		{
			printf("RTC Config error: month > 12\r\n");
			printf("RTC Config error: month\r\n");
			return;
		}
		if (rtc_initpara.date > 0x31)
		{
			printf("RTC Config error: date > 31\r\n");
			printf("RTC Config error: date\r\n");
			return;
		}
		if (rtc_initpara.hour > 0x23)
		{
			printf("RTC Config error: hour > 23\r\n");
			printf("RTC Config error: hour\r\n");
			return;
		}
		if (rtc_initpara.minute > 0x59)
		{
			printf("RTC Config error: minute > 59\r\n");
			printf("RTC Config error: minute\r\n");
			return;
		}
		if (rtc_initpara.second > 0x59)
		{
			printf("RTC Config error: second > 59\r\n");
			printf("RTC Config error: second\r\n");
			return;
		}
	}

	//! 初始化RTC
	if (rtc_init(&rtc_initpara) == ERROR)
	{
		printf("RTC init false\r\n");
		return;
	}
	// printf("RTC init success\r\n");

	//! 备份校验值
	RTC_BKP0 = BKP_VALUE;
}

void rtc_show_time(uint8_t* timeInfor, uint8_t len)
{
	uint32_t time_subsecond = 0;
	uint8_t subsecond_ss = 0, subsecond_ts = 0, subsecond_hs = 0;
	time_subsecond = rtc_subsecond_get();
	subsecond_ss = (1000 - (time_subsecond * 1000 + 1000) / 400) / 100;
	subsecond_ts = (1000 - (time_subsecond * 1000 + 1000) / 400) % 100 / 10;
	subsecond_hs = (1000 - (time_subsecond * 1000 + 1000) / 400) % 10;
	rtc_current_time_get(&rtc_initpara);
	if (!timeInfor)
	{

		printf("Current time: %0.2x:%0.2x:%0.2x \n\r",
			rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second);
		return;
	}

	if (len < 24)
	{
		printf("timeInfor len error\r\n");
		return;
	}

	sprintf((char*)timeInfor, "%04x-%02x-%02x %0.2x:%0.2x:%0.2x:%0.1x%0.1x%0.1x",
		rtc_initpara.year | 0x2000, rtc_initpara.month, rtc_initpara.date,
		rtc_initpara.hour, rtc_initpara.minute, rtc_initpara.second, subsecond_ss, subsecond_ts, subsecond_hs);

}

void rtc_set_time(uint32_t unix_time_stamp)
{
	struct tm time_tmp = { 0 };
	time_tmp = *localtime(&unix_time_stamp);

	uint8_t time_buf[32] = { 0 };
	sprintf((char*)time_buf, "%04d-%02d-%02d %02d:%02d:%02d",
		time_tmp.tm_year + 1900, time_tmp.tm_mon + 1, time_tmp.tm_mday,
		time_tmp.tm_hour + 8, time_tmp.tm_min, time_tmp.tm_sec);
	// printf("\r\ntime_buf = %s\r\n" , time_buf);
	rtc_setup(time_buf);

}


void rtc_get_time(uint16_t* tm)
{
	rtc_current_time_get(&rtc_initpara);
	//!获取年月日时分秒
	tm[0] = (rtc_initpara.year % 16 + rtc_initpara.year / 16 * 10 + 2000);
	tm[1] = rtc_initpara.month % 16 + rtc_initpara.month / 16 * 10;
	tm[2] = rtc_initpara.date % 16 + rtc_initpara.date / 16 * 10;
	tm[3] = rtc_initpara.hour % 16 + rtc_initpara.hour / 16 * 10;
	tm[4] = rtc_initpara.minute % 16 + rtc_initpara.minute / 16 * 10;
	tm[5] = rtc_initpara.second % 16 + rtc_initpara.second / 16 * 10;
}

void my_rtc_enable_alarm(void)
{

	rtc_current_time_get(&rtc_initpara);

	rtc_alarm_disable(RTC_ALARM0);
	rtc_alarm.alarm_mask = RTC_ALARM_DATE_MASK | RTC_ALARM_HOUR_MASK | RTC_ALARM_MINUTE_MASK;
	rtc_alarm.weekday_or_date = RTC_ALARM_DATE_SELECTED;
	rtc_alarm.alarm_day = 0x31;
	rtc_alarm.am_pm = RTC_AM;

	/* RTC alarm value */
	rtc_alarm.alarm_hour = 0x00;
	rtc_alarm.alarm_minute = 0x00;
	// rtc_alarm.alarm_second = rtc_initpara.second + Alarm_Time;	//!Alarm_Time 秒后 触发闹钟
	// rtc_alarm.alarm_second = 0x03;	//!Alarm_Time 秒后 触发闹钟
	uint8_t cur_sec = (rtc_initpara.second >> 4) * 10 + (rtc_initpara.second & 0x0F);
	uint8_t alarm_sec = (cur_sec + 10) % 60;  // 超60自动回绕
	rtc_alarm.alarm_second = ((alarm_sec / 10) << 4) | (alarm_sec % 10);

	/* configure RTC alarm */
	rtc_alarm_config(RTC_ALARM0, &rtc_alarm);

	rtc_interrupt_enable(RTC_INT_ALARM0);
	rtc_alarm_enable(RTC_ALARM0);

	rtc_flag_clear(RTC_FLAG_ALRM0);

	// ? 第一步：配置 EXTI Line17（RTC Alarm专用线），上升沿触发
	exti_init(EXTI_17, EXTI_INTERRUPT, EXTI_TRIG_RISING);
	exti_flag_clear(EXTI_17);

	// ? 第二步：使能 NVIC，否则 WFI 无法响应
	nvic_irq_enable(RTC_Alarm_IRQn, 4, 0);

}


void my_rtc_disable_alarm(void)
{
	rtc_interrupt_disable(RTC_INT_ALARM0);
	rtc_alarm_disable(RTC_ALARM0);
	nvic_irq_disable(RTC_Alarm_IRQn);
	rtc_flag_clear(RTC_FLAG_ALRM0);
}


void RTC_Alarm_IRQHandler(void)
{
	if (rtc_flag_get(RTC_FLAG_ALRM0) != RESET)
	{
		rtc_flag_clear(RTC_FLAG_ALRM0);
		exti_flag_clear(EXTI_17);          // ? 必须清除 EXTI flag
	}
}

