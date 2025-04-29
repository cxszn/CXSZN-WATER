#ifndef _A_FEEDBACK_H_
#define _A_FEEDBACK_H_

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_system.h"

extern TaskHandle_t xTaskHandle_feedback;

esp_err_t a_feedback_init(void);

#endif