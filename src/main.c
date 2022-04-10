#include "nvs_storage.h"
#include "ble_server.h"
#include "sleep_light.h"

#include "esp_log.h"
#include "esp_system.h"
#include "gpio_cont.h"
#include "timer_logic.h"
#include "global_setting.h"

#define CMD_HANDLER_TAG "MAIN_HANDLER"


//example use
void app_main()
{	
	nvs_init();
	
	global_setting_init();
	
	ble_start();
	light_init();
    initGPIO();
	init_timer_logic();
	timer_loop();

}

