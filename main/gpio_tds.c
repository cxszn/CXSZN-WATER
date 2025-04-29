#include "gpio_tds.h"
#include "head.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/uart.h" // 包含 UART 驱动头文件
#include "esp_log.h"
#include <string.h>

#include "a_led_event.h"

#define TAG "GPIO-TDS"

QueueHandle_t uart2_event_queue = NULL;

/* UART 事件任务句柄 */
static TaskHandle_t tds_uart_event_task_handle = NULL; // UART事件任务句柄，可实现挂起(vTaskSuspend())、恢复(vTaskResume())、删除(vTaskDelete,并重置NULL)等

/* 接收缓冲区结构体 */
#define RXBUFF_MAX_SIZE 512
typedef struct
{
    char Recv[RXBUFF_MAX_SIZE]; // 接收缓冲区
    size_t rx_len;              // 接收缓冲区长度
} stcUARTRxBuff;
static stcUARTRxBuff UART_RXbuff;

static void tds_uart_event_task(void *pvParameters);
static void parse_response(const char *response, size_t len);

esp_err_t gpio_tds_init(void)
{
    ESP_LOGI(TAG, "开始UART初始化");
    uart_config_t uart_config = {
        .baud_rate = CONFIG_TDS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS, // 数据位
        .parity = UART_PARITY_DISABLE, // 校验位
        .stop_bits = UART_STOP_BITS_1, // 停止位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        // .source_clk = UART_SCLK_APB,
    };
    // 配置 UART 参数
    esp_err_t err = uart_param_config(CONFIG_TDS_UART_NUM, &uart_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "UART 设置参数失败: %s", esp_err_to_name(err));
        return err;
    }

    // 设置 UART 引脚
    err = uart_set_pin(CONFIG_TDS_UART_NUM, CONFIG_TDS_UART_TX_PIN, CONFIG_TDS_UART_RX_PIN, CONFIG_TDS_UART_RTS_PIN, CONFIG_TDS_UART_CTS_PIN);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "UART 设置引脚失败: %s", esp_err_to_name(err));
        return err;
    }

    // 安装 UART 驱动并创建事件队列
    err = uart_driver_install(CONFIG_TDS_UART_NUM, CONFIG_TDS_UART_RX_BUF_SIZE, CONFIG_TDS_UART_TX_BUF_SIZE, CONFIG_TDS_UART_QUEUE_SIZE, &uart2_event_queue, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "安装 UART 驱动失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 创建 UART 事件处理任务 */
    // if (xTaskCreate(tds_uart_event_task, "tds_uart_event_task", 3096, NULL, 2, &tds_uart_event_task_handle) != pdPASS)
    // {
    //     ESP_LOGE(TAG, "创建 UART 事件任务失败");
    //     return ESP_FAIL;
    // }
    if (xTaskCreatePinnedToCore(
            tds_uart_event_task,         // 任务函数
            "tds_uart_event_task",       // 任务名称
            3096,                        // 栈大小
            NULL,                        // 参数
            2,                           // 优先级（数值越低优先级越低）
            &tds_uart_event_task_handle, // 任务句柄
            1                            // 核心编号（0=核心0，1=核心1）
            ) != pdPASS)
    {
        ESP_LOGE(TAG, "创建 UART 事件任务失败");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "UART%d 初始化成功丨波特率: %d | TX缓冲区: %d | RX缓冲区: %d", CONFIG_TDS_UART_NUM, CONFIG_TDS_UART_BAUD_RATE, CONFIG_TDS_UART_RX_BUF_SIZE, CONFIG_TDS_UART_TX_BUF_SIZE);

    // 识别TDS通道
    for (size_t i = 0; i < 2; i++)
    {
        tds_get();
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
    return ESP_OK;
}

void tds_get(void)
{
    // 发送检测指令
    const uint8_t check_cmd[] = {0xA0, 0x00, 0x00, 0x00, 0x00, 0xA0}; // 检测指令
    uart_write_bytes(CONFIG_TDS_UART_NUM, (const char *)check_cmd, sizeof(check_cmd));
}

// TDS任务处理
static void tds_uart_event_task(void *pvParameters)
{
    uart_event_t event;
    while (1)
    {
        // if (xQueueReceive(uart2_event_queue, (void *)&event, portMAX_DELAY))
        if (xQueueReceive(uart2_event_queue, (void *)&event, pdMS_TO_TICKS(5000)))
        {
            switch (event.type)
            {
            case UART_DATA:
            {
                memset(UART_RXbuff.Recv, 0, RXBUFF_MAX_SIZE);
                UART_RXbuff.rx_len = RXBUFF_MAX_SIZE;
                int read_len = uart_read_bytes(CONFIG_TDS_UART_NUM, (uint8_t *)UART_RXbuff.Recv, RXBUFF_MAX_SIZE, pdMS_TO_TICKS(250));
                if (read_len > 0)
                {
                    UART_RXbuff.rx_len = read_len;
                    // ESP_LOGD(TAG, "接收UART响应: %.*s", read_len, UART_RXbuff.Recv);
                    // ESP_LOGD(TAG, "接收UART响应长度: %d", read_len);
                    // // 打印二进制数据 (可选)
                    // ESP_LOGI(TAG, "TDS二进制数据内容: ");
                    // for (size_t i = 0; i < read_len; i++)
                    // {
                    //     printf("%02X ", UART_RXbuff.Recv[i]);
                    //     if ((i + 1) % 16 == 0)
                    //         printf("\n");
                    // }
                    // printf("\n");
                    parse_response(UART_RXbuff.Recv, read_len);
                }
                break;
            }
            default:
            {
                ESP_LOGW(TAG, "收到未处理的 TDS-UART 事件类型: %d", event.type);
                break;
            }
            }
        }
        else
        {
            tds_get();
        }
    }
    vTaskDelete(NULL);
}

/**
 * 计算效验和
 * start_size 开始位
 */
static esp_err_t tds_Validation(const char *data, size_t len, size_t start_size)
{
    // 检查 start_size 是否超出数据长度
    if (start_size >= len)
    {
        return ESP_ERR_INVALID_ARG; // 超出数据长度
    }

    // 计算校验和
    uint8_t checksum = 0;
    for (size_t i = start_size; i < len - 1; i++) // 计算前面所有字节的和，不包括最后一个字节
    {
        checksum += data[i];
        // ESP_LOGI(TAG, "TDS二进制数据内容: %02X", checksum); // 打印二进制数据 (可选)
    }

    // 校验和验证
    if (checksum != data[len - 1]) // 比较计算得到的校验和与响应中的校验和
    {
        ESP_LOGE(TAG, "校验和错误: 计算值: %02X, 接收到的值: %02X", checksum, (uint8_t)data[len - 1]);
        return ESP_ERR_INVALID_CRC; // 校验和不匹配，返回
    }
    return ESP_OK;
}

uint8_t pure_tds_old = 0; // 纯水TDS旧状态
uint8_t raw_tds_old = 0;  // 原水TDS旧状态

static void parse_response(const char *response, size_t len)
{
    // 打印二进制数据 (可选)
    // ESP_LOGI(TAG, "TDS二进制数据内容: ");
    // for (size_t i = 0; i < len; i++)
    // {
    //     printf("%02X ", response[i]);
    //     if ((i + 1) % 16 == 0)
    //         printf("\n");
    // }
    // printf("\n");

    esp_err_t ret;
    a_led_event_t event;
    // 检查返回的响应长度
    if (len == 12) // TDS双通道
    {
        ret = tds_Validation(response, len, 6); // 计算AA效验和
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "TDS双通道效验错误");
            return;
        }

        // AA 00 23 00 28 F5 AB 09 C4 09 C4 45  亲自测试反馈的数值
        // 2 个通道温度值：AB 0A 5D 0A 96 B2
        // 通道 1 温度值：0A 5D = 0x0A5D/100 = 26.53
        // 通道 2 温度值：0A 96 = 0x0A96/100 = 27.1
        DEVICE_TDSWD.pure_tds = ((uint8_t)response[1] << 8) | (uint8_t)response[2];
        DEVICE_TDSWD.raw_tds = ((uint8_t)response[3] << 8) | (uint8_t)response[4];
        ESP_LOGI(TAG, "双通道 纯水TDS 值: %d, 原水TDS 值: %d", DEVICE_TDSWD.pure_tds, DEVICE_TDSWD.raw_tds);

        uint16_t value_3 = ((uint8_t)response[7] << 8) | (uint8_t)response[8];
        uint16_t value_4 = ((uint8_t)response[9] << 8) | (uint8_t)response[10];
        DEVICE_TDSWD.pure_temperature = (float)value_3 / 100.0; // 转换为摄氏度
        DEVICE_TDSWD.raw_temperature = (float)value_4 / 100.0;  // 转换为摄氏度
        ESP_LOGI(TAG, "双通道 温度1 值: %.2f, 温度2 值: %.2f", DEVICE_TDSWD.pure_temperature, DEVICE_TDSWD.raw_temperature);

        if (DEVICE_TDSWD.pure_tds != pure_tds_old)
        {
            event.type = LED_TDS_PURE;
            event.data.led_tds_pure = DEVICE_TDSWD.pure_tds;
            xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));
            pure_tds_old = DEVICE_TDSWD.pure_tds;
        }

        if (DEVICE_TDSWD.raw_tds != raw_tds_old)
        {
            event.type = LED_TDS_RAW;
            event.data.led_tds_raw = DEVICE_TDSWD.raw_tds;
            xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));
            raw_tds_old = DEVICE_TDSWD.raw_tds;
        }
    }
    else if (len == 6) // TDS单通道
    {
        ret = tds_Validation(response, len, 0); // 计算AA效验和
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "TDS单通道效验错误");
            return;
        }

        /**
         * 单通道 TDS 值和温度值：AA 00 64 0A 96 40
         * -TDS 值： 00 64 = 0x0064
         * -温度值：0A 96 = 0x0A96/100 = 27.1
         * 2 个通道 TDS 值：AA 00 64 00 32 40
         * -通道 1 TDS 值：00 64 = 0x0064
         * 通道 2 TDS 值：00 32 = 0x0032
         */
        DEVICE_TDSWD.pure_tds = ((uint8_t)response[1] << 8) | (uint8_t)response[2];

        uint16_t value_1 = ((uint8_t)response[3] << 8) | (uint8_t)response[4];
        DEVICE_TDSWD.pure_temperature = (float)value_1 / 100.0; // 转换为摄氏度
        ESP_LOGI(TAG, "单通道 纯水TDS 值: %d, 温度: %.2f", DEVICE_TDSWD.pure_tds, DEVICE_TDSWD.pure_temperature);

        if (DEVICE_TDSWD.pure_tds != pure_tds_old)
        {
            event.type = LED_TDS_PURE;
            event.data.led_tds_pure = DEVICE_TDSWD.pure_tds;
            xQueueSend(a_led_event_queue, &event, pdMS_TO_TICKS(100));
            pure_tds_old = DEVICE_TDSWD.pure_tds;
        }
    }
    else
    {
        ESP_LOGE(TAG, "响应长度 %d 错误", len);
    }
}