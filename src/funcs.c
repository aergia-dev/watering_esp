
#include <time.h>
#include <sys/time.h>
#include "esp_log.h"

#define TAG "FUNCS"

void set_current_time(int timestamp)
{
    ESP_LOGI(TAG, "%d", timestamp);
    
    struct timeval tv;
    tv.tv_sec = timestamp;
    tv.tv_usec = 0;

    settimeofday(&tv, 0);

}

struct tm get_current_time()
{
    time_t now;
    // char strftime_buf[64];
    struct tm timeinfo;

    time(&now);

    setenv("TZ", "UTC-9", 1);
    tzset();

    localtime_r(&now, &timeinfo);
    // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);

    return timeinfo;
}