#include <stdio.h>
#include "head.h"
#include "a_nvs_flash.h" // nvs_flash应用类
#include "a_led_event.h"
#include "a_network.h" // 网络工作类
#include "gpio_water.h"
#include "gpio_blackout.h"
#include "gpio_tds.h"
#include "gpio_buzzer.h"    // 蜂鸣器类
#include "gpio_flowmeter.h" // 头文件，用于获取函数声明
#include "u4g_uart.h"
#include "u4g_at_cmd.h"
#include "u4g_at_http.h"
#include "u4g.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h" // 包含看门狗相关库
#include "esp_log.h"
#include <string.h>
#define TAG "MAIN" // 日志标签
#include "hal/wdt_hal.h"
// #include "esp_wifi.h"

TaskHandle_t net_task_handle = NULL;
// xtensa-esp32s3-elf-addr2line -pfiaC -e build/CXSZN-WATER.elf 0x403c8b01

// 主任务初始化
static esp_err_t main_task_init(void)
{
    DEVICE.RUNSTATE = READY_RUN; // 就绪中
    esp_err_t ret;

    ret = a_nvs_flash_init(); // 初始化NVS应用类
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "a_nvs_flash_init初始化失败(错误码：%s)，执行重启", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    // -------- LED控制初始化 -------
    if (a_led_event_queue == NULL)
    {
        a_led_event_queue = xQueueCreate(10, sizeof(a_led_event_t));
        if (a_led_event_queue == NULL)
        {
            ESP_LOGE(TAG, "创建LED事件队列失败");
            return ESP_FAIL;
        }
    }
    // 创建事件回调任务
    if (xTaskCreate(a_led_event_task, "a_led_event_task", 4096, NULL, 10, NULL) != pdPASS)
    {
        ESP_LOGE(TAG, "创建LED事件处理任务失败");
        return ESP_FAIL;
    }
    if (gpio_water_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "gpio_water_init create fail");
        return ESP_FAIL;
    }
    if (gpio_tds_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "gpio_tds_init create fail");
        return ESP_FAIL;
    }
    if (gpio_flowmeter_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "gpio_flowmeter_init create fail");
        return ESP_FAIL;
    }

    if (gpio_blackout_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "gpio_blackout_init fail");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "主任务初始化完成");
    return ESP_OK;
}

// 联网任务初始化
static esp_err_t net_task_init(void)
{
    // AT初始化
    if (u4g_init() != U4G_OK)
    {
        ESP_LOGE(TAG, "u4g_init初始化失败");
        return ESP_FAIL;
    }
    // 创建网络初始化
    if (xTaskCreate(a_network_init, "a_network_init", 10240, NULL, 6, &net_task_handle) != pdPASS)
    {
        ESP_LOGE(TAG, "a_network_init create fail");
        return ESP_FAIL;
    }
    // xTaskCreatePinnedToCore(
    //     a_network_init,   // 任务函数
    //     "a_network_init", // 任务名称
    //     8192,             // 栈大小
    //     NULL,             // 参数
    //     0,                // 优先级（数值越低优先级越低）
    //     NULL,             // 任务句柄
    //     1                 // 核心编号（0=核心0，1=核心1）
    // );
    return ESP_OK;
}

/** 监控任务 */
void task_jk(void *arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(20000));
        vTaskDelay(pdMS_TO_TICKS(3000));
        ESP_LOGI(TAG, "监控中");
        // 获取格式化统计信息
        printf("┌─────────────────────获取CPU详情────────────────────┐\n");
        char pcWriteBuffer[512];
        vTaskGetRunTimeStats(pcWriteBuffer);
        printf("名称 | 数量 | CPU占用率\n");
        printf(" %s\n", pcWriteBuffer);
        printf("└───────────────────────────────────────────────────┘\n");

        printf("┌─────────────────────获取内存分布详情────────────────────┐\n");
        printf("Free DRAM: %d B | Free IRAM: %d B\n",
               heap_caps_get_free_size(MALLOC_CAP_8BIT),
               heap_caps_get_free_size(MALLOC_CAP_32BIT));
        printf("└───────────────────────────────────────────────────────┘\n");
        multi_heap_info_t info;
        heap_caps_get_info(&info, MALLOC_CAP_8BIT);
        printf("┌─────────────────────详细内存区域分析───────────────────────────────────────────────┐\n");
        printf("│ 已分配内存总量: %-6d KB   堆内存总空闲量: %-6d KB                                   │\n",
               info.total_allocated_bytes / 1024, info.total_free_bytes / 1024);
        printf("│ 最大连续空闲块: %-6d KB   历史最小空闲量: %-6d KB                                   │\n",
               info.largest_free_block / 1024, info.minimum_free_bytes / 1024);
        printf("│ 已分配内存块数量: %-6d   空闲内存块数量: %-6d                                       │\n",
               info.allocated_blocks, info.free_blocks);
        printf("│ 内存块总数: %-6d   Min Free: %-6d                                                 │\n",
               info.total_blocks, heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
        printf("│ 碎片化(最大连续空闲块/堆内存总空闲量): %.1f%% 阻碍(已分配内存块数量/内存块总数): %d/%d │\n",
               (1.0 - (float)info.largest_free_block / info.total_free_bytes) * 100,
               info.allocated_blocks, info.total_blocks);
        printf("└──────────────────────────────────────────────────────────────────────────────────┘\n");
    }
}

void app_main(void)
{
    // 初始化看门狗
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = 15000,
        // .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
        .idle_core_mask = 0, // 不监控空闲核心
        .trigger_panic = true};
    esp_err_t ret = esp_task_wdt_reconfigure(&twdt_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "看门狗初始化失败(错误码：%s)，执行重启", esp_err_to_name(ret));
        esp_restart();
    }

    xTaskCreate(task_jk, "task_jk", 4096, NULL, 0, NULL);
    gpio_config_t io_conf_output = {
        .pin_bit_mask = (1ULL << CONFIG_BUZZER_GPIO_NUM), // 蜂鸣器接口
        .mode = GPIO_MODE_OUTPUT,                         // 将GPIO引脚配置为输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,                // 禁用上拉电阻
        .pull_down_en = GPIO_PULLDOWN_DISABLE,            // 禁用下拉电阻
        .intr_type = GPIO_INTR_DISABLE                    // 禁用GPIO中断
    };
    if (gpio_config(&io_conf_output) != ESP_OK)
    {
        ESP_LOGE(TAG, "蜂鸣器初始化失败-执行esp重启");
        esp_restart();
    }
    gpio_buzzer_timer(0);   // 关闭蜂鸣器
    gpio_buzzer_timer(1.5); // 开启蜂鸣器
    for (int i = 0; i < 5; i++)
    {
        ESP_LOGE(TAG, "等待时间：%d秒", i);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (main_task_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "主任务初始化失败-执行esp重启");
        gpio_buzzer_timer(0); // 关闭蜂鸣器
        esp_restart();
    }
    if (net_task_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "联网任务初始化失败-执行esp重启");
        gpio_buzzer_timer(0); // 关闭蜂鸣器
        esp_restart();
    }
    gpio_buzzer_timer(0); // 关闭蜂鸣器
    ESP_LOGI(TAG, "MAIN初始化完成");
}

// void app_main2(void)
// {
//     // wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT();
//     // wdt_hal_write_protect_disable(&rtc_wdt_ctx);
//     // wdt_hal_disable(&rtc_wdt_ctx); // 禁用 RTC_WDT硬件看门狗定时器
//     // wdt_hal_write_protect_enable(&rtc_wdt_ctx);

//     // 初始化看门狗
//     // esp_task_wdt_config_t twdt_config = {
//     //     .timeout_ms = 10000,
//     //     // .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
//     //     .idle_core_mask = 0,
//     //     .trigger_panic = true};
//     // // ret = esp_task_wdt_init(&twdt_config);
//     // ret = esp_task_wdt_reconfigure(&twdt_config);
//     // if (ret != ESP_OK)
//     // {
//     //     ESP_LOGE(TAG, "esp_task_wdt_init初始化失败(错误码：%s)，执行重启", esp_err_to_name(ret));
//     //     esp_restart();
//     // }

//     // esp_wifi_set_mode(WIFI_MODE_NULL); // 关闭 Wi-Fi

//     // gpio_config_t io_conf_output = {
//     //     .pin_bit_mask = (1ULL << CONFIG_BUZZER_GPIO_NUM) // 蜂鸣器接口
//     //     ,                                                // 选择要配置的引脚
//     //     .mode = GPIO_MODE_OUTPUT,                        // 将GPIO引脚配置为输出模式
//     //     .pull_up_en = GPIO_PULLUP_DISABLE,               // 禁用上拉电阻
//     //     .pull_down_en = GPIO_PULLDOWN_DISABLE,           // 禁用下拉电阻
//     //     .intr_type = GPIO_INTR_DISABLE                   // 禁用GPIO中断
//     // };
//     // gpio_config(&io_conf_output);

//     // xTaskCreate(task_jk, "task_jk", 4096, NULL, 0, NULL);
//     esp_task_wdt_add(NULL);
//     // esp_task_wdt_deinit();
//     gpio_buzzer_timer(1.5); // 开启蜂鸣器将1秒
//     for (int i = 0; i < 5; i++)
//     {
//         ESP_LOGE(TAG, "等待时间：%d秒", i);
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
//     // main_init();
//     gpio_buzzer_timer(0); // 开启蜂鸣器将1秒

//     // 初始化看门狗
//     // esp_task_wdt_config_t twdt_config = {
//     //     .timeout_ms = 10000,
//     //     // .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
//     //     .idle_core_mask = 0,
//     //     .trigger_panic = true};
//     // // ret = esp_task_wdt_init(&twdt_config);
//     // if (esp_task_wdt_reconfigure(&twdt_config) != ESP_OK)
//     // {
//     //     ESP_LOGE(TAG, "esp_task_wdt_init初始化失败，执行重启");
//     //     esp_restart();
//     // }
//     // esp_task_wdt_add(NULL);

//     // AT初始化
//     // if (u4g_init() != U4G_OK)
//     // {
//     //     ESP_LOGE(TAG, "u4g_init");
//     //     esp_restart();
//     // }

//     while (1)
//     {
//         ESP_LOGI(TAG, "监控中，已喂狗");
//         // UBaseType_t watermark = uxTaskGetStackHighWaterMark(uart_recv_task_handle);
//         //     ESP_LOGI(TAG, "uart_recv_task-任务栈剩余空间: %d", watermark);
//         //     // 若watermark < 100字则触发警告
//         //     if (watermark < 100)
//         //     {
//         //         ESP_LOGE(TAG, "uart栈空间不足! 需要扩大任务栈");
//         //     }
//         //     watermark = uxTaskGetStackHighWaterMark(net_task_handle);
//         //     ESP_LOGW(TAG, "net-任务栈剩余空间: %d", watermark);
//         //     if (watermark < 100)
//         //     {
//         //         ESP_LOGW(TAG, "net栈空间不足! 需要扩大任务栈");
//         //     }
//         esp_task_wdt_reset();
//         tds_get();
//         vTaskDelay(pdMS_TO_TICKS(3000));

//         //     // handle_device_auth();
//         // u4g_at_http_request(HTTP_URL, "/api/v1/device/auth?deviceid=&model=CXSZN-WATER&version=1&mccid=898604E41822D0597966", NULL);
//         // u4g_at_http_request(HTTP_URL, "/api/v1/device/CXSZN-WATER/869663078129884", NULL);
//     }
//     //     ESP_LOGI(TAG, "网络初始化完成 执行关闭任务");
//     //     esp_task_wdt_delete(NULL);
// }
