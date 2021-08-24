
#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#define NVS_TAG "NVS"


void nvs_init()
{
    esp_err_t ret;

     // Initialize NVS.
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
  
}

void open_nvs_handle(nvs_handle_t* handle)
{
    esp_err_t err;
    err = nvs_open("storage", NVS_READWRITE, handle);
    if (err != ESP_OK) {
        ESP_LOGE(NVS_TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
}


bool nvs_write_uint32(char *key, uint32_t val)
{
    esp_err_t err;
    bool ret = true;
    nvs_handle_t handle;
    open_nvs_handle(&handle);

    err = nvs_set_u32(handle, key, val);
    
    if(err != ESP_OK)
       ret = false;
        
    printf("Committing updates in NVS ... ");
    err = nvs_commit(handle);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
    nvs_close(handle);

    return ret;
}

uint32_t nvs_read_uint32(char* key, uint32_t default_val)
{
    esp_err_t err;
    nvs_handle_t handle;
    open_nvs_handle(&handle);
    uint32_t saved_val = 0;
    err = nvs_get_u32(handle, key, &saved_val);

    switch (err) {
        case ESP_OK:
            printf("Done\n");
            printf("saved val = %d\n", saved_val);
            break;
        default:
            saved_val = default_val;
            ESP_LOGI(NVS_TAG, "%d, %s not init or can't read. set as default %d",  err,key, saved_val);
            nvs_write_uint32(key, default_val);
        }

    nvs_close(handle);

    return saved_val;
}

bool erase_all()
{
    esp_err_t err;
    bool ret = true;
    nvs_handle_t handle;
    open_nvs_handle(&handle);

    err = nvs_erase_all(handle);
    
    if(err != ESP_OK)
       ret = false;
        
    printf("Committing updates in NVS ... ");
    err = nvs_commit(handle);
    printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

        // Close
    nvs_close(handle);

    return ret;
}