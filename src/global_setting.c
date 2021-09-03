#include "global_setting.h"
#include <stdbool.h>
#include "nvs_storage.h"

const enum VALVE_GPIO valve_gpio[] = {VALVE_0, VALVE_1, VALVE_2, VALVE_3};
bool valve_activation[] = {true, true, true, true};

int gworking_unit_time;
int gvalve_cnt;
int gwatering_time;
int gresting_time;
int gtime_checking_time;
int gopen_hour;
int gopen_minute;
int gend_hour;
int gend_minute;
int gvalue_activation;
enum CURRENT_STATE gstate = NONE;

void global_setting_init()
{
    //erase_all();

    gvalve_cnt = nvs_read_uint32("VALVE_CNT", 4);
    gworking_unit_time = nvs_read_uint32("WOKRING_UNIT", 60);
    gwatering_time = nvs_read_uint32("WATERING_TIME", 1);
    gresting_time = nvs_read_uint32("RESTING_TIME", 1);
    gtime_checking_time = nvs_read_uint32("TIME_CHECKING", 1);

    gopen_hour = nvs_read_uint32("OPEN_HOUR", 1);
    gopen_minute = nvs_read_uint32("OPEN_MINUTE", 0);
    gend_hour = nvs_read_uint32("END_HOUR", 12);
    gend_minute = nvs_read_uint32("END_MINUTE", 59);
    gvalue_activation = nvs_read_uint32("ACTIVATION", 0xFFFF);

    //valve_cnt's max is 4.
    for(int i = 0;i < gvalve_cnt; i++)
    {
        
        valve_activation[i] = ((gvalue_activation >> i) & 1);
    }
}

void reset_setting()
{
    erase_all();
    global_setting_init();

}

void set_state(enum CURRENT_STATE s)
{
    gstate = s;
}

enum CURRENT_STATE get_state()
{
    return gstate;
}
