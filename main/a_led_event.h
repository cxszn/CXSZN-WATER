#ifndef _A_LED_EVENT_H_
#define _A_LED_EVENT_H_

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
// #include "esp_err.h"

// LED事件类型枚举
typedef enum
{
    LED_TDS_RAW,              // 原水TDS水质数显，最大值188
    LED_TDS_PURE,             // 纯水TDS水质数显，最大值188
    LED_WATER,                // 制水状态
    LED_WATER_LOW,            // 低压状态
    LED_WATER_HIGH,           // 高压状态
    LED_WATER_FLUSH,          // 冲洗状态
    LED_WATER_LEAK,           // 漏水状态
    LED_WATER_FILTER_ELEMENT, // 滤芯级别
    LED_ICON,                 // 图标状态
    LED_SIGNAL                // 信号状态
} a_led_event_type_t;

// LED事件结构体
typedef struct
{
    a_led_event_type_t type; // 事件类型
    // void *content;         // 事件内容指针
    union
    {
        uint8_t led_tds_raw;
        uint8_t led_tds_pure;
        bool led_water;
        bool led_water_low;
        bool led_water_high;
        bool led_water_flush;
        bool led_water_leak;
        uint8_t led_water_filter_element;
        bool led_icon;
        bool led_signal;
    } data;
} a_led_event_t;

// 声明LED事件队列句柄
extern QueueHandle_t a_led_event_queue;

void a_led_event_task(void *pvParameters);
esp_err_t a_led_timer(int8_t num);

#endif