#include "u4g_uart.h"
#include "u4g_state.h" // 状态码
#include "u4g_uart_recv.h"
#include "u4g_at_cmd.h"
#include "u4g_at_buffer.h"
#include "driver/uart.h"       // 引入 ESP-IDF 中 UART 驱动库，用于配置和操作 UART 接口
#include "freertos/FreeRTOS.h" // 引入 FreeRTOS 操作系统的核心库，FreeRTOS 是一个开源的实时操作系统内核，用于任务管理、调度等
#include "freertos/task.h"     // 引入 FreeRTOS 的任务相关库，提供任务创建、删除、挂起等功能
#include "driver/gpio.h"       // 引入 ESP-IDF 中 GPIO 驱动库，用于配置和操作通用输入输出引脚
#include "sdkconfig.h"         // 引入 SDK 配置文件，该文件包含了项目的各种配置选项，如引脚定义、波特率等
#include "freertos/queue.h"
#include "esp_log.h" // 引入 ESP-IDF 的日志库，用于输出调试信息、错误信息等
#include <string.h>
#include "esp_task_wdt.h" // 包含看门狗相关库

// 内部 UART 参数配置
#define U4G_UART_NUM UART_NUM_1
#define U4G_UART_BAUD_RATE 9600 // 波特率
// #define U4G_UART_BAUD_RATE 115200           // 波特率
#define U4G_UART_TX_PIN GPIO_NUM_17         // TX 发送引脚
#define U4G_UART_RX_PIN GPIO_NUM_18         // RX 接收引脚
#define U4G_UART_RTS_PIN UART_PIN_NO_CHANGE // RTS 引脚
#define U4G_UART_CTS_PIN UART_PIN_NO_CHANGE // CTS 引脚
#define U4G_UART_TX_BUF_SIZE (1024)         // TX 发送缓冲区大小
#define U4G_UART_QUEUE_SIZE (10)            // 事件队列数量
#define U4G_UART_RESET_PIN GPIO_NUM_3       // 4G模块硬复位，拉低300ms以上释放则复位重启

static const char *TAG = "4G-UART"; // 定义日志标签，用于在日志输出中标识该模块的信息

/**
 * UART事件队列句柄
 * 其类型是 QueueHandle_t。在 FreeRTOS 中，QueueHandle_t 是用于表示消息队列句柄的数据类型。
 * 消息队列句柄实际上是一个指针，通过它可以对相应的消息队列进行各种操作，比如发送消息、接收消息等，
 * 这里声明的这个变量后续将用来指向创建出来的用于 UART 事件的消息队列
 */
static QueueHandle_t u4g_uart_event_queue = NULL;
// static TaskHandle_t uart_recv_task_handle = NULL; // 接收数据的任务句柄
TaskHandle_t uart_recv_task_handle = NULL;

/**
 * @brief 接收数据处理任务
 * @param arg 任务参数
 */
// static IRAM_ATTR void uart_recv_task(void *arg)
// static void uart_recv_task2(void *arg)
// {
//     uart_event_t event;
//     static uint8_t cache_data[U4G_UART_RX_BUF_SIZE]; // 接收缓冲区
//     memset(cache_data, 0, U4G_UART_RX_BUF_SIZE);
//     while (1)
//     {
//         vTaskDelay(pdMS_TO_TICKS(300));
//         int len = uart_read_bytes(U4G_UART_NUM, cache_data, U4G_UART_RX_BUF_SIZE, pdMS_TO_TICKS(150));
//         /***** 接收到数据 *****/
//         if (len > 0)
//         {
//             // cache_data[len] = '\0';
//             ESP_LOGD(TAG, "<<<UART%d | size:%d | %.*s", U4G_UART_NUM, len, len, cache_data); // 为调试方便，在接收中打印数据，正式生产需删除
//             emU4GResult ret = u4g_uart_recv_process((char *)cache_data, len);
//             if (ret == U4G_STATE_AT_RSP_WAITING)
//             {
//                 ESP_LOGW(TAG, "收到响应，执行继续等待");
//             }
//             memset(cache_data, 0, U4G_UART_RX_BUF_SIZE);
//         }
//         // bool recv_state = true; // true处理接收 false
//         // if (recv_state == true)
//         // {
//         // if (xQueueReceive(u4g_uart_event_queue, (void *)&event, pdMS_TO_TICKS(3000)))
//         // {
//         //     switch (event.type)
//         //     {
//         //     case UART_DATA:
//         //         // vTaskDelay(pdMS_TO_TICKS(150));
//         //         // recv_state = true;
//         //         int len = uart_read_bytes(U4G_UART_NUM, cache_data, U4G_UART_RX_BUF_SIZE, pdMS_TO_TICKS(150));
//         //         /***** 接收到数据 *****/
//         //         if (len > 0)
//         //         {
//         //             cache_data[len] = '\0';
//         //             ESP_LOGD(TAG, "<<<UART%d | size:%d | %.*s", U4G_UART_NUM, len, len, cache_data); // 为调试方便，在接收中打印数据，正式生产需删除
//         //             emU4GResult ret = u4g_uart_recv_process((char *)cache_data, len);
//         //             if (ret == U4G_STATE_AT_RSP_WAITING)
//         //             {
//         //                 ESP_LOGW(TAG, "收到响应，执行继续等待");
//         //             }
//         //             memset(cache_data, 0, U4G_UART_RX_BUF_SIZE);
//         //         }
//         //         break;
//         //     default:
//         //         ESP_LOGW(TAG, "未知的UART事件类型: %d", event.type);
//         //         break;
//         //     }
//         // }
//         // else
//         // {
//         //     ESP_LOGW(TAG, "等待接收数据处理任务超时");
//         //     int len = uart_read_bytes(U4G_UART_NUM, cache_data, U4G_UART_RX_BUF_SIZE, pdMS_TO_TICKS(150));
//         //     /***** 接收到数据 *****/
//         //     if (len > 0)
//         //     {
//         //         cache_data[len] = '\0';
//         //         ESP_LOGD(TAG, "<<<UART%d | size:%d | %.*s", U4G_UART_NUM, len, len, cache_data); // 为调试方便，在接收中打印数据，正式生产需删除
//         //         emU4GResult ret = u4g_uart_recv_process((char *)cache_data, len);
//         //         if (ret == U4G_STATE_AT_RSP_WAITING)
//         //         {
//         //             ESP_LOGW(TAG, "收到响应，执行继续等待");
//         //         }
//         //         memset(cache_data, 0, U4G_UART_RX_BUF_SIZE);
//         //     }
//         // }
//         // }else{

//         // }
//     }
//     // 理论上这里不会执行到，但为了代码完整性保留
//     vTaskDelete(NULL);
// }
static void uart_recv_task(void *arg)
{
    // esp_task_wdt_add(NULL); // 增加任务看门狗保护
    // esp_task_wdt_delete(NULL); // 看门狗-卸载此任务
    uart_event_t event;
    static uint8_t cache_data[U4G_UART_RX_BUF_SIZE]; // 接收缓冲区
    memset(cache_data, 0, U4G_UART_RX_BUF_SIZE);

    while (1)
    {
        // esp_task_wdt_reset(); // 喂狗
        if (xQueueReceive(u4g_uart_event_queue, (void *)&event, (TickType_t)portMAX_DELAY))
        {
            switch (event.type)
            {
            case UART_DATA:
                vTaskDelay(pdMS_TO_TICKS(300));
                // vTaskDelay(pdMS_TO_TICKS(300));
                // uart_get_buffered_data_len(U4G_UART_NUM, &buffered_size); // 获取UART接收数据的长度
                int len = uart_read_bytes(U4G_UART_NUM, cache_data, (U4G_UART_RX_BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
                /***** 接收到数据 *****/
                if (len > 0)
                {
                    cache_data[len] = '\0';
                    ESP_LOGD(TAG, "<<<UART%d | size:%d | %.*s", U4G_UART_NUM, len, len, cache_data); // 为调试方便，在接收中打印数据，正式生产需删除

                    // if (u4g_at_cmd_handle.cmd_content->name == U4G_AT_CMD_HTTP_REQUEST)
                    // {
                    //     ESP_LOGI(TAG, "Recv DATA %s", (char *)UART_RXbuff.Recv);
                    // }
                    // else
                    // {
                    emU4GResult ret = u4g_uart_recv_process((char *)cache_data, len);
                    if (ret == U4G_STATE_AT_RSP_WAITING)
                    {
                        ESP_LOGW(TAG, "收到响应，执行继续等待");
                    }
                    memset(cache_data, 0, U4G_UART_RX_BUF_SIZE);
                    // }
                }
                break;
            case UART_FIFO_OVF:
                ESP_LOGE(TAG, "UART缓冲区溢出或已满，清空输入");
                uart_flush_input(U4G_UART_NUM); // 清空UART输入缓冲区
                xQueueReset(u4g_uart_event_queue);
                vTaskDelay(pdMS_TO_TICKS(100)); // 等待一段时间，防止频繁重置
                break;
            case UART_BUFFER_FULL:
                ESP_LOGE(TAG, "UART ring buffer full");
                uart_flush_input(U4G_UART_NUM); // 清空UART输入缓冲区
                xQueueReset(u4g_uart_event_queue);
                vTaskDelay(pdMS_TO_TICKS(100)); // 等待一段时间，防止频繁重置
                break;
            case UART_BREAK:
                ESP_LOGE(TAG, "接收到UART事件-通信中断");
                // 错误处理逻辑，可以尝试重新初始化UART或其他恢复措施
                // uart_driver_delete(U4G_UART_NUM);                                                                                                              // 重新删除UART驱动
                // uart_driver_install(U4G_UART_NUM, U4G_UART_RX_BUF_SIZE, U4G_UART_TX_BUF_SIZE, U4G_UART_QUEUE_SIZE, &u4g_uart_event_queue, ESP_INTR_FLAG_IRAM); // 重新安装UART驱动
                break;
            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "接收到UART事件-UART_PARITY_ERR");
                break;
            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "接收到UART事件-UART_FRAME_ERR");
                break;
            case UART_PATTERN_DET:
                ESP_LOGE(TAG, "接收到UART事件-UART_PATTERN_DET");
                break;
            default:
                ESP_LOGW(TAG, "未知的UART事件类型: %d", event.type);
                break;
            }
        }
    }
    // 理论上这里不会执行到，但为了代码完整性保留
    vTaskDelete(NULL);
}

/**
 * @brief UART初始化函数
 * @return esp_err_t
 */
esp_err_t u4g_uart_init(void)
{
    ESP_LOGI(TAG, "启动 UART 初始化");

    // 初始化4G复位针脚
    gpio_config_t io_conf_input = {
        .pin_bit_mask = (1ULL << U4G_UART_RESET_PIN),
        .mode = GPIO_MODE_OUTPUT,              // 输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,     // 禁用内部上拉电阻
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用内部下拉电阻
        .intr_type = GPIO_INTR_DISABLE};
    gpio_config(&io_conf_input);
    u4g_uart_reset();

    const uart_config_t uart_config = {
        .baud_rate = U4G_UART_BAUD_RATE,       // 设置 UART 的波特率
        .data_bits = UART_DATA_8_BITS,         // 数据位
        .parity = UART_PARITY_DISABLE,         // 校验位
        .stop_bits = UART_STOP_BITS_1,         // 停止位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, // 禁用硬件流控制
        .source_clk = UART_SCLK_DEFAULT,       // 明确时钟源
    };
    int intr_alloc_flags = 0; // 定义中断分配标志，初始化为 0

    // 如果配置了 UART 中断服务程序在 IRAM 中运行
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM; // 设置中断分配标志为在 IRAM 中分配中断
#endif

    // 配置 UART 参数
    esp_err_t err = uart_param_config(U4G_UART_NUM, &uart_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "UART 设置参数失败: %s", esp_err_to_name(err));
        return err;
    }
    // 设置 UART 引脚
    err = uart_set_pin(U4G_UART_NUM, U4G_UART_TX_PIN, U4G_UART_RX_PIN, U4G_UART_RTS_PIN, U4G_UART_CTS_PIN);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "UART 设置引脚失败: %s", esp_err_to_name(err));
        return err;
    }
    // 安装 UART 驱动并创建事件队列
    err = uart_driver_install(U4G_UART_NUM, U4G_UART_RX_BUF_SIZE * 2, U4G_UART_TX_BUF_SIZE, U4G_UART_QUEUE_SIZE, &u4g_uart_event_queue, intr_alloc_flags);
    // err = uart_driver_install(U4G_UART_NUM, U4G_UART_RX_BUF_SIZE, U4G_UART_TX_BUF_SIZE, U4G_UART_QUEUE_SIZE, &u4g_uart_event_queue, intr_alloc_flags);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "安装 UART 驱动失败: %s", esp_err_to_name(err));
        return err;
    }
    if (u4g_uart_event_queue == NULL)
    {
        ESP_LOGE(TAG, "事件队列创建失败");
        return ESP_FAIL;
    }

    // 配置 DMA 缓冲区（必须 4 字节对齐）
    // uart_set_rx_full_threshold(U4G_UART_NUM, 128);
    // uart_set_tx_empty_threshold(U4G_UART_NUM, 16);
    // uart_set_rx_timeout(U4G_UART_NUM, 10); // 缩短超时时间

    // 初始化缓冲区管理
    u4g_at_buffer_init();
    if (xTaskCreatePinnedToCore(
            uart_recv_task,         // 任务函数
            "uart_recv_task",       // 任务名称
            4096,                   // 栈大小
            NULL,                   // 参数
            10,                     // 优先级（数值越低优先级越低）
            &uart_recv_task_handle, // 任务句柄
            1                       // 核心编号（0=核心0，1=核心1）
            ) != pdPASS)
    {
        ESP_LOGE(TAG, "创建 uart_recv_task 失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "UART%d 初始化成功丨波特率: %d | TX引脚%d | RX引脚%d | TX缓冲区:%d | RX(接收)缓冲区:%d | 队列数量%d",
             U4G_UART_NUM, U4G_UART_BAUD_RATE, U4G_UART_TX_PIN, U4G_UART_RX_PIN, U4G_UART_TX_BUF_SIZE, U4G_UART_RX_BUF_SIZE, U4G_UART_QUEUE_SIZE);
    return ESP_OK;
}

// 重启4G模块
void u4g_uart_reset(void)
{
    gpio_set_level(U4G_UART_RESET_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(U4G_UART_RESET_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(2000)); // 延迟
}

/**
 * @brief 发送数据到UART
 * @param p_data 发送数据的指针
 * @param len   发送数据的长度
 * @return 发送结果，成功返回发送的字节数，否则返回错误码
 */
int32_t u4g_uart_send(const char *p_data, uint16_t len)
{
    if (p_data == NULL)
    {
        ESP_LOGE(TAG, "发送数据指针为空");
        return U4G_ERR_INVALID_ARG;
    }

    if (len > U4G_UART_TX_BUF_SIZE)
    {
        ESP_LOGE(TAG, "数据长度超过UART%d TX缓冲区大小: %d > %d", U4G_UART_NUM, len, U4G_UART_TX_BUF_SIZE);
        return U4G_ERR_INVALID_ARG; // 返回错误码
    }
    ESP_LOGI(TAG, ">>> 发送数据长度: %d | 数据: %.*s", len, len, p_data);
    // esp_task_wdt_reset(); // 喂狗
    int bytes_written = uart_write_bytes(U4G_UART_NUM, p_data, len);
    // int bytes_written = uart_write_bytes(U4G_UART_NUM, (const char *)p_data, len);
    if (bytes_written < 0)
    {
        ESP_LOGE(TAG, "向UART%d发送数据失败 错误码: %d", U4G_UART_NUM, bytes_written);
        return U4G_STATE_AT_UART_TX_FAILED; // 返回发送失败错误码
    }
    // esp_task_wdt_reset(); // 喂狗
    // 确保数据发送完毕
    esp_err_t ret = uart_wait_tx_done(U4G_UART_NUM, portMAX_DELAY);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "数据发送成功，发送字节数: %d", bytes_written);
        return len; // 成功返回发送的字节数
    }
    else if (ret == ESP_ERR_TIMEOUT)
    {
        ESP_LOGE(TAG, "UART%d发送数据超时", U4G_UART_NUM);
        return U4G_STATE_AT_UART_TX_TIMEOUT;
    }
    else
    {
        ESP_LOGE(TAG, "UART%d发送数据失败，错误码: %s", U4G_UART_NUM, esp_err_to_name(ret));
        return U4G_STATE_AT_UART_TX_FAILED;
    }
}

/**
 * @brief 4G-UART 驱动删除
 */
void u4g_uart_deinit(void)
{
    esp_err_t err = uart_driver_delete(U4G_UART_NUM);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "UART%d 驱动已删除", U4G_UART_NUM);
    }
    else
    {
        ESP_LOGE(TAG, "删除UART%d驱动失败: %s", U4G_UART_NUM, esp_err_to_name(err));
    }
}