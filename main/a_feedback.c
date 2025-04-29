#include "a_feedback.h" // 设备反馈
#include "a_network.h"  // 网络工作类
#include "head.h"
#include "u4g_at_http.h"
#include "u4g_data.h"
#include "u4g_at_cmd.h"
#include "a_nvs_flash.h" // nvs_flash应用类
#include "a_service.h"   // 应用服务类
#include "a_led_event.h"
#include "gpio_water.h"
#include "gpio_flush.h"
#include "freertos/timers.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_task_wdt.h" // 包含看门狗相关库
#include <string.h>

#define TAG "A_FEEDBACK" // 日志标签

#define DEBUG 0

TaskHandle_t xTaskHandle_feedback = NULL; // 通知执行get和post
// xTaskNotifyGive(xTaskHandle_net); // 触发通知设备反馈
static TimerHandle_t timer_feedback = NULL;

static void a_feedback_task(void *pvParameters);
static bool submit_httpget();
static bool submit_httppost(char *key);
static void timer_feedback_callback(TimerHandle_t xTimer);

esp_err_t a_feedback_init(void)
{
    // 创建事件处理任务
    if (xTaskCreate(a_feedback_task, "a_feedback_task", 6144, NULL, 6, &xTaskHandle_feedback) != pdPASS)
    {
        ESP_LOGE(TAG, "创建 a_feedback_task 失败");
        return ESP_FAIL;
    }

    // 创建定时器
    timer_feedback = xTimerCreate(
        "timer_feedback",
        pdMS_TO_TICKS(DEVICE.duration_s * 1000), // 初始不设置周期，由 flush_process 动态设置
        pdFALSE,
        NULL,                   // 初始参数
        timer_feedback_callback // 使用新的回调函数
    );
    if (timer_feedback == NULL)
    {
        ESP_LOGE(TAG, "创建冲洗结束定时器失败");
        return ESP_FAIL;
    }
    // 启动定时器
    if (xTimerStart(timer_feedback, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "启动定时器失败");
        return ESP_FAIL;
    }
    DEVICE_STATUS.feedback_init = true;
    return ESP_OK;
}
// static bool a_network_init_task_wdt_added = false; // 定义一个静态变量，记录当前任务是否已添加到任务看门狗中

// 任务
static void a_feedback_task(void *pvParameters)
{
    // if (!a_network_init_task_wdt_added)
    // {
    // esp_task_wdt_add(NULL); // 将当前任务添加到任务看门狗中
    //     a_network_init_task_wdt_added = true; // 设置标志位为 true，表示任务已经注册到看门狗
    // }
    // esp_task_wdt_delete(NULL); // 看门狗-卸载此任务
    while (1)
    {
        // esp_task_wdt_reset();                                                 // 每个AT命令后重置看门狗
        if (xTaskNotifyWait(0x00, 0xFFFFFFFF, NULL, portMAX_DELAY) == pdTRUE) // 等待通知
        {
            ESP_LOGI(TAG, "收到反馈通知");
            char *key = a_nvs_flash_get("key"); // 读取设备ID的key秘钥
            if (key == NULL || strlen(key) < 10)
            {
                ESP_LOGI(TAG, "key值为空");
                network_auth_4g();
                continue; // 继续循环
            }
            // esp_task_wdt_reset();
#ifndef DEBUG
            int64_t start_time = esp_timer_get_time(); // 记录开始时间-微秒
#endif
            submit_httppost(key);
#ifndef DEBUG
            ESP_LOGI(TAG, "handle_at_init-耗时 %.3f 秒", (esp_timer_get_time() - start_time) / 1000000.0); // 计算耗时（秒）
#endif
            // esp_task_wdt_reset();
            vTaskDelay(pdMS_TO_TICKS(1000));
            // esp_task_wdt_reset();
#ifndef DEBUG
            int64_t start_time = esp_timer_get_time(); // 记录开始时间-微秒
#endif
            submit_httpget();
#ifndef DEBUG
            ESP_LOGI(TAG, "handle_at_init-耗时 %.3f 秒", (esp_timer_get_time() - start_time) / 1000000.0); // 计算耗时（秒）
#endif
            // xTaskNotifyGive(xTaskHandle_water);
            // UBaseType_t uxHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
            // ESP_LOGE(TAG, "任务栈的最小剩余空间: %d", uxHighWaterMark);
        }
    }
}

// 获取设备信息
static bool submit_httpget()
{
    char path[200];
    snprintf(path, sizeof(path), "/api/v1/device/%s/%s", CONFIG_PROJECT_NAME, DEVICE.IMEI);
    // 发送 HTTP GET 请求
    emU4GResult ret = u4g_at_http_request(HTTP_URL, path, NULL);
    // esp_task_wdt_reset();
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "HTTP GET请求失败 错误码: %d", ret);
        return false;
    }
    char *u4g_data = (char *)u4g_data_get()->data;
    if (u4g_data == NULL)
    {
        ESP_LOGE(TAG, "u4g_data 无数据");
        return false;
    }

    // 解析JSON数据
    cJSON *root = cJSON_Parse(u4g_data);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "解析JSON数据失败");
        cJSON_Delete(root);
        return false; // 直接返回，避免调用 cJSON_Delete
    }
    // 获取"code"字段
    cJSON *code_item = cJSON_GetObjectItem(root, "code");
    if (!cJSON_IsNumber(code_item))
    {
        ESP_LOGE(TAG, "code 字段无效");
        cJSON_Delete(root);
        return false;
    }
    if (code_item->valueint == 200)
    {
        ESP_LOGI(TAG, "接收成功数据执行逐步操作");
        // 获取"data"对象
        cJSON *data_item = cJSON_GetObjectItem(root, "data");
        if (!cJSON_IsObject(data_item))
        {
            ESP_LOGE(TAG, "data 字段无效");
            cJSON_Delete(root);
            return false;
        }

        // 获取计费模式
        cJSON *charging_item = cJSON_GetObjectItem(data_item, "charging");
        if (!cJSON_IsNumber(charging_item))
        {
            ESP_LOGE(TAG, "charging 字段无效");
            cJSON_Delete(root);
            return false;
        }
        if (charging_item->valueint == 0)
        {
            ESP_LOGI(TAG, "下发 计费模式：永久");
            a_service_expiry_set(0, 0);
        }
        else if (charging_item->valueint == 1)
        {
            ESP_LOGI(TAG, "下发 计费模式：计时");
            cJSON *expire_time_item = cJSON_GetObjectItem(data_item, "expire_time");
            if (cJSON_IsNumber(expire_time_item))
            {
                int32_t charging = 0;
                a_nvs_flash_get_int("charging", &charging);
                int32_t expire_time = 0;
                a_nvs_flash_get_int("expire_time", &expire_time); // 到期时间
                if (charging != charging_item->valueint || expire_time != expire_time_item->valueint)
                {
                    ESP_LOGI(TAG, "[到期时间]从flash获取: %d", expire_time_item->valueint);
                    a_service_expiry_set(1, expire_time_item->valueint);
                }
                else
                {
                    ESP_LOGI(TAG, "[到期时间]下发到期时间与flash缓存相等");
                    a_service_expiry_check();
                }
            }
            else
            {
                ESP_LOGE(TAG, "未获取到计时时间戳");
            }
        }

        // 获取滤芯值
        cJSON *filter_level_item = cJSON_GetObjectItem(data_item, "filter_level");
        if (cJSON_IsNumber(filter_level_item))
        {
            ESP_LOGI(TAG, "下发滤芯值：%d", filter_level_item->valueint);
            a_led_event_t event;
            event.type = LED_WATER_FILTER_ELEMENT;
            event.data.led_water_filter_element = filter_level_item->valueint;
            xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));
        }
        else
        {
            ESP_LOGE(TAG, "filter_level 字段无效");
            cJSON_Delete(root);
            return false;
        }

        // 步长(秒)
        cJSON *duration_s_item = cJSON_GetObjectItem(data_item, "duration_s");
        if (cJSON_IsNumber(duration_s_item))
        {
            ESP_LOGI(TAG, "下发步长：%d", duration_s_item->valueint);
            DEVICE.duration_s = duration_s_item->valueint;
        }

        // 累计制水故障(秒)always_water_time

        // 反冲洗flush
        cJSON *flush_item = cJSON_GetObjectItem(data_item, "flush");
        if (cJSON_IsObject(flush_item))
        {
            // 开机冲洗
            cJSON *item_flush_power_on = cJSON_GetObjectItem(flush_item, "power_on");
            if (cJSON_IsNumber(item_flush_power_on))
            {
                a_nvs_flash_insert_int("flush_power_on", item_flush_power_on->valueint);
            }
            else
            {
                ESP_LOGE(TAG, "power_on 字段无效");
                cJSON_Delete(root);
                return false;
            }
            // 低压结束
            cJSON *item_flush_low_end = cJSON_GetObjectItem(flush_item, "low_end");
            if (cJSON_IsNumber(item_flush_low_end))
            {
                a_nvs_flash_insert_int("flush_low_end", item_flush_low_end->valueint);
            }
            else
            {
                ESP_LOGE(TAG, "low_end 字段无效");
                cJSON_Delete(root);
                return false;
            }
            // 高压进入
            cJSON *item_flush_high_start = cJSON_GetObjectItem(flush_item, "high_start");
            if (cJSON_IsNumber(item_flush_high_start))
            {
                a_nvs_flash_insert_int("flush_hs", item_flush_high_start->valueint);
            }
            else
            {
                ESP_LOGE(TAG, "high_start 字段无效");
                cJSON_Delete(root);
                return false;
            }
            // 高压结束
            cJSON *item_flush_high_end = cJSON_GetObjectItem(flush_item, "high_end");
            if (cJSON_IsNumber(item_flush_high_end))
            {
                a_nvs_flash_insert_int("flush_high_end", item_flush_high_end->valueint);
            }
            else
            {
                ESP_LOGE(TAG, "high_end 字段无效");
                cJSON_Delete(root);
                return false;
            }
            // 累计制水
            cJSON *item_flush_water_total = cJSON_GetObjectItem(flush_item, "water_total");
            if (cJSON_IsNumber(item_flush_water_total))
            {
                a_nvs_flash_insert_int("flush_wt", item_flush_water_total->valueint);
            }
            else
            {
                ESP_LOGE(TAG, "water_total 字段无效");
                cJSON_Delete(root);
                return false;
            }
            // 累计制水冲洗
            cJSON *item_flush_water_prouction_time = cJSON_GetObjectItem(flush_item, "water_prouction_time");
            if (cJSON_IsNumber(item_flush_water_prouction_time))
            {
                a_nvs_flash_insert_int("flush_wp", item_flush_water_prouction_time->valueint);
            }
            else
            {
                ESP_LOGE(TAG, "water_prouction_time 字段无效");
                cJSON_Delete(root);
                return false;
            }
            gpio_flush_data_update();
        }
        else
        {
            ESP_LOGE(TAG, "flush 字段无效");
            cJSON_Delete(root);
            return false;
        }
    }
    cJSON_Delete(root); // 解析完成后释放 JSON 对象
    return true;
}

// 提交设备信息
static bool submit_httppost(char *key)
{
    emU4GResult ret = u4g_at_csq();
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "4G模块信号值 请求失败 错误码: %d", ret);
        return false;
    }
    int8_t csq = u4g_data_get()->csq;
    // 创建一个 cJSON 对象
    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
    {
        ESP_LOGE(TAG, "创建 JSON 对象失败");
        return false;
    }
    // 添加数据到 JSON 对象
    cJSON_AddNumberToObject(json, "total_water_time", DEVICE.total_water_time);
    cJSON_AddBoolToObject(json, "water_leak", DEVICE.WATER_LEAK);
    cJSON_AddNumberToObject(json, "tds_raw", DEVICE_TDSWD.raw_tds);
    cJSON_AddNumberToObject(json, "tds_pure", DEVICE_TDSWD.pure_tds);
    cJSON_AddNumberToObject(json, "temp_raw", DEVICE_TDSWD.raw_temperature);
    cJSON_AddNumberToObject(json, "temp_pure", DEVICE_TDSWD.pure_temperature);
    cJSON_AddNumberToObject(json, "flowmeter", DEVICE.flowmeter);
    cJSON_AddNumberToObject(json, "expire_time", DEVICE.expire_time);
    cJSON_AddNumberToObject(json, "signal", csq);
    // 将 JSON 对象转换为字符串
    char *body = cJSON_PrintUnformatted(json);
    if (body == NULL)
    {
        ESP_LOGE(TAG, "JSON 转换为字符串失败");
        cJSON_Delete(json); // 释放 JSON 对象
        return false;
    }

    char path[200];
    snprintf(path, sizeof(path), "/api/v1/device/%s/%s", CONFIG_PROJECT_NAME, DEVICE.IMEI);

    // 发送 HTTP POST 请求
    ret = u4g_at_http_request(HTTP_URL, path, body);
    // 释放内存
    free(body);         // 释放 JSON 字符串内存
    cJSON_Delete(json); // 释放 JSON 对象
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "HTTP POST请求失败 错误码: %d", ret);
        return false;
    }
    char *u4g_data = (char *)u4g_data_get()->data;
    if (u4g_data == NULL)
    {
        ESP_LOGE(TAG, "u4g_data 无数据");
        return false;
    }
    ESP_LOGD(TAG, "HTTP POST请求成功 提取JSON数据: %s", u4g_data);
    // 解析JSON数据
    cJSON *root = cJSON_Parse(u4g_data);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "解析JSON数据失败");
        return false;
    }
    // 获取"code"字段
    cJSON *code_item = cJSON_GetObjectItem(root, "code");
    if (!cJSON_IsNumber(code_item))
    {
        ESP_LOGE(TAG, "code 字段无效");
        cJSON_Delete(root);
        return false;
    }
    if (code_item->valueint == 200)
    {
        ESP_LOGI(TAG, "接收成功数据执行清零操作");
        DEVICE.flowmeter = 0;        // 流量计清零
        DEVICE.total_water_time = 0; // 累计制水清零
    }
    cJSON_Delete(root); // 解析完成后释放 JSON 对象
    return true;
}

// 定时器回调
static void timer_feedback_callback(TimerHandle_t xTimer)
{
    if (xTaskHandle_feedback != NULL)
    {
        xTaskNotifyGive(xTaskHandle_feedback); // 触发通知设备反馈
    }

    ESP_LOGI(TAG, "=====================================定时器回调执行==========================================================================");
    // 更新定时器周期
    if (xTimer != NULL)
    {
        ESP_LOGI(TAG, "===============接收到步长为：%ld", DEVICE.duration_s);
        // 将周期转换为 ticks
        TickType_t new_period = pdMS_TO_TICKS(DEVICE.duration_s * 1000); // 转换为毫秒
        if (xTimerChangePeriod(xTimer, new_period, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "更新定时器周期失败");
        }
    }
}