
void init_timer_logic(void);
void timer_loop(void);
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
    int valve_activation);

void stop_watering();


void testing_open_all();
void testing_close_all();