/**
 * gpio_flowmeter.h
 * 流量计接口类.
 */
#ifndef GPIO_FLOWMETER_H // 检查是否未定义
#define GPIO_FLOWMETER_H // 如果未定义，则定义
#include "esp_err.h"     // 使用正确的ESP错误码定义
#include <inttypes.h>    // 添加这一行以确保 PRIu32 可用

esp_err_t gpio_flowmeter_init(void);
uint32_t gpio_flowmeter_get_pulse_count(void);
void gpio_flowmeter_reset_count(void);
// float gpio_flowmeter_get_liters(void);

#endif