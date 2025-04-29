#include "gpio_water.h"
#include "head.h"
#include "gpio_flush.h" // 引入冲洗类
#include "a_led_event.h"
#include "gpio_water_timer.h" // 引入制水时间类
#include "gpio_buzzer.h"      // 蜂鸣器类
#include "a_feedback.h"       // 设备反馈
// #include "freertos/FreeRTOS.h"
#include "esp_attr.h" // 包含 IRAM_ATTR 的定义
#include "esp_log.h"

#define TAG "GPIO-WATER"

// 状态定义
typedef enum
{
    STATE_IDLE = 0, // 空闲
    STATE_WATER,    // 制水状态
    STATE_HIGH,     // 高压状态
    STATE_LOW,      // 低压状态
    STATE_LEAK,     // 漏水状态
    STATE_FAULT     // 故障状态
} water_state_t;
static water_state_t old_state = STATE_IDLE; // 旧状态-初始化为空闲

// 全局变量
TaskHandle_t xTaskHandle_water = NULL;

// 内部声明
static void gpio_water_task(void *arg);
static void handle_state_change(water_state_t state);
static void water_task_loop(void *arg);

// ISR处理程序
static void IRAM_ATTR gpio_isr_handler_water(void *arg)
{
    // 发送通知给任务
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTaskNotifyFromISR(xTaskHandle_water, 0, eNoAction, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}
esp_err_t gpio_water_init(void)
{
    ESP_LOGI(TAG, "开始执行GPIO初始化");
    esp_err_t ret;
    // 配置输出GPIO
    gpio_config_t io_conf_output = {
        .pin_bit_mask = (1ULL << CONFIG_WATER_GPIO_NUM)   // 制水接口
                        | (1ULL << CONFIG_FLUSH_GPIO_NUM) // 冲洗接口
                                                          // | (1ULL << CONFIG_BUZZER_GPIO_NUM) // 蜂鸣器接口
        ,                                                 // 选择要配置的引脚
        .mode = GPIO_MODE_OUTPUT,                         // 将GPIO引脚配置为输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,                // 禁用上拉电阻
        .pull_down_en = GPIO_PULLDOWN_DISABLE,            // 禁用下拉电阻
        .intr_type = GPIO_INTR_DISABLE                    // 禁用GPIO中断
    };
    ret = gpio_config(&io_conf_output);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "输出 GPIO 配置失败: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    // 由于外部连接的是下拉电阻，故不用初始化针脚电平

    // 配置输入GPIO（高压、低压、漏水）
    gpio_config_t io_conf_input = {
        .pin_bit_mask = (1ULL << CONFIG_HIGH_GPIO_NUM) | (1ULL << CONFIG_LOW_GPIO_NUM) | (1ULL << CONFIG_LEAK_GPIO_NUM),
        // .pin_bit_mask = (1ULL << CONFIG_HIGH_GPIO_NUM) | (1ULL << CONFIG_LOW_GPIO_NUM),
        .mode = GPIO_MODE_INPUT,               // 输入模式
        .pull_up_en = GPIO_PULLUP_DISABLE,     // 禁用上拉电阻
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用下拉电阻
        .intr_type = GPIO_INTR_ANYEDGE         // GPIO中断类型：上升沿和下降沿
        // .intr_type = GPIO_INTR_NEGEDGE         // 下降沿中断
    };
    ret = gpio_config(&io_conf_input);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "输入 GPIO 配置失败: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }
    // gpio_config_t io_conf_input2 = {
    //     .pin_bit_mask = (1ULL << CONFIG_LEAK_GPIO_NUM),
    //     .mode = GPIO_MODE_INPUT,               // 输入模式
    //     .pull_up_en = GPIO_PULLUP_ENABLE,     // 禁用上拉电阻
    //     .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用下拉电阻
    //     .intr_type = GPIO_INTR_ANYEDGE         // GPIO中断类型：上升沿和下降沿
    //     // .intr_type = GPIO_INTR_NEGEDGE         // 下降沿中断
    // };
    // ret = gpio_config(&io_conf_input2);
    // if (ret != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "输入 GPIO 配置失败: %s", esp_err_to_name(ret));
    //     return ESP_FAIL;
    // }

    // 初始化冲洗逻辑
    ret = gpio_flush_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "冲洗定时器初始化失败: %s", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    // 创建事件处理任务
    // if (xTaskCreate(gpio_water_task, "gpio_water_task", 6000, NULL, 11, &xTaskHandle_water) != pdPASS)
    // {
    //     ESP_LOGE(TAG, "创建 gpio_water_task 任务失败");
    //     return ESP_FAIL;
    // }
    xTaskCreatePinnedToCore(
        gpio_water_task,    // 任务函数
        "gpio_water_task",  // 任务名称
        8190,               // 栈大小
        NULL,               // 参数
        9,                  // 优先级（数值越低优先级越低）
        &xTaskHandle_water, // 任务句柄
        0                   // 核心编号（0=核心0，1=核心1）
    );

    // 安装GPIO中断服务
    if (!is_isr_service_installed)
    {
        if (gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1) != ESP_OK)
        {
            ESP_LOGE(TAG, "安装GPIO中断服务失败");
            return ESP_FAIL;
        }
        is_isr_service_installed = true;
    }
    gpio_isr_handler_add(CONFIG_HIGH_GPIO_NUM, gpio_isr_handler_water, NULL);
    gpio_isr_handler_add(CONFIG_LOW_GPIO_NUM, gpio_isr_handler_water, NULL);
    gpio_isr_handler_add(CONFIG_LEAK_GPIO_NUM, gpio_isr_handler_water, NULL);

    // xTaskNotifyGive(xTaskHandle_water); // 初始化一次通知校准制水状态

    // if (xTaskCreate(water_task_loop, "water_task_loop", 4096, NULL, 8, NULL) != pdPASS)
    // {
    //     ESP_LOGE(TAG, "创建 water_task_loop 任务失败");
    //     return ESP_FAIL;
    // }
    if (xTaskCreatePinnedToCore(
            water_task_loop,   // 任务函数
            "water_task_loop", // 任务名称
            4096,              // 栈大小
            NULL,              // 参数
            8,                 // 优先级（数值越低优先级越低）
            NULL,              // 任务句柄
            0                  // 核心编号（0=核心0，1=核心1）
            ) != pdPASS)
    {
        ESP_LOGE(TAG, "创建 water_task_loop 任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "执行GPIO初始化完成");
    return ESP_OK;
}

// 事件处理任务
static void gpio_water_task(void *arg)
{
    a_led_event_t event;
    while (1)
    {
        // 等待通知
        if (xTaskNotifyWait(0x00, 0xFFFFFFFF, NULL, portMAX_DELAY) == pdTRUE)
        {
            // 识别状态变化
            water_state_t new_state = STATE_WATER; // 默认状态为制水状态
            // 漏水状态
            if (gpio_get_level(CONFIG_LEAK_GPIO_NUM) == 0) // 电平漏水
            {
                event.type = LED_WATER_LEAK;
                event.data.led_water_leak = true;
                gpio_buzzer_output(true); // 蜂鸣器开启
                new_state = STATE_LEAK;   // 更新为漏水状态
                ESP_LOGI(TAG, "当前制水状态为：漏水");
                DEVICE.WATER_LEAK = true;
                if (DEVICE_STATUS.feedback_init)
                {
                    xTaskNotifyGive(xTaskHandle_feedback); // 触发通知设备反馈
                }
            }
            else
            {
                event.type = LED_WATER_LEAK;
                event.data.led_water_leak = false;
                gpio_buzzer_output(false); // 蜂鸣器关闭
                DEVICE.WATER_LEAK = false;
            }
            xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));

            // 低压状态
            if (gpio_get_level(CONFIG_LOW_GPIO_NUM) == 1)
            {
                event.type = LED_WATER_LOW;
                event.data.led_water_low = true;

                new_state = STATE_LOW; //  更新为低压状态
                ESP_LOGI(TAG, "当前制水状态为：低压");
            }
            else
            {
                event.type = LED_WATER_LOW;
                event.data.led_water_low = false;
            }
            xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));

            // 高压状态
            if (gpio_get_level(CONFIG_HIGH_GPIO_NUM) == 1)
            {
                event.type = LED_WATER_HIGH;
                event.data.led_water_high = true;

                new_state = STATE_HIGH; //  更新为高压状态
                ESP_LOGI(TAG, "当前制水状态为：高压");
            }
            else
            {
                event.type = LED_WATER_HIGH;
                event.data.led_water_high = false;
            }
            xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));

            // 比较新状态与上一个状态
            if (new_state != old_state)
            {
                ESP_LOGI(TAG, "制水传感器状态变化：%d -> %d", old_state, new_state);
                handle_state_change(new_state); // 处理状态变化
                old_state = new_state;          // 更新上一个状态
            }
            else
            {
                ESP_LOGI(TAG, "制水传感器状态未变化 (%d)，无需处理", new_state);
            }
        }
    }
}

static void water_task_loop(void *arg)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
        if (xTaskHandle_water == NULL)
        {
            ESP_LOGD(TAG, "循环制水通知-报错检测到xTaskHandle_water为NULL");
        }
        else
        {
            ESP_LOGD(TAG, "循环制水通知 !");
            xTaskNotifyGive(xTaskHandle_water);
        }
    }
    vTaskDelete(NULL);
}

// 状态处理函数
static void handle_state_change(water_state_t state)
{
    a_led_event_t event;

    if (DEVICE.MODE_EXPIRY == EXPIRY)
    {
        ESP_LOGI(TAG, "设备已到期，停止制水流程");
        gpio_set_level(CONFIG_WATER_GPIO_NUM, 0); // 关闭水泵
        event.type = LED_WATER;
        event.data.led_water = false;                              // 制水灯关闭
        xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100)); // 通知制水灯任务
        gpio_flush_process(0);                                     // 停止反冲洗
        return;
    }

    // 处理水泵控制
    event.type = LED_WATER;
    if (state == STATE_WATER)
    {
        ESP_LOGI(TAG, "当前制水状态为：制水");
        event.data.led_water = true;              // 制水灯点亮
        gpio_set_level(CONFIG_WATER_GPIO_NUM, 1); // 开启水泵
        gpio_water_production_time_set(true);     // 开始制水计时-冲洗定时
    }
    else
    {
        ESP_LOGI(TAG, "重置结束制水状态");
        event.data.led_water = false;             // 制水灯关闭
        gpio_set_level(CONFIG_WATER_GPIO_NUM, 0); // 关闭水泵
        gpio_water_production_time_set(false);    // 停止制水计时
    }
    xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100)); // 通知制水灯任务

    // 处理冲洗逻辑
    if (state != STATE_LOW && old_state == STATE_LOW) // 状态退出低压
    {
        gpio_flush_process(FLUSH_TIME.LOW_END);
    }
    else
    {
        if (state == STATE_HIGH) // 状态进入高压
        {
            gpio_flush_process(FLUSH_TIME.HIGH_START);
        }
        else if (state != STATE_HIGH && old_state == STATE_HIGH) // 状态退出高压
        {
            gpio_flush_process(FLUSH_TIME.HIGH_END);
        }
    }
}