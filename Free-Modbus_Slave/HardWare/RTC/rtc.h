#ifndef __RTC_H__
#define __RTC_H__

#include "HeaderFiles.h"

void my_rtc_init(void);

void rtc_show_time(uint8_t* timeInfor , uint8_t len);

void rtc_get_time(uint16_t* tm);

void rtc_setup(uint8_t* time_config);

void rtc_set_time(uint32_t unix_time_stamp);

void my_rtc_enable_alarm(void);

void my_rtc_disable_alarm(void); 


#endif // !__RTC_H__
