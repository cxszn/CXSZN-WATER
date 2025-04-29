#ifndef APP_SERVICE_H
#define APP_SERVICE_H

#include "esp_err.h"

esp_err_t a_service_expiry_set(uint8_t charging, int32_t expire_time);
esp_err_t a_service_expiry_check(void);

#endif