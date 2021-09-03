#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/timer.h"
#include "gpio_cont.h"
#include "esp_log.h"
#include "global_setting.h"
#include "funcs.h"
#include "time.h"
#include <inttypes.h>

#define TAG "timer_logic.c"

static bool IRAM_ATTR timer_group_isr_callback(void *args);

// group0, timer0 => every minute timer for check day/night.
// group1, timer0 => valve active time timer, valve has limited working time. 
// group1, timer1 => watering, resting state timer
#define TIMER_DIVIDER         (16)  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds
#define WATERING_TIMER TIMER_GROUP_0
#define STATE_TIMER TIMER_GROUP_1

enum VALVE_STATE {
    CLOSE,
    OPEN,
};
typedef struct {
    int timer_group;
    int timer_idx;
    int alarm_interval;
    bool auto_reload;
} example_timer_info_t;

/**
 * @brief A sample structure to pass events from the timer ISR to task
 *
 */
typedef struct {
    example_timer_info_t info;
    uint64_t timer_counter_value;
} example_timer_event_t;

static xQueueHandle s_timer_queue;

//todo: merge with global state
enum CURRENT_STATE state = NONE;
int valve_idx = 0;


int _valve_cnt;
int _working_unit_time;
int _watering_time;
int _resting_time;
int _time_checking_time;
int _open_hour;
int _open_minute;
int _end_hour;
int _end_minute;
bool _valve_activation[100];


static int next_valve_idx(int valve_idx)
{
    int ret = valve_idx;
   for(int i = valve_idx; i < valve_idx + _valve_cnt +1; i++)
    {
        valve_idx++;
        valve_idx = valve_idx % _valve_cnt;

        if(_valve_activation[valve_idx] == 1)
        {
            ret = valve_idx;
            break;
        }
    }
    return ret;
}

static int start_valve_idx()
{
    int ret = 0;
    int idx = -1;
    ret = next_valve_idx(idx);
    return ret;
}

static void IRAM_ATTR set_valve(int idx, enum VALVE_STATE state)
{
    if(state == CLOSE)
    {
        ESP_LOGI(TAG, "valve #%d is closed", idx);
    }
    else
    {
        ESP_LOGI(TAG, "valve #%d is opened", idx);
    }
    set_gpio(valve_gpio[idx], state);
}

static void turn_off_all_valve()
{
    ESP_LOGI(TAG,  "close all value; %d",_valve_cnt);
    for(int i =0; i < _valve_cnt; i++)
    {
        set_valve(i, CLOSE);
    }
}

/**
 * @brief Initialize selected timer of timer group
 *
 * @param group Timer Group number, index from 0
 * @param timer timer ID, index from 0
 * @param auto_reload whether auto-reload on alarm event
 * @param timer_interval_sec interval of alarm
 */
static void start_timer(int group, int timer, bool auto_reload, int timer_interval_sec)
{
    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_PAUSE,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = auto_reload,
    }; // default clock source is APB
    timer_init(group, timer, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(group, timer, 0);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(group, timer, timer_interval_sec * TIMER_SCALE);
    timer_enable_intr(group, timer);
   
    example_timer_info_t *timer_info = calloc(1, sizeof(example_timer_info_t));
    timer_info->timer_group = group;
    timer_info->timer_idx = timer;
    timer_info->auto_reload = auto_reload;
    timer_info->alarm_interval = timer_interval_sec;
    timer_isr_callback_add(group, timer, timer_group_isr_callback, timer_info, 0);

    timer_start(group, timer);
}

void disable_timer()
{
    // timer_disable_intr(TIMER_GROUP_0, TIMER_0);
    timer_disable_intr(TIMER_GROUP_1, TIMER_0);
    timer_disable_intr(TIMER_GROUP_1, TIMER_1);
    
    // timer_pause(TIMER_GROUP_0, TIMER_0);
    timer_pause(TIMER_GROUP_1, TIMER_0);
    timer_pause(TIMER_GROUP_1, TIMER_1);
}

void stop_watering()
{
    disable_timer();
    turn_off_all_valve();
    state = STOP_WORKING;
    set_state(STOP_WORKING);
}

int test_s_h;
int test_s_m;
int test_e_h;
int test_e_m;


bool is_watering_time()
{
    bool ret = false;
    struct tm now = get_current_time();
    ESP_LOGI(TAG,  "                    current time: %d:%d ", now.tm_hour, now.tm_min);
    ESP_LOGI(TAG,  "watering time: %d:%d  ~ %d:%d", test_s_h, test_s_m, test_e_h, test_e_m);
    
    int current_min = now.tm_hour * 60 + now.tm_min;
    int start_min = _open_hour * 60 + _open_minute;
    int end_min = _end_hour * 60 + _end_minute;

    // int start_min = test_s_h * 60 + test_s_m;
    // int end_min = test_e_h * 60 + test_e_m;


    ESP_LOGI(TAG,  "start %d, current: %d, end: %d", start_min, current_min, end_min);

    //start time is bigger than end time
    //ex: star time: 20:00 ~ end time: 10:00
    if(start_min > end_min)
    {
        if(current_min > start_min)
        {
            ret = true;
        }
        else 
        {
            if(current_min < end_min)
            {
                ret = true;
            }
        }
    }
    else //ex: start time 10:00 ~ end time 20:00
    {
        if(start_min < current_min &&  current_min < end_min)
        {
            ret = true;
        }
        else
        {
            ret = false;
        }
       
    }
     ESP_LOGI(TAG,  "is open time?: %d", ret);

     return ret;
}

//watering/resting/not_availabe mode change preparation action.
enum CURRENT_STATE change_mode(enum CURRENT_STATE _state)
{
    enum CURRENT_STATE new_state = _state; 
    ESP_LOGI(TAG,  "change mode: %d", _state);

    if(is_watering_time())
    {
        //restore or start
        if(_state == NONE)
        {
            ESP_LOGI(TAG,  "## working time.");   
            new_state = WATERING;
            valve_idx = start_valve_idx();
            set_valve(valve_idx, OPEN);
            start_timer(TIMER_GROUP_1, TIMER_0, false, _watering_time);
            start_timer(TIMER_GROUP_1, TIMER_1, true, _working_unit_time);    
        }
    }
    else if(_state != NONE)
    {
        ESP_LOGI(TAG,  "## not working time.");   
        new_state = NONE;
        stop_watering();
    }

    set_state(new_state);
    return new_state;
}


void start_watering(
    int valve_cnt, 
    int working_unit_time, 
    int watering_time,
    int resting_time,
    int time_checking_time,
    int open_hour,
    int open_minute,
    int end_hour,
    int end_minute, 
    int valve_activation)
{
    ESP_LOGI(TAG,  "start_Watering - logic");
   
    //TODO: return err at first time. 
    if(state == WATERING || state == RESTING)
    {
        disable_timer();
        turn_off_all_valve();
    }  
    state = NONE;
    set_state(NONE);
 ESP_LOGI(TAG,  "2");
   
    //set file's global varis.
    _valve_cnt = valve_cnt;
    _working_unit_time = working_unit_time;
    _watering_time = watering_time * working_unit_time;
    _resting_time = resting_time * working_unit_time;
    _time_checking_time = time_checking_time * working_unit_time;
    _open_hour = open_hour;
    _open_minute = open_minute;
    _end_hour = end_hour;
    _end_minute = end_minute;

    for(int i = 0;i < valve_cnt; i++)
    {
        _valve_activation[i] = ((valve_activation >> i) & 1);
    }
ESP_LOGI(TAG,  "3");
  
    start_timer(TIMER_GROUP_0, TIMER_0, true, _time_checking_time);
 ESP_LOGI(TAG,  "4");
 
    state = change_mode(state);
    ESP_LOGI(TAG,  "5");
 
}


static bool IRAM_ATTR timer_group_isr_callback(void *args)
{
    BaseType_t high_task_awoken = pdFALSE;
    example_timer_info_t *info = (example_timer_info_t *) args;

    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(info->timer_group, info->timer_idx);

    /* Prepare basic event data that will be then sent back to task */
    example_timer_event_t evt = {
        .info.timer_group = info->timer_group,
        .info.timer_idx = info->timer_idx,
        .info.auto_reload = info->auto_reload,
        .info.alarm_interval = info->alarm_interval,
        .timer_counter_value = timer_counter_value
    };
  
      //TIMER_GROUP_1, TIMER_0 watering/resting state timer. 
        if(!evt.info.auto_reload && evt.info.timer_group == TIMER_GROUP_1 && evt.info.timer_idx == TIMER_0)
        {
             int t;
            if(state == WATERING)
            {
                 t = _resting_time;
               
            }
            else
            {
                t = _watering_time;
            }

            timer_counter_value += t * TIMER_SCALE;
            timer_group_set_alarm_value_in_isr(evt.info.timer_group, evt.info.timer_idx, timer_counter_value);
        }
   
    /* Now just send the event data back to the main program task */
    xQueueSendFromISR(s_timer_queue, &evt, &high_task_awoken);

    return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
}



void timer_loop(void)
{
    s_timer_queue = xQueueCreate(10, sizeof(example_timer_event_t));

    while (1) {

        example_timer_event_t evt;
        xQueueReceive(s_timer_queue, &evt, portMAX_DELAY);

        ESP_LOGI(TAG,  "group: %d, timer: %d, state: %d" , evt.info.timer_group,evt.info.timer_idx, state  );
          
        if(evt.info.timer_group == TIMER_GROUP_0 && evt.info.timer_idx == TIMER_0)
        {
            ESP_LOGI(TAG,  "state: %d", state);
            
            state = change_mode(state);

            if(state == NONE)
                continue;
        }

        if(evt.info.timer_group == TIMER_GROUP_1 && evt.info.timer_idx == TIMER_1)
        {
            if(state == WATERING)
            {
                set_valve(valve_idx, CLOSE);
                valve_idx = next_valve_idx(valve_idx);
                set_valve(valve_idx, OPEN);
            }
        }

        if(evt.info.timer_group == TIMER_GROUP_1 && evt.info.timer_idx == TIMER_0)
        { 
            switch(state)
            {
                case NONE:
                break;

                case WATERING:
                {
                    ESP_LOGI(TAG,  "RESTING");
                    state = RESTING;
                    set_state(state);
                    turn_off_all_valve();
                    timer_pause(TIMER_GROUP_1, TIMER_1);
                break;
                }
                case RESTING:
                    ESP_LOGI(TAG,  "WATERING");
                    state = WATERING;
                    set_state(state);
                    valve_idx = start_valve_idx();
                    set_valve(valve_idx, OPEN);
                    timer_start(TIMER_GROUP_1, TIMER_1);
                break;
                case STOP_WORKING:
                break;
                case NOT_AVAILABLE:
                break;
            }
        }
    }
}


void testing_open_all()
{
     //defence.. return err at first time. 
    stop_watering();

/*  int valve_cnt, 
    int working_unit_time, 
    int watering_time,
    int resting_time,
    int time_checking_time,
    int open_hour,
    int open_minute,
    int end_hour,
    int end_minute, 
    int valve_activation
    */
    start_watering(4, 5, 1*4, 1, 30, 1, 1, 24, 59, 0xFF);
}
void testing_close_all()
{
       //defence.. return err at first time. 
    stop_watering();

    ESP_LOGI(TAG,  "close all - %d", gvalve_cnt); 
  
    turn_off_all_valve();
}
