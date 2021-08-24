
#include "spi_ws2812.h"
#include "sleep_light.h"
#include "esp_log.h"
#include "nvs_storage.h"
#include "nvs_flash.h"
#include "nvs.h"

#define LIGHT_TAG "Light Log"
#define LED_CNT 15
int led_using_cnt = LED_CNT;
#define DEFAULT_COLOR PaleGreen
ARGB current_color;
bool light_on_off = false;

void light_on()
{
    light_chage_color(current_color);
    light_on_off = true;
}

void light_off()
{
    ARGB color;
    color.code = Black;

    light_chage_color(color);
    light_on_off = false;
}

void light_chage_color(ARGB color)
{

    ESP_LOGI(LIGHT_TAG, "change light color %d, %d, %d, %d", color.code, color.argb.red, color.argb.green, color.argb.blue);
    //not machted change. manually match it.. fix it.
    ARGB c = {.argb.red=color.argb.blue, .argb.green = color.argb.green, .argb.blue = color.argb.red};   
    change_color(c.code, led_using_cnt);
    current_color = color;
    led_strip_update();
}

void light_change_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    light_chage_color(fromRGB(r,g,b));
}

void control_brightness(int level)
{
    // level = 50 : do nothing
    // level = 100 : maximum bright
    // leve = 0 : minimum brightness

    //if(direction == -1)
    //    level = level * -1;
    printf("level %d\n", level);
    level = level - 50;
    printf("level %d\n", level);
 
 printf("%d, %d, %d\n", current_color.argb.red, current_color.argb.green, current_color.argb.blue);
 
    int r = current_color.argb.red * level / 100;
    int g = current_color.argb.green * level / 100;
    int b = current_color.argb.blue * level / 100;
    
    printf("%d, %d, %d\n", r, g, b);
 
    if((r >= 0 && g >= 0 && b >= 0) || (r <= 255 && g <= 255 && b <= 255))
    {
        current_color = fromRGB(r, g, b);
        light_chage_color(current_color);
    }
}

ARGB read_color_nvs()
{
    ARGB saved_color = {.code =  nvs_read_uint32("saved_color", DEFAULT_COLOR)};
    ESP_LOGI(LIGHT_TAG, "color from nvs %d", saved_color.code);   
    return saved_color;
}

void write_color_nvs(uint32_t color)
{
    nvs_write_uint32("saved_color", color);
}

void save_color_nvs(uint8_t r, uint8_t g, uint8_t b)
{
    write_color_nvs((r<<16) | (g<<8) | b);
}

void light_init()
{
    initSPIws2812();
    current_color = read_color_nvs();
    //light_chage_color(current_color);
    light_on();
    ESP_LOGI(LIGHT_TAG, "change light color %d", current_color.code);
}

void get_current_color(uint8_t* color)
{
    color[0] = current_color.argb.alpha;
    color[1] = current_color.argb.red;
    color[2] = current_color.argb.green;
    color[3] = current_color.argb.blue;

    printf("%d, %d, %d, %d, %d",current_color.code, color[0], color[1], color[2], color[3]);
}

bool get_light_on_off()
{
    return light_on_off;
}

ARGB fromRGB(uint8_t r, uint8_t g, uint8_t b)
{
    ARGB color = {.argb.alpha=0, .argb.red=r, .argb.blue=b, .argb.green=g, };

    return color;
}