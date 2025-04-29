#include "a_network.h" // 网络工作类
#include "head.h"
#include "a_led_event.h"
#include "gpio_buzzer.h" // 蜂鸣器类
#include "u4g_at_cmd.h"
#include "u4g_at_http.h"
#include "u4g_data.h"
#include "a_nvs_flash.h" // nvs_flash应用类
#include "a_feedback.h"
#include "a_service.h" // 应用服务类
#include "a_time.h"
#include "cJSON.h"
#include "esp_log.h"
#include <string.h>
#include "esp_task_wdt.h" // 包含看门狗相关库
#include "esp_timer.h"    // 添加此行以包含时间相关函数

#define TAG "A_NETWORK"

#define DEBUG 0

#define NETWORK_RETRY_THRESHOLD 5 // 重试次数阈值

/******************************/
/*  1. 通用辅助函数          */
/******************************/

/**
 * @brief 根据返回码决定是否重启 4G 模块，并更新重试计数
 */
static void reset_device_if_needed(uint16_t *retry_count, emU4GResult ret)
{
    if (ret != U4G_OK)
    {
        (*retry_count)++;
        if (*retry_count > NETWORK_RETRY_THRESHOLD)
        {
            ESP_LOGE(TAG, "重试次数超过 %d，硬重启 4G 模块", NETWORK_RETRY_THRESHOLD);
            u4g_at_reboot_hard();
            // *retry_count = 0;
        }
        else if (*retry_count > 100)
        {
            ESP_LOGE(TAG, "重试次数超过 %d，重启ESP系统", 100);
            esp_restart();
        }
    }
}
/**
 * @brief 从 NVS 读取 IMEI，长度校验
 * @param[out] imei 输出指针
 * @return ESP_OK 或 ESP_FAIL
 */
static esp_err_t get_imei_from_nvs(char **imei)
{
    *imei = a_nvs_flash_get("deviceid");
    if (*imei == NULL || strlen(*imei) < 10)
    {
        ESP_LOGW(TAG, "NVS 中未找到合法 IMEI");
        return ESP_FAIL;
    }
    return ESP_OK;
}

/******************************/
/*  2. 网络初始化任务         */
/******************************/
/**
 * @brief 网络初始化任务
 */
void a_network_init(void *pvParameters)
{
    ESP_LOGI(TAG, "网络初始化任务启动");
    esp_task_wdt_add(NULL); // 注册到 Task WDT
    a_led_timer(1);         // 灯闪烁提示

    DEVICE.NETSTATE = DEVICE_NETRUN; // 联网中
    bool inited = false;
    bool checked = false;
    bool check_net_not = false;
    bool authed = false;
    emU4GResult ret;
    uint16_t retry_count = 0;

    while (1)
    {
        esp_task_wdt_reset();

        // 2.1 AT 指令初始化
        if (!inited)
        {
            ret = u4g_at();
            reset_device_if_needed(&retry_count, ret);
            if (ret == U4G_OK)
            {
                inited = true;
                retry_count = 0; // 重置重试错误
                ESP_LOGI(TAG, "AT 模块初始化成功");
            }
            else
            {
                ESP_LOGW(TAG, "AT 初始化失败，重试中...");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        // 2.2 网络状态检测
        if (!checked)
        {
            ret = u4g_at_netstatus();
            reset_device_if_needed(&retry_count, ret);
            if (ret == U4G_OK)
            {
                checked = true;
                DEVICE.NETSTATE = DEVICE_NETON; // 已联网
                a_led_timer(0);                 // 灯常亮
                retry_count = 0;                // 重置重试错误
                ESP_LOGI(TAG, "设备-4G已联网");
            }
            else if (ret == U4G_STATE_AT_NET_NOT)
            {
                ESP_LOGW(TAG, "设备-未联网,等待重试");
                if (!check_net_not)
                {
                    ESP_LOGI(TAG, "设备-检测到未联网,执行首次离线流程");
                    a_led_timer(-1); // 未联网-常灭
                    DEVICE.NETSTATE = DEVICE_NETOFF;
                    a_time_sync_offline();    // 执行获取离线时间
                    a_service_expiry_check(); // 判断是否到期
                    check_net_not = true;     // 已执行离线流程
                }
                for (int i = 0; i < 10; i++)
                {
                    esp_task_wdt_reset();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                continue;
            }
            else
            {
                ESP_LOGW(TAG, "设备-网络模块异常，准备系统重启进度：%d/5", retry_count);
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        // 2.3 获取并保存 IMEI
        if (checked && !authed)
        {
            char *imei = NULL;
            if (get_imei_from_nvs(&imei) != ESP_OK)
            {
                // NVS 未配置，执行 AT 获取
                ret = u4g_at_imei_get();
                reset_device_if_needed(&retry_count, ret);
                if (ret == U4G_OK)
                {
                    imei = u4g_data_get()->imei;
                    if (a_nvs_flash_insert("deviceid", imei) != ESP_OK) // 保存设备ID
                    {
                        ESP_LOGE(TAG, "[4G] 设备ID保存失败");
                        continue; // 继续循环
                    }
                    retry_count = 0; // 重置重试错误
                }
                else
                {
                    ESP_LOGW(TAG, "IMEI 获取失败");
                    vTaskDelay(pdMS_TO_TICKS(3000));
                    continue;
                }
            }
            strncpy(DEVICE.IMEI, imei, sizeof(DEVICE.IMEI));
            // free(imei); // 释放堆内存
            ESP_LOGI(TAG, "获取设备IMEI: %s", DEVICE.IMEI);
        }

        // 2.4 设备认证
        if (checked && !authed)
        {
            esp_err_t auth_ret = network_auth_4g();
            reset_device_if_needed(&retry_count, auth_ret);
            if (auth_ret == 200)
            {
                ESP_LOGI(TAG, "设备认证-入库成功");
            }
            else if (auth_ret == 3001)
            {
                ESP_LOGI(TAG, "设备认证-未激活，等待后重试");
                vTaskDelay(pdMS_TO_TICKS(5000));
                // continue;
            }
            else if (auth_ret == 3002)
            {
                ESP_LOGI(TAG, "设备认证-已激活");
                authed = true;
            }
            else if (auth_ret == 1001)
            {
                ESP_LOGE(TAG, "设备认证-认证秘钥无效-执行清空设备id和秘钥");
                a_nvs_flash_del("deviceid");
                a_nvs_flash_del("key");
            }
            else
            {
                ESP_LOGW(TAG, "设备认证-认证异常，code=%d，重试中", auth_ret);
                vTaskDelay(pdMS_TO_TICKS(3000));
                continue;
            }
        }
        // 全部完成则跳出
        if (inited && checked && authed)
        {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 2.5 同步时间与服务检查
    ESP_LOGI(TAG, "网络初始化完成，开始时间同步");
    esp_task_wdt_reset();
    if (a_time_sync() != ESP_OK)
    {
        ESP_LOGE(TAG, "时间同步失败，系统重启");
        esp_restart();
    }
    ESP_LOGI(TAG, "开始判断设备到期");
    a_service_expiry_check(); // 判断到期
    esp_task_wdt_reset();

    ESP_LOGI(TAG, "启动反馈任务");
    if (a_feedback_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "反馈初始化失败,执行重启");
        esp_restart();
    }

    // 退出任务
    ESP_LOGI(TAG, "网络初始化任务结束");
    esp_task_wdt_delete(NULL);
    vTaskDelete(NULL);
}

esp_err_t network_auth_4g(void)
{
    ESP_LOGI(TAG, "[4G] 正在获取KEY...");
    char *key = a_nvs_flash_get("key"); // 读取设备ID的key秘钥
    char *path = malloc(256);           // 从堆分配
    if (!path)
    {
        ESP_LOGE(TAG, "内存分配失败");
        return ESP_FAIL;
    }
    if (key == NULL || strlen(key) < 10)
    {
        ESP_LOGI(TAG, "设备认证秘钥为空 执行4G配网入库");
        if (u4g_at_mccid_get() != U4G_OK)
        {
            ESP_LOGW(TAG, "[4G] send CMD-MCCID FAIL");
        }
        char *mccid = u4g_data_get()->mccid;
        snprintf(path, 256, "/api/v1/device/auth?deviceid=%s&model=%s&version=%d&mccid=%s", DEVICE.IMEI, CONFIG_PROJECT_NAME, CONFIG_BOOTLOADER_PROJECT_VER, mccid);
    }
    else
    {
        ESP_LOGD(TAG, "[4G] 获取KEY：%s", key);
        ESP_LOGI(TAG, "获取KEY成功 执行4G配网验证");
        snprintf(path, 200, "/api/v1/device/auth?deviceid=%s&model=%s&key=%s", DEVICE.IMEI, CONFIG_PROJECT_NAME, key);
        free(key);
    }
    emU4GResult ret = u4g_at_http_request(HTTP_URL, path, NULL);
    free(path);
    esp_task_wdt_reset(); // 喂狗
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "HTTP GET请求失败 错误码: %d", ret);
        return ESP_FAIL;
    }
    char *u4g_data = (char *)u4g_data_get()->data;
    if (u4g_data == NULL)
    {
        ESP_LOGE(TAG, "u4g_data 无数据");
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "HTTP GET请求成功 提取JSON数据: %s", u4g_data);

    // 解析JSON数据
    cJSON *root = cJSON_Parse(u4g_data);
    if (!root)
    {
        ESP_LOGE(TAG, "解析JSON数据失败");
        return ESP_FAIL;
    }

    // 获取"code"字段
    cJSON *code_item = cJSON_GetObjectItem(root, "code");
    if (!cJSON_IsNumber(code_item))
    {
        ESP_LOGE(TAG, "code 字段无效");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    if (code_item->valueint == 200)
    {
        ESP_LOGI(TAG, "配网入库请求成功");

        // 获取"data"对象
        cJSON *data_item = cJSON_GetObjectItem(root, "data");
        if (!cJSON_IsObject(data_item))
        {
            ESP_LOGE(TAG, "data 字段无效");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        // 获取"key"字段
        cJSON *key_item = cJSON_GetObjectItem(data_item, "key");
        if (!cJSON_IsString(key_item))
        {
            ESP_LOGE(TAG, "key 字段无效");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
        // 保存 key 到 NVS
        if (a_nvs_flash_insert("key", key_item->valuestring) == ESP_OK)
        {
            ESP_LOGI(TAG, "[4G] key值已配置到设备中: %s", key_item->valuestring);
            ESP_LOGI(TAG, "[4G] 配网入库成功");
            cJSON_Delete(root);
            return 200;
        }
        else
        {
            ESP_LOGE(TAG, "[4G] 保存设备key键值失败");
            cJSON_Delete(root);
            return ESP_FAIL;
        }
    }
    else if (code_item->valueint == 3001)
    {
        cJSON_Delete(root);
        return 3001;
    }
    else if (code_item->valueint == 3002)
    {
        cJSON_Delete(root);
        return 3002;
    }
    else if (code_item->valueint == 1001)
    {
        cJSON_Delete(root);
        return 1001;
    }
    else
    {
        ESP_LOGE(TAG, "未知错误，代码: %d", code_item->valueint);
        // 清空JSON数据以节省内存
        cJSON_Delete(root);
        return ESP_FAIL;
    }
}
