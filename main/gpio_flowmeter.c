/**
 * gpio_flowmeter.c
 * 流量计接口类.
 */
#include "gpio_flowmeter.h" // 头文件，用于获取函数声明
#include "head.h"
#include "driver/gpio.h" //GPIO驱动
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h" // 包含 portMUX_TYPE 的定义
#include "esp_log.h"            //包含ESP-IDF的日志功能，用于输出调试信息
#include "esp_attr.h"           // 包含 IRAM_ATTR 的定义

#define PULSES_PER_LITER 1260 // 每升对应的脉冲数

static const char *TAG = "FLOWMETER";
// 脉冲计数变量，需要在中断服务程序中修改，使用原子操作或临界区保护
// static volatile uint32_t pulse_count = 0;
// 定义一个 portMUX_TYPE 用于临界区保护
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * @brief 脉冲去抖动
 * @param arg 中断处理程序的参数
 * 如果流量计信号存在抖动，建议在硬件上添加去抖动电路（如 RC 低通滤波器），或者在软件中实现去抖动逻辑
 */
static void IRAM_ATTR flowmeter_isr_handler(void *arg)
{
    static uint32_t last_pulse_time = 0;
    uint32_t current_time = xTaskGetTickCountFromISR();

    // 定义最小间隔为10ms
    if ((current_time - last_pulse_time) > (10 / portTICK_PERIOD_MS))
    {
        portENTER_CRITICAL_ISR(&mux);
        DEVICE.flowmeter++;
        portEXIT_CRITICAL_ISR(&mux);
        last_pulse_time = current_time;
    }
}
/**
 * 脉冲去抖动
 * 增加记录脉冲时间戳，忽略间隔过短的脉冲以去抖动
 * 如果流量计信号存在抖动，建议在硬件上添加去抖动电路（如 RC 低通滤波器），或者在软件中实现去抖动逻辑
 */
// static void IRAM_ATTR flowmeter_isr_handler(void *arg)
// {
//     static portMUX_TYPE mux_isr = portMUX_INITIALIZER_UNLOCKED;
//     static uint32_t last_pulse_time = 0;
//     uint32_t current_time = xTaskGetTickCountFromISR();
//     // 定义最小间隔为10ms
//     if ((current_time - last_pulse_time) > (10 / portTICK_PERIOD_MS))
//     {
//         portENTER_CRITICAL_ISR(&mux_isr);
//         pulse_count++;
//         portEXIT_CRITICAL_ISR(&mux_isr);
//         last_pulse_time = current_time;
//     }
// }

/**
 * @brief 初始化流量计接口 GPIO
 * @return esp_err_t ESP_OK 表示成功，其他值表示失败
 */
esp_err_t gpio_flowmeter_init(void)
{
    ESP_LOGI(TAG, "初始化流量计接口-开始");
    // 配置GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE, // 上升沿中断
        .mode = GPIO_MODE_INPUT,        // 输入模式
        .pin_bit_mask = (1ULL << CONFIG_FLOWMETER_GPIO_NUM),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        // .pull_up_en = GPIO_PULLUP_DISABLE
        .pull_up_en = GPIO_PULLUP_ENABLE, // 启用上拉
        // .glitch_filter_en = true          // 启用硬件滤波（ESP32-S3 特有）
    };
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "流量计 GPIO 配置失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 安装GPIO中断服务
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
    ret = gpio_isr_handler_add(CONFIG_FLOWMETER_GPIO_NUM, flowmeter_isr_handler, NULL);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "添加流量计中断处理程序失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "流量计 GPIO 初始化成功");
    return ESP_OK;
}

// 获取并重置当前的脉冲计数
uint32_t gpio_flowmeter_get_pulse_count(void)
{
    portENTER_CRITICAL(&mux);
    uint32_t count = DEVICE.flowmeter;
    // pulse_count = 0;
    portEXIT_CRITICAL(&mux);
    return count;
}

// 重置脉冲计数
void gpio_flowmeter_reset_count(void)
{
    portENTER_CRITICAL(&mux);
    DEVICE.flowmeter = 0;
    portEXIT_CRITICAL(&mux);
}

// 获取当前的水量（升）
// float gpio_flowmeter_get_liters(void)
// {
//     uint32_t count = gpio_flowmeter_get_pulse_count();
//     float liters = (float)count / PULSES_PER_LITER;
//     return liters;
// }