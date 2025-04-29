#include "gpio_flush.h"
#include "head.h" // 引入公共类
#include "a_led_event.h"
#include "a_nvs_flash.h" // nvs_flash应用类
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"

#define TAG "GPIO-FLUSH"

// 反冲洗时间参数
flush_time_t FLUSH_TIME = {
    .POWER_ON = 18,              // 默认18秒
    .LOW_END = 18,               // 默认18秒
    .HIGH_START = 5,             // 默认5秒
    .HIGH_END = 10,              // 默认10秒
    .WATER_TOTAL = 7200,         // 默认累计制水2小时执行反冲洗
    .WATER_PRODUCTION_TIME = 18, // 默认累计制水执行18秒
};

// 定时器定义
static TimerHandle_t gpio_flush_end_timer = NULL;
// 冲洗结束的定时器回调
static void flush_timer_end_callback(TimerHandle_t xTimer);

// 冲洗参数更新
esp_err_t gpio_flush_data_update(void)
{
    int32_t flush_time = 0; // 定义冲洗时间
    if (a_nvs_flash_get_int("flush_power_on", &flush_time) == ESP_OK)
    {
        FLUSH_TIME.POWER_ON = (uint16_t)flush_time;
    }
    if (a_nvs_flash_get_int("flush_low_end", &flush_time) == ESP_OK)
    {
        FLUSH_TIME.LOW_END = (uint16_t)flush_time;
    }
    if (a_nvs_flash_get_int("flush_hs", &flush_time) == ESP_OK)
    {
        FLUSH_TIME.HIGH_START = (uint16_t)flush_time;
    }
    if (a_nvs_flash_get_int("flush_high_end", &flush_time) == ESP_OK)
    {
        FLUSH_TIME.HIGH_END = (uint16_t)flush_time;
    }
    if (a_nvs_flash_get_int("flush_wt", &flush_time) == ESP_OK)
    {
        FLUSH_TIME.WATER_TOTAL = (uint16_t)flush_time;
    }
    if (a_nvs_flash_get_int("flush_wp", &flush_time) == ESP_OK)
    {
        FLUSH_TIME.WATER_PRODUCTION_TIME = (uint16_t)flush_time;
    }
    return ESP_OK;
}

esp_err_t gpio_flush_init(void)
{
    // 创建冲洗结束定时器（用于控制冲洗持续时间）
    gpio_flush_end_timer = xTimerCreate(
        "FlushEndTimer",
        pdMS_TO_TICKS(1000), // 初始不设置周期，由 flush_process 动态设置
        pdFALSE,
        NULL,                    // 初始参数
        flush_timer_end_callback // 使用新的回调函数
    );
    if (gpio_flush_end_timer == NULL)
    {
        ESP_LOGE(TAG, "创建冲洗结束定时器失败");
        return ESP_FAIL;
    }
    gpio_flush_data_update(); // 更新冲洗参数

    gpio_flush_process(FLUSH_TIME.POWER_ON); // 开机自动冲洗xx秒

    // ESP_LOGI(TAG, "创建计时器后释放堆: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "初始化反冲洗任务完成");
    return ESP_OK;
}

// 冲洗结束的定时器回调
static void flush_timer_end_callback(TimerHandle_t xTimer)
{
    // uint32_t duration = (uint32_t)pvTimerGetTimerID(xTimer); // 原32位
    // ESP_LOGI(TAG, "冲洗结束，持续时间: %lu 秒", duration);
    gpio_flush_process(0); // 立即停止反冲洗
}

// 冲洗处理函数
void gpio_flush_process(uint16_t time)
{
    a_led_event_t event;
    event.type = LED_WATER_FLUSH;
    if (time == 0) // 立即停止反冲洗
    {
        ESP_LOGI(TAG, "停止冲洗");
        gpio_set_level(CONFIG_FLUSH_GPIO_NUM, 0); // 关闭冲洗阀
        // gpio_set_level(GPIO_FLUSH_LED, 1);  // 关闭冲洗LED
        event.data.led_water_flush = false;
        xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));
        return;
    }
    ESP_LOGI(TAG, "开始冲洗，持续时间: %u 秒", time);
    gpio_set_level(CONFIG_FLUSH_GPIO_NUM, 1); // 开启冲洗阀
    event.data.led_water_flush = true;
    xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));

    if (gpio_flush_end_timer == NULL)
    {
        ESP_LOGE(TAG, "冲洗结束定时器句柄不存在");
        return;
    }
    // 更改冲洗结束定时器的周期
    if (xTimerChangePeriod(gpio_flush_end_timer, pdMS_TO_TICKS(time * 1000), 0) != pdPASS)
    {
        ESP_LOGE(TAG, "更改冲洗结束定时器周期失败");
    }
    else
    {
        // 启动定时器
        if (xTimerStart(gpio_flush_end_timer, 0) == pdPASS)
        {
            ESP_LOGI(TAG, "冲洗结束计时器已启动"); // Flush end timer started
        }
        else
        {
            ESP_LOGE(TAG, "启动刷新结束计时器失败"); // Failed to start Flush end timer
        }
    }
}