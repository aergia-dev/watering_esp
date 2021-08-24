
#define ESP_INTR_FLAG_DEFAULT 0

 void initGPIO();
 void IRAM_ATTR set_gpio(int valve_idx, int val);
