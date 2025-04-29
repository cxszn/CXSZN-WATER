#include "a_service.h" // 应用服务类
#include "head.h"
#include "a_nvs_flash.h" // nvs_flash应用类
#include "esp_log.h"
#include <time.h>

static const char *TAG = "APP-SERVICE";

typedef struct
{
    uint8_t charging;    // 计费模式 0永久 1计时
    int32_t expire_time; // 到期时间
} service_expiry_t;

esp_err_t a_service_expiry_set(uint8_t charging, int32_t expire_time)
{
    if (charging == 0)
    {
        ESP_LOGE(TAG, "计费模式：永久");
        a_nvs_flash_insert_int("charging", charging);
        a_nvs_flash_insert_int("expire_time", 0);
        DEVICE.MODE_EXPIRY = NOT_EXPIRY;
    }
    else if (charging == 1)
    {
        ESP_LOGI(TAG, "计费模式：计时");
        a_nvs_flash_insert_int("charging", charging);
        a_nvs_flash_insert_int("expire_time", expire_time);
        a_service_expiry_check();
    }
    else
    {
        ESP_LOGE(TAG, "计费模式：未知");
        return ESP_FAIL;
    }
    return ESP_OK;
}

// 到期判断服务类
esp_err_t a_service_expiry_check(void)
{
    int32_t charging = 0;
    a_nvs_flash_get_int("charging", &charging); // 计费模式 0永久 1计时
    if (charging == 0)
    {
        ESP_LOGE(TAG, "计费模式：永久");
        DEVICE.MODE_EXPIRY = NOT_EXPIRY;
        return ESP_OK;
    }

    esp_err_t ret = a_nvs_flash_get_int("expire_time", &DEVICE.expire_time); // 到期时间
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "未获取到到期时间数据");
    }

    // 检查到期时间
    if (DEVICE.expire_time == 0)
    {
        ESP_LOGE(TAG, "设备到期时间：未到期");
        DEVICE.MODE_EXPIRY = NOT_EXPIRY;
        return ESP_OK;
    }

    time_t now;
    time(&now); // 获取当前时间戳

    // 检查当前时间与到期时间的关系
    if (now >= DEVICE.expire_time)
    {
        ESP_LOGE(TAG, "设备到期时间：已到期");
        DEVICE.MODE_EXPIRY = EXPIRY; // 已到期
        return ESP_OK;
    }

    // 处理离线状态
    if (DEVICE.NETSTATE == DEVICE_NETOFF)
    {
        int32_t offline_time = 0;
        if (a_nvs_flash_get_int("offline_time", &offline_time) == ESP_OK) // 使用指针获取首次离线时间
        {
            int days_to_add = 7;                                    // 要增加的天数
            const int seconds_in_a_day = 86400;                     // 每天的秒数
            time_t seconds_to_add = days_to_add * seconds_in_a_day; // 计算增加的秒数
            if (now > (offline_time + seconds_to_add))
            {
                ESP_LOGE(TAG, "设备到期时间：已到期");
                DEVICE.MODE_EXPIRY = EXPIRY; // 已到期
                return ESP_OK;
            }
        }
    }

    ESP_LOGE(TAG, "设备到期时间：未到期");
    DEVICE.MODE_EXPIRY = NOT_EXPIRY;
    return ESP_OK;
}