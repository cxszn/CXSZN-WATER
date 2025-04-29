#ifndef CHECK_A_NVS_FLASH_H // 检查是否未定义
#define CHECK_A_NVS_FLASH_H // 如果未定义，则定义

#include "esp_err.h"

esp_err_t a_nvs_flash_init(void);
esp_err_t a_nvs_flash_insert(const char *key, const char *value);
char *a_nvs_flash_get(const char *key);
// esp_err_t app_nvs_flash_get_multiple(const char **keys, char **values, size_t num_keys);
esp_err_t a_nvs_flash_insert_int(const char *key, int32_t value);
esp_err_t a_nvs_flash_get_int(const char *key, int32_t *value);
esp_err_t a_nvs_flash_del(const char *key);
#endif