#include "protocol.h"
#include "sleep_light.h"
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "ble_cmd_handler.h"
#include <string.h>
#include "gpio_cont.h"
#include "nvs_storage.h"
#include "timer_logic.h"
#include "funcs.h"
#include "global_setting.h"

#define TAG "CMD_HANDLER"
#define rsp_buffer_sz 10
uint8_t rsp_buffer[rsp_buffer_sz];
uint8_t rsp_sz = 0;


void cmd_handler(uint16_t main, uint8_t len, uint8_t * vals)
{
   ESP_LOGI(TAG, "main: %d, len: %d,", main, len);
    ESP_LOGI(TAG, "vals");
  
   for(int i =0; i< len; i++)
    ESP_LOGI(TAG, "%d", vals[i]);

    switch(main)
    {

        case WATERING_TIME:
        {
            int val = (vals[1] << 8) | vals[0];
            ESP_LOGI(TAG, "set watering time: %d - %d: %d ", vals[0], vals[1], val);
            nvs_write_uint32("WATERING_TIME", val);
            break;
        }
        case RESTING_TIME:
        {
            int val = (vals[1] << 8) | vals[0];
            nvs_write_uint32("RESTING_TIME", val);
            break;
        }
        case OPEN_TIME:
        {

            extern int test_s_h;
            extern int test_s_m;
            extern int test_e_h;
            extern int test_e_m;

            test_s_h = vals[0];
            test_s_m = vals[1];
            test_e_h = vals[2];
            test_e_m = vals[3];
            
            ESP_LOGI(TAG, "set open time: %d;%d - %d: %d ", vals[0], vals[1], vals[2], vals[3]);
            nvs_write_uint32("OPEN_HOUR", vals[0]);
            nvs_write_uint32("OPEN_MINUTE", vals[1]);
            nvs_write_uint32("END_HOUR", vals[2]);
            nvs_write_uint32("END_MINUTE", vals[3]);
        break;
        }
        case START:
        {
            ESP_LOGI(TAG, "start watering ");
            start_watering(
                nvs_read_uint32("VALVE_CNT", gvalve_cnt),
                nvs_read_uint32("WOKRING_UNIT", gworking_unit_time),
                nvs_read_uint32("WATERING_TIME", gwatering_time), 
                nvs_read_uint32("RESTING_TIME", gresting_time),
                nvs_read_uint32("TIME_CHECKING", gtime_checking_time),
                nvs_read_uint32("OPEN_HOUR", gopen_hour),
                nvs_read_uint32("OPEN_MINUTE", gopen_minute),
                nvs_read_uint32("END_HOUR", gend_hour),
                nvs_read_uint32("END_MINUTE", gend_minute),
                nvs_read_uint32("ACTIVATION", gvalue_activation));
            break;
        }
        case STOP:
            stop_watering();
        break;
        case SYNC_DATA:
        {
            ESP_LOGI(TAG, "sync data ");
            memset(rsp_buffer, 0, sizeof(uint8_t) * rsp_buffer_sz);
            rsp_buffer[0] = (main >> 8) & 0xFF;
            rsp_buffer[1] = main & 0xFF ;
            rsp_buffer[2] = 14;
            rsp_buffer[3] = nvs_read_uint32("VALVE_CNT", gvalve_cnt);
            rsp_buffer[4] = nvs_read_uint32("USING_VALVE_CNT", gworking_unit_time);
            rsp_buffer[5] = nvs_read_uint32("WOKRING_UNIT", gwatering_time);
            rsp_buffer[6] = nvs_read_uint32("WATERING_TIME", gresting_time);
            rsp_buffer[7] = nvs_read_uint32("RESTING_TIME", gresting_time);
            rsp_buffer[8] = nvs_read_uint32("TIME_CHECKING", gtime_checking_time);
            rsp_buffer[9] = nvs_read_uint32("OPEN_HOUR", gopen_minute);
            rsp_buffer[10] = nvs_read_uint32("OPEN_MINUTE", gopen_minute);
            rsp_buffer[11] = nvs_read_uint32("END_HOUR", gend_hour);
            rsp_buffer[12] = nvs_read_uint32("OPEN_MINUTE", gend_minute);
            rsp_buffer[13] = nvs_read_uint32("ACTIVATION", gvalue_activation);
            break;
        }
        case TIMESTAMP:
            ESP_LOGI(TAG, "sync time");
            if(len == 4)
            {
                int v = ((vals[0] << 0)| (vals[1] << 8) | (vals[2] << 16) | (vals[3] << 24));
                ESP_LOGI(TAG, "sync time: %d", v);
                set_current_time(v);
            }
           
        break;

        case VALVE_ACTIVATION:
        {
            if(len == 2)
            {
                char key[] = "ACTIVATION";
                int idx = vals[0];
                int state = vals[1];
                uint32_t saved_state = nvs_read_uint32(key, 0xFFFF);
            
                if(state)
                    nvs_write_uint32(key, saved_state | (1 << idx));
                else 
                    nvs_write_uint32(key, saved_state & ~(1 << idx));
            
                int a = ((state << idx) & 0xFFFF);
                saved_state = nvs_read_uint32(key, 0xFFFF);
                ESP_LOGI(TAG, "idx: %d, input: %d, result: %d", idx, a, saved_state);
                // ESP_LOGI(TAG, "valve: %d, ", saved_state);
            }
            break;
        }
        case STATE:
        {
            memset(rsp_buffer, 0, sizeof(uint8_t) * rsp_buffer_sz);
            rsp_buffer[0] = (main >> 8) & 0xFF;
            rsp_buffer[1] = main & 0xFF ;
            rsp_buffer[2] = 4;
            rsp_buffer[3] = get_state();
            break;
        }
        case RESET:
            reset_setting();
            break;
            
        case TEST1:
        {
            testing_open_all();
            break;
        }
        case TEST2:
        {
            testing_close_all();
            break;
        }
    }

}

uint8_t* get_rsp_buffer()
{
    return rsp_buffer;
}

uint8_t get_rsp_sz()
{
    return rsp_sz;
}