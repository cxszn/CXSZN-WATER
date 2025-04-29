#include "gpio_water_timer.h"
#include "head.h"
#include "gpio_flush.h"
#include "esp_timer.h" // 添加此行以包含时间相关函数
#include "esp_log.h"

#define TAG "GPIO-WATER-TIMER"

// 累计制水时间参数
static uint64_t water_production_time_start = 0; // 上一个开始计算的耗时微秒
static uint64_t water_production_time = 0;       // 制水累计时间-秒

/**
 * 累计制水x小时,执行x秒反冲洗
 * true开始计时 false结束计时
 */
void gpio_water_production_time_set(bool type)
{
    int64_t microseconds = esp_timer_get_time(); // 记录时间-微秒
    if (microseconds > 0)                        // 正确响应
    {
        if (type) // 开始计时
        {
            water_production_time_start = microseconds; // 转为秒
            ESP_LOGI(TAG, "制水计时开始，起始时间: %llu 微秒", water_production_time_start);
        }
        else // 结束计时
        {
            if (water_production_time_start == 0)
            {
                ESP_LOGW(TAG, "制水计时尚未开始");
                return;
            }

            // 计算耗时
            uint64_t diff = microseconds - water_production_time_start; // 微秒
            uint64_t seconds = (uint64_t)(diff / 1000000);              // 转换为秒
            water_production_time += seconds;                           // 只存储非负的制水时间
            DEVICE.total_water_time += seconds;                         // 只存储非负的制水时间
            ESP_LOGI(TAG, "累计制水耗时 %lld 秒", water_production_time);

            if (water_production_time > FLUSH_TIME.WATER_TOTAL) // 累计制水时间超过
            {
                gpio_flush_process(FLUSH_TIME.WATER_PRODUCTION_TIME); // 执行反冲洗
                water_production_time = 0;                            // 重置制水时间
            }
        }
    }
    else // 响应失败
    {
        ESP_LOGE(TAG, "累计制水时间读取失败: %lld", microseconds);
    }
}