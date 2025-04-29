#include "gpio_buzzer.h" // 蜂鸣器类
#include "head.h"
#include "driver/gpio.h" // 制水GPIO配置针脚声明
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

#define TAG "GPIO-BUZZER"

/**
 * 蜂鸣器控制输出
 * true输出 false关闭
 */
esp_err_t gpio_buzzer_output(bool type)
{
    if (type)
    {
        gpio_set_level(CONFIG_BUZZER_GPIO_NUM, 1);
    }
    else
    {
        gpio_set_level(CONFIG_BUZZER_GPIO_NUM, 0);
    }
    return ESP_OK;
}

// 自定义传递的间隔秒数
static uint8_t buzzer_interval_sec = 1; // 默认间隔为 1 秒

// 定时器句柄
TimerHandle_t buzzer_timer = NULL;

static void buzzer_timer_callback(TimerHandle_t xTimer);

/**
 * 蜂鸣器间隔声响
 * 0 结束 大于0则间隔秒数
 */
esp_err_t gpio_buzzer_timer(uint8_t num)
{
    if (num == 0)
    {
        if (buzzer_timer != NULL)
        {
            gpio_buzzer_output(false);
            xTimerStop(buzzer_timer, 0); // 停止定时器
            ESP_LOGI(TAG, "蜂鸣器定时器已停止");
        }
    }
    else
    {
        buzzer_interval_sec = num;
        ESP_LOGI(TAG, "蜂鸣器间隔时间设置为 %u 秒", buzzer_interval_sec);

        // 如果定时器已经创建，则停止并删除之前的定时器
        if (buzzer_timer != NULL)
        {
            // 如果定时器已经在运行，先停止它
            xTimerStop(buzzer_timer, 0);
            // 删除之前的定时器实例
            xTimerDelete(buzzer_timer, 0);
            buzzer_timer = NULL; // 重新初始化定时器句柄
        }

        // 创建定时器，定时器每隔 buzzer_interval_sec 秒触发一次
        buzzer_timer = xTimerCreate("Buzzer Timer", pdMS_TO_TICKS(buzzer_interval_sec * 1000), pdTRUE, (void *)0, buzzer_timer_callback);
        if (buzzer_timer != NULL)
        {
            xTimerStart(buzzer_timer, 0); // 启动定时器
            ESP_LOGI(TAG, "蜂鸣器定时器已启动");
        }
        else
        {
            ESP_LOGE(TAG, "定时器创建失败");
        }
    }
    return ESP_OK;
}

// 定时器回调函数，周期性地控制蜂鸣器的开关
static void buzzer_timer_callback(TimerHandle_t xTimer)
{
    static bool buzzer_state = false;

    if (buzzer_state)
    {
        gpio_buzzer_output(false);
    }
    else
    {
        gpio_buzzer_output(true); // 如果当前蜂鸣器是关着的，则启动它
    }
    buzzer_state = !buzzer_state; // 切换蜂鸣器状态
}
