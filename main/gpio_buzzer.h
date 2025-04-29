#ifndef GPIO_BUZZER_H
#define GPIO_BUZZER_H

#include "esp_err.h"
#include <stdbool.h>

esp_err_t gpio_buzzer_output(bool type);
esp_err_t gpio_buzzer_timer(uint8_t num);

#endif