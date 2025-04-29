#include "u4g_data.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include <string.h> // 增加字符串操作支持

#define TAG "U4G-DATA"

// 新增保护机制
static SemaphoreHandle_t data_mutex = NULL;

u4g_data_t u4g_data = {0};

emU4GResult u4g_data_init(void)
{
    data_mutex = xSemaphoreCreateMutex();
    if (data_mutex == NULL)
    {
        // 互斥锁创建失败
        ESP_LOGE(TAG, "发送命令互斥锁创建失败");
        return U4G_FAIL;
    }
    ESP_LOGI(TAG, "AT 命令模块初始化完成");
    return U4G_OK;
}

// 获取4G模组信息
u4g_data_t* u4g_data_get(void) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGW(TAG, "模块数据获取超时");
        return NULL;
    }
    xSemaphoreGive(data_mutex); // 快速释放锁（假设后续只读操作）
    return &u4g_data;
}

// 安全获取模块信息（返回数据副本）-- 占用空间极大
// u4g_data_t u4g_data_get(void)
// {
//     u4g_data_t module_copy = {0};
//     // 获取数据锁
//     if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(500)) == pdTRUE)
//     {
//         memcpy(&module_copy, &u4g_data, sizeof(u4g_data_t));
//         xSemaphoreGive(data_mutex);
//     }
//     else
//     {
//         ESP_LOGW(TAG, "模块数据获取超时，返回空数据");
//     }
//     return module_copy;
// }

// 应用层代码示例
// void app_task(void *arg) {
//     u4g_at_imei_refresh();
//     u4g_module_safe_t mod = u4g_at_module_get_safe();
//     ESP_LOGI("APP", "IMEI: %s (v%d)", mod.data.imei, mod.data_version);
//     // 检测数据更新
//     while(1) {
//         u4g_module_safe_t new_mod = u4g_at_module_get_safe();
//         if(new_mod.data_version != mod.data_version) {
//             ESP_LOGI("APP", "检测到IMEI更新: %s", new_mod.data.imei);
//             mod = new_mod;
//         }
//         vTaskDelay(1000);
//     }
// }

// // 线程安全存储
// static u4g_module_safe_t u4g_module_safe = {0};
// static SemaphoreHandle_t u4g_module_mutex = NULL;

// // 数据更新接口（AT响应解析器调用）
// void u4g_module_update(const u4g_module_t *new_data)
// {
//     if (u4g_module_mutex == NULL)
//         u4g_module_mutex = xSemaphoreCreateMutex();

//     xSemaphoreTake(u4g_module_mutex, portMAX_DELAY);
//     u4g_module_safe.data_version++;
//     memcpy(&u4g_module_safe.data, new_data, sizeof(u4g_module_t));
//     xSemaphoreGive(u4g_module_mutex);
// }

// // 数据获取接口
// u4g_module_safe_t u4g_at_module_safe_get(void)
// {
//     u4g_module_safe_t ret;
//     if (u4g_module_mutex == NULL)
//         u4g_module_mutex = xSemaphoreCreateMutex();

//     xSemaphoreTake(u4g_module_mutex, portMAX_DELAY);
//     memcpy(&ret, &u4g_module_safe, sizeof(u4g_module_safe_t));
//     xSemaphoreGive(u4g_module_mutex);
//     return ret;
// }

// 新增保护机制
// static SemaphoreHandle_t data_mutex = NULL;

// // 增强版获取接口
// u4g_module_t *u4g_at_module_get_safe(void)
// {
//     if (!data_mutex)
//         data_mutex = xSemaphoreCreateMutex();

//     xSemaphoreTake(data_mutex, portMAX_DELAY);
//     static u4g_module_t safe_copy;
//     memcpy(&safe_copy, &u4g_module, sizeof(u4g_module_t));
//     xSemaphoreGive(data_mutex);

//     return &safe_copy;
// }

// // 废弃原全局变量访问
// #define u4g_at_module_get() u4g_at_module_get_safe()
