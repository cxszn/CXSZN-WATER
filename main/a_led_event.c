#include "a_led_event.h"
#include "gn1640t_driver.h"
#include "freertos/timers.h"
#include "esp_log.h"

#define TAG "LED_EVENT" // 日志标签

QueueHandle_t a_led_event_queue = NULL;

// 发布点亮LED任务队列
void a_led_event_task(void *pvParameters)
{
    gn1640t_init();         // 初始化LED驱动
    gn1640t_led_icon(true); // 启动文字显示
    a_led_event_t event;
    // gn1640t_led_water(true); // 测试
    while (1)
    {
        if (xQueueReceive(a_led_event_queue, &event, portMAX_DELAY)) // 阻塞等待队列消息
        {
            switch (event.type)
            {
            case LED_TDS_RAW: // 原水TDS数显
            {
                uint8_t value = event.data.led_tds_raw;
                gn1640t_led_tds_raw_water(value);
                break;
            }
            case LED_TDS_PURE: // 纯水TDS数显
            {
                uint8_t value = event.data.led_tds_pure;
                gn1640t_led_tds_pure_water(value);
                break;
            }
            case LED_WATER: // 制水状态
            {
                bool value = event.data.led_water;
                gn1640t_led_water(value);
                break;
            }
            case LED_WATER_LOW: // 低压状态
            {
                bool value = event.data.led_water_low;
                gn1640t_led_low(value);
                break;
            }
            case LED_WATER_HIGH: // 高压状态
            {
                bool value = event.data.led_water_high;
                gn1640t_led_high(value);
                break;
            }
            case LED_WATER_FLUSH: // 冲洗状态
            {
                bool value = event.data.led_water_flush;
                gn1640t_led_flush(value);
                break;
            }
            case LED_WATER_LEAK: // 漏水状态
            {
                bool value = event.data.led_water_leak;
                gn1640t_led_leak(value);
                break;
            }
            case LED_WATER_FILTER_ELEMENT: // 滤芯级别
            {
                uint8_t value = event.data.led_water_filter_element;
                gn1640t_led_filter_element(value);
                break;
            }
            case LED_ICON: // 图标状态
            {
                bool value = event.data.led_icon;
                gn1640t_led_icon(value);
                break;
            }
            case LED_SIGNAL: // 信号状态
            {
                bool value = event.data.led_signal;
                gn1640t_led_signal(value);
                break;
            }
            default:
            {
                ESP_LOGW(TAG, "未知事件类型: %d", event.type);
                break;
            }
            }
        }
    }
    vTaskDelete(NULL);
}

// 定时器回调函数，周期性地控制LED的开关
static void led_timer_callback(TimerHandle_t xTimer)
{
    static bool state = false;
    a_led_event_t event;

    event.type = LED_SIGNAL;
    if (state)
    {
        event.data.led_signal = false;
    }
    else
    {
        event.data.led_signal = true;
    }
    xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));
    state = !state; // 切换LED状态
}

// 定时器句柄
TimerHandle_t led_timer = NULL;
// 自定义传递的间隔秒数
static uint8_t led_interval_sec = 1; // 默认间隔为 1 秒
/**
 * 信号灯间隔亮灭
 * 0 已联网-常亮 则定时器停止
 * -1 未联网-常灭 则定时器停止
 * 1 联网中-间隔1秒亮灭
 */
esp_err_t a_led_timer(int8_t num)
{
    if (num == 0 || num == -1)
    {
        a_led_event_t event;
        event.type = LED_SIGNAL;
        if (num == 0)
        {
            event.data.led_signal = true;
        }
        else if (num == 0)
        {
            event.data.led_signal = false;
        }
        xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));

        if (led_timer != NULL)
        {
            xTimerStop(led_timer, 0); // 停止定时器
            // ESP_LOGI(TAG, "LED定时器已停止");
            // 删除之前的定时器实例
            xTimerDelete(led_timer, 0);
            led_timer = NULL; // 重新初始化定时器句柄
        }
    }
    else
    {
        led_interval_sec = num;
        // ESP_LOGI(TAG, "LED间隔时间设置为 %u 秒", led_interval_sec);

        // 如果定时器已经创建，则停止并删除之前的定时器
        if (led_timer != NULL)
        {
            // 如果定时器已经在运行，先停止它
            xTimerStop(led_timer, 0);
            // 删除之前的定时器实例
            xTimerDelete(led_timer, 0);
            led_timer = NULL; // 重新初始化定时器句柄
        }

        // 创建定时器，定时器每隔 led_interval_sec 秒触发一次
        led_timer = xTimerCreate("led Timer", pdMS_TO_TICKS(led_interval_sec * 1000), pdTRUE, (void *)0, led_timer_callback);
        if (led_timer != NULL)
        {
            xTimerStart(led_timer, 0); // 启动定时器
            // ESP_LOGI(TAG, "LED定时器已启动");
        }
        else
        {
            ESP_LOGE(TAG, "定时器创建失败");
        }
    }
    return ESP_OK;
}
