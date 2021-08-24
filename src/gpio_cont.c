#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "gpio_cont.h"
#include "global_setting.h"

static xQueueHandle gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

static uint64_t set_gpio_pin_sel()
{
    uint64_t pin_sel = 0;
    for(int i =0; i< VALVE_CNT; i++)
    {
        pin_sel |= (1ULL << valve_gpio[i]);
    }

    return pin_sel;
}

void initGPIO()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = set_gpio_pin_sel();
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

	 //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);


    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
  
    for(int i =0; i < VALVE_CNT; i++)
    {
        gpio_isr_handler_add(valve_gpio[i], gpio_isr_handler, (void*) valve_gpio[i]);
    }

    // gpio_isr_handler_add(VALVE_0, gpio_isr_handler, (void*) VALVE_0);
    //hook isr handler for specific gpio pin
    // gpio_isr_handler_add(VALVE_1, gpio_isr_handler, (void*) VALVE_1);

    printf("Minimum free heap size: %d bytes\n", esp_get_minimum_free_heap_size());
}

 void IRAM_ATTR set_gpio(int valve_idx, int val)
{
    gpio_set_level(valve_idx, val);
}
