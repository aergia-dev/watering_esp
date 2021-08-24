#include <stdio.h>
#include <stdbool.h>

void nvs_init();
bool nvs_write_uint32(char *key, uint32_t val);
uint32_t nvs_read_uint32(char* key, uint32_t default_val);
void erase_all();
