#ifndef GPIO_WATER_H
#define GPIO_WATER_H

#include "freertos/FreeRTOS.h"
#include "esp_err.h"

extern TaskHandle_t xTaskHandle_water;
esp_err_t gpio_water_init(void);

#endif