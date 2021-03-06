
#ifndef __GLOBAL_SETTING
#define __GLOBAL_SETTING

#include <stdbool.h>

#define VALVE_CNT 4
enum VALVE_GPIO {
    VALVE_0 = 18,
    VALVE_1 = 19,
    VALVE_2 = 26,
    VALVE_3 = 27,
};

extern const enum VALVE_GPIO valve_gpio[];
extern bool valve_activation[];

extern int gusing_valve_cnt;
extern int gworking_unit_time;
extern int gvalve_cnt;
extern int gvalve_idx ;
extern int gwatering_time;
extern int gresting_time;
extern int gtime_checking_time;
extern int gopen_hour;
extern int gopen_minute;
extern int gend_hour;
extern int gend_minute;
extern bool hvalve_activation[];
void global_setting_init();

#endif
