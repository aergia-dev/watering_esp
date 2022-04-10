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
bool valve_cur_state[10] = {false, };
int timer_cnt = 0;
const int state_change_timer_term = 1; //1minute
bool is_working_time = false;
int current_working_valve = -1;


static int current_valve()
{
    int ret = -1;
    for(int i = 0; i < gvalve_cnt; i++)
    {
        if(valve_cur_state[i] == true)
        {
            ret = i;
        }
    }
    
    if(ret == -1)
        ret = 99;
        
    return ret;
}

void update_notification(enum CURRENT_STATE _state, int _timer_cnt, bool _is_in_time, int _cur_valve)
{
    int set_time;

    if(_state == NONE)
    {
        set_time = 0;
    }
    else
    {
        if(_state == WATERING)
            set_time = gwatering_time;
        else
            set_time = gresting_time;
    }
    ESP_LOGI(TAG, "set time:%d, _timer_cnt: %d ", set_time, _timer_cnt);
    gnotification[0] = _state;
    gnotification[1] = _timer_cnt & 0xff;
    gnotification[2] = (_timer_cnt >> 8) & 0xff; 
    gnotification[3] = set_time & 0xff;
    gnotification[4] = (set_time >> 8) & 0xff;
    gnotification[5] = _is_in_time;
    gnotification[6] = _cur_valve;
        
}

static int next_valve_idx(int valve_idx)
{
    int ret = valve_idx;
   for(int i = valve_idx; i < valve_idx + gvalve_cnt +1; i++)
    {
        valve_idx++;
        valve_idx = valve_idx % gvalve_cnt;

        if(gvalve_activation[valve_idx] == 1)
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

    valve_cur_state[idx] = state;
}

static void turn_off_all_valve()
{
    ESP_LOGI(TAG,  "close all value; %d", gvalve_cnt);
    for(int i =0; i < gvalve_cnt; i++)
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
    timer_set_alarm_value(group, timer, timer_interval_sec * TIMER_SCALE * 60);
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
    // set_state(STOP_WORKING);
}

bool is_watering_time()
{
    bool ret = false;
    struct tm now = get_current_time();
    // ESP_LOGI(TAG,  "watering time: %d:%d  ~ %d:%d", test_s_h, test_s_m, test_e_h, test_e_m);
    
    int current_min = now.tm_hour * 60 + now.tm_min;
    int start_min = gopen_hour * 60 + gopen_minute;
    int end_min = gend_hour * 60 + gend_minute;

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
     ESP_LOGI(TAG,  "TIME: %d:%d, is in range?: %d ", now.tm_hour, now.tm_min, ret);

     return ret;
}


//watering/resting/not_availabe mode change preparation action.
enum CURRENT_STATE change_mode(enum CURRENT_STATE _state, bool _is_in_time)
{
    enum CURRENT_STATE new_state = _state; 
    ESP_LOGI(TAG,  "change mode: %d", _state);
    // is_working_time = is_watering_time();
    if(_is_in_time)
    {
        //restore
        if(_state == OUT_OF_TIME)
        {
            ESP_LOGI(TAG,  "## working time.");   
            new_state = WATERING;
            valve_idx = start_valve_idx();
            set_valve(valve_idx, OPEN);
            start_timer(TIMER_GROUP_1, TIMER_0, true, state_change_timer_term);
            start_timer(TIMER_GROUP_1, TIMER_1, true, gworking_unit_time);    
        }
    }
    else if(_state != NONE)
    {
        ESP_LOGI(TAG,  "## not working time.");   
        new_state = OUT_OF_TIME;
        stop_watering();
    }

    // set_state(new_state);
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
    state = WATERING;
    start_timer(TIMER_GROUP_1, TIMER_0, true, state_change_timer_term);
    start_timer(TIMER_GROUP_1, TIMER_1, true, gworking_unit_time);    

    bool is_in_time = is_watering_time();
    valve_idx = start_valve_idx();
    if(is_in_time)
    {
        set_valve(valve_idx, OPEN);
       
    }
    update_notification(state, 0, is_in_time, valve_idx);
}

void init_timer_logic()
{
    start_timer(TIMER_GROUP_0, TIMER_0, true, gtime_checking_time);
    state = NONE;
    bool is_in_time = is_watering_time();
    update_notification(state, 0, is_in_time, 99);
}

static bool IRAM_ATTR timer_group_isr_callback(void *args)
{
    BaseType_t high_task_awoken = pdFALSE;
    example_timer_info_t *info = (example_timer_info_t *) args;

    uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(info->timer_group, info->timer_idx);

    // /* Prepare basic event data that will be then sent back to task */
    example_timer_event_t evt = {
        .info.timer_group = info->timer_group,
        .info.timer_idx = info->timer_idx,
        .info.auto_reload = info->auto_reload,
        .info.alarm_interval = info->alarm_interval,
        .timer_counter_value = timer_counter_value
    };
   
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
       // ESP_LOGI(TAG,  "group: %d, timer: %d, state: %d, counter value: %lld" , evt.info.timer_group, evt.info.timer_idx, state, timer_counter_value);
        
        ESP_LOGI(TAG,  "group: %d, timer: %d, state: %d, valve: %d" , evt.info.timer_group, evt.info.timer_idx, state ,current_valve());
        
        bool is_in_time = is_watering_time();
        // update_notification(state); 
        //group0, timer0: checking "working time range"
        if(evt.info.timer_group == TIMER_GROUP_0 && evt.info.timer_idx == TIMER_0)
        {
            // ESP_LOGI(TAG,  "state: %d", state);
            state = change_mode(state, is_in_time);
            // if(state == NONE)
            //     continue;
        }

        //group1, timer1: valve's default working time
        if(evt.info.timer_group == TIMER_GROUP_1 && evt.info.timer_idx == TIMER_1)
        {
            if(state == WATERING)
            {
                set_valve(valve_idx, CLOSE);
                valve_idx = next_valve_idx(valve_idx);
                set_valve(valve_idx, OPEN);
            }
        }

        //group1, timer0: change watering/resting
        if(evt.info.timer_group == TIMER_GROUP_1 && evt.info.timer_idx == TIMER_0)
        { 
            timer_cnt++;
             ESP_LOGI(TAG,  "current timer cnt: %d, watering: %d, resting: %d", timer_cnt, gwatering_time, gresting_time);
             
            switch(state)
            {
                case NONE:
                break;

                case WATERING:
                {
                    if(timer_cnt >= gwatering_time)
                    {
                        ESP_LOGI(TAG,  "RESTING");
                        state = RESTING;
                        set_state(state);
                        turn_off_all_valve();
                        timer_cnt = 0;
                    }
                break;
                }
                case RESTING:
                {
                    if(timer_cnt >= gresting_time)
                    {
                        ESP_LOGI(TAG,  "WATERING");
                        state = WATERING;
                        set_state(state);
                        valve_idx = start_valve_idx();
                        set_valve(valve_idx, OPEN);
                        timer_cnt = 0;
                    }
                break;
                case OUT_OF_TIME:
                break;
                }

                case STOP_WORKING:
                break;
                case NOT_AVAILABLE:
                break;
            }
        }

        update_notification(state, timer_cnt, is_in_time, current_valve()); 
       
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
