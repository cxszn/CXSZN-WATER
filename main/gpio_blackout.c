/*
 * @function: 断电保存任务类
 * @Description: 检测电压低于阈值时，保存当前时间到 NVS
 * @Author: 初馨顺之诺
 * @contact: 微信：cxszn01
 * @link: shop.cxszn.com
 * @Date: 2024-12-17 11:04:37
 * @LastEditTime: 2024-12-23 15:43:09
 * @FilePath: \CXSZN-WATER\main\gpio_blackout.c
 * Copyright (c) 2024 by 临沂初馨顺之诺网络科技中心, All Rights Reserved.
 */
#include "gpio_blackout.h"
#include "head.h"
#include "driver/gpio.h"
#include "esp_attr.h"    // 包含 IRAM_ATTR 的定义
#include "a_nvs_flash.h" // nvs_flash应用类
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "a_time.h"

static const char *TAG = "GPIO-BLACKOUT";

static bool is_blackout_exist = false; // 是否执行过断电保存事项

// NVS 存储任务
static void app_nvs_flash_insert_task(void *arg)
{
    ESP_LOGI(TAG, "执行处理断电事件...");
    if (is_blackout_exist == true)
    {
        ESP_LOGW(TAG, "已执行过断电事项，已忽略");
    }
    else
    {
        a_time_save(); // 执行保存当前时间
    }
    vTaskDelete(NULL); // 删除当前任务
}

// 中断服务程序
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    // 创建一个任务来处理 NVS 存储操作
    xTaskCreatePinnedToCore(
        app_nvs_flash_insert_task, // 任务函数
        "BLACKOUT_Insert_Task",    // 任务名称
        3098,                      // 堆栈大小
        NULL,                      // 任务参数
        23,                        // 任务优先级
        NULL,                      // 任务句柄
        1);                        // 核心ID
}

/**
 * @brief 初始化断电检测模块
 * @return esp_err_t ESP_OK 表示成功，其他值表示失败
 */
esp_err_t gpio_blackout_init(void)
{
    // 配置断电检测接口 GPIO
    static gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CONFIG_BLACKOUT_GPIO_NUM), // 指定要配置的引脚，通过位掩码选择GPIO引脚
        .mode = GPIO_MODE_INPUT,                            // 将GPIO引脚配置为输入模式
        // .pull_up_en = GPIO_PULLUP_ENABLE,         // 启用上拉电阻，以确保在引脚未连接时保持高电平
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用下拉电阻
        // .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE // 下降沿触发中断
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "断电检测接口 GPIO 配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    // 安装GPIO中断服务程序
    if (!is_isr_service_installed)
    {
        ret = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "安装GPIO中断服务失败: %s", esp_err_to_name(ret));
            return ret;
        }
        is_isr_service_installed = true;
    }

    // 将中断服务程序与GPIO引脚关联
    ret = gpio_isr_handler_add(CONFIG_BLACKOUT_GPIO_NUM, gpio_isr_handler, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "添加断电检测中断处理程序失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "系统初始化完成，等待断电事件...");
    return ESP_OK;
}