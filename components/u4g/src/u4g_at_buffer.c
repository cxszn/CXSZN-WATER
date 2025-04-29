#include "u4g_at_buffer.h"
#include "esp_log.h"
#include <string.h>

#define BUFFER_TAG "AT_BUF"

// 全局缓冲区实例
static u4g_at_cache_t AT_Cache = {
    .len = {0},
    .state = {BUFFER_FREE, BUFFER_FREE}}; // 初始化为FREE状态

// 信号量
SemaphoreHandle_t u4g_at_buffer_mutex = NULL;
static SemaphoreHandle_t buffer_available_sem = NULL;
// QueueHandle_t u4g_data_queue = NULL; // 全局数据队列句柄

/**
 * @brief 初始化缓冲区管理
 */
esp_err_t u4g_at_buffer_init(void)
{
    // 创建缓冲区互斥锁
    u4g_at_buffer_mutex = xSemaphoreCreateMutex();
    if (u4g_at_buffer_mutex == NULL)
    {
        ESP_LOGE(BUFFER_TAG, "无法创建缓冲区互斥锁，系统可能无法正常工作");
        // 根据需求，可以在此处添加系统重启或进入安全模式的代码
        return ESP_FAIL;
    }

    // 创建缓冲区可用信号量
    buffer_available_sem = xSemaphoreCreateCounting(BUFFER_COUNT, BUFFER_COUNT);
    if (buffer_available_sem == NULL)
    {
        ESP_LOGE(BUFFER_TAG, "无法创建缓冲区可用信号量，系统可能无法正常工作");
        // 根据需求，可以在此处添加系统重启或进入安全模式的代码
        return ESP_FAIL;
    }

    // 初始化缓冲区状态
    for (int i = 0; i < BUFFER_COUNT; i++)
    {
        AT_Cache.state[i] = BUFFER_FREE;
        AT_Cache.len[i] = 0;
        memset(AT_Cache.data[i], 0, U4G_UART_RX_BUF_SIZE);
    }

    // 创建队列：每个元素包含缓冲区索引和数据长度
    // u4g_data_queue = xQueueCreate(5, sizeof(int) + sizeof(size_t));
    // if (u4g_data_queue == NULL)
    // {
    //     ESP_LOGE(BUFFER_TAG, "数据队列创建失败");
    //     return ESP_FAIL;
    // }
    ESP_LOGI(BUFFER_TAG, "缓冲区管理初始化完成");
    return ESP_OK;
}

/**
 * @brief 获取一个空闲缓冲区进行写入
 * @param buffer_index 输出参数，缓冲区索引
 * @param len 输出参数，当前缓冲区已有数据长度
 * @param wait_ticks 等待的Tick数
 * @return true 获取成功
 * @return false 获取失败
 */
bool u4g_at_buffer_write(int *buffer_index, size_t *len, TickType_t wait_ticks)
{
    bool success = false;

    // 获取可用的缓冲区信号量
    if (xSemaphoreTake(buffer_available_sem, wait_ticks) == pdTRUE)
    {
        if (xSemaphoreTake(u4g_at_buffer_mutex, portMAX_DELAY) == pdTRUE)
        {
            for (int i = 0; i < BUFFER_COUNT; i++)
            {
                if (AT_Cache.state[i] == BUFFER_FREE)
                {
                    *buffer_index = i;
                    *len = AT_Cache.len[i];
                    AT_Cache.state[i] = BUFFER_WRITING; // 设置为写入状态
                    ESP_LOGI(BUFFER_TAG, "获取写入缓冲区：data%d", i + 1);
                    success = true;
                    break;
                }
            }
            // 释放信号量
            if (xSemaphoreGive(u4g_at_buffer_mutex) != pdTRUE)
            {
                ESP_LOGE(BUFFER_TAG, "buffer_get_write_buffer 释放缓冲区信号量失败");
            }
        }
        else
        {
            ESP_LOGW(BUFFER_TAG, "获取互斥锁超时，无法获取写入缓冲区");
        }
    }
    else
    {
        ESP_LOGW(BUFFER_TAG, "获取信号量超时，无法获取写入缓冲区");
    }
    return success;
}

/**
 * @brief 获取一个准备好的缓冲区进行读取
 * @param buffer_index 输出参数，缓冲区索引
 * @param len 输出参数，数据长度
 * @return true 获取成功
 * @return false 获取失败
 */
bool u4g_at_buffer_read(int *buffer_index, size_t *len)
{
    // xSemaphoreGive(buffer_available_sem);
    bool success = false;
    if (xSemaphoreTake(u4g_at_buffer_mutex, portMAX_DELAY) == pdTRUE)
    {
        for (int i = 0; i < BUFFER_COUNT; i++)
        {
            if (AT_Cache.state[i] == BUFFER_READY)
            {
                *buffer_index = i;
                *len = AT_Cache.len[i];
                AT_Cache.state[i] = BUFFER_PROCESSING; // 设置为处理状态
                ESP_LOGI(BUFFER_TAG, "获取读取缓冲区：data%d", i + 1);
                success = true;
                break;
            }
        }
        // 释放信号量
        if (xSemaphoreGive(u4g_at_buffer_mutex) != pdTRUE)
        {
            ESP_LOGE(BUFFER_TAG, "buffer_get_read_buffer 释放缓冲区信号量失败");
        }
    }
    else
    {
        ESP_LOGW(BUFFER_TAG, "获取互斥锁超时，无法获取读取缓冲区");
    }
    ESP_LOGW(BUFFER_TAG, "buffer_get_read_buffer 结束");
    return success;
}

/**
 * @brief 切换缓冲区状态为准备处理
 * @param buffer_index 缓冲区索引
 * @return true 切换成功
 * @return false 切换失败
 */
bool u4g_at_buffer_switch(int buffer_index)
{
    if (xSemaphoreTake(u4g_at_buffer_mutex, portMAX_DELAY) != pdTRUE)
    {
        ESP_LOGE(BUFFER_TAG, "互斥锁获取失败");
        return false;
    }
    if (AT_Cache.state[buffer_index] != BUFFER_WRITING)
    {
        ESP_LOGW(BUFFER_TAG, "缓冲区 %d 未处于写入状态，无法切换", buffer_index);
        // 释放信号量
        if (xSemaphoreGive(u4g_at_buffer_mutex) != pdTRUE)
        {
            ESP_LOGE(BUFFER_TAG, "buffer_switch_buffer 释放缓冲区信号量失败");
        }
        return false;
    }

    // 标记为就绪状态
    AT_Cache.state[buffer_index] = BUFFER_READY; // 切换为已准备状态
    ESP_LOGI(BUFFER_TAG, "缓冲区 %d 已就绪 (数据长度: %d)",
             buffer_index, AT_Cache.len[buffer_index]);
    // // 发送数据到队列（索引+长度）
    // int queue_data[2] = {buffer_index, AT_Cache.len[buffer_index]};
    // if (xQueueSend(u4g_data_queue, &queue_data, pdMS_TO_TICKS(100)) != pdPASS)
    // {
    //     ESP_LOGE(BUFFER_TAG, "队列发送失败！缓冲区 %d 数据可能丢失", buffer_index);
    // }
    if (xSemaphoreGive(u4g_at_buffer_mutex) != pdTRUE)
    {
        ESP_LOGE(BUFFER_TAG, "buffer_switch_buffer 释放缓冲区信号量失败");
    }
    return true;
}

/**
 * @brief 释放处理完成的缓冲区
 * @param buffer_index 缓冲区索引
 */
void u4g_at_buffer_release(int buffer_index)
{
    ESP_LOGD(BUFFER_TAG, "at_buffer_release准备释放缓冲区: data%d", buffer_index + 1);
    if (xSemaphoreTake(u4g_at_buffer_mutex, portMAX_DELAY) == pdTRUE)
    {
        AT_Cache.len[buffer_index] = 0;
        // ESP_LOGD(BUFFER_TAG, "at_buffer_release正在释放缓冲区: data%d, 步骤: 1/4", buffer_index + 1);
        memset(AT_Cache.data[buffer_index], 0, U4G_UART_RX_BUF_SIZE);
        // ESP_LOGD(BUFFER_TAG, "at_buffer_release正在释放缓冲区: data%d, 步骤: 2/4", buffer_index + 1);
        AT_Cache.state[buffer_index] = BUFFER_FREE; // 释放缓冲区
        // ESP_LOGD(BUFFER_TAG, "at_buffer_release正在释放缓冲区: data%d, 步骤: 3/4", buffer_index + 1);
        ESP_LOGI(BUFFER_TAG, "释放缓冲区: data%d", buffer_index + 1);
        // 释放信号量
        if (xSemaphoreGive(u4g_at_buffer_mutex) != pdTRUE)
        {
            ESP_LOGE(BUFFER_TAG, "at_buffer_release 释放缓冲区信号量失败");
        }
        // ESP_LOGD(BUFFER_TAG, "at_buffer_release正在释放缓冲区: data%d, 步骤: 4/4", buffer_index + 1);
    }
    else
    {
        ESP_LOGE(BUFFER_TAG, "at_buffer_release 超时");
    }
    // 释放信号量
    if (xSemaphoreGive(buffer_available_sem) != pdTRUE)
    {
        ESP_LOGE(BUFFER_TAG, "释放缓冲区信号量失败");
    }
}

/**
 * @brief 清空缓冲区
 * @param type 缓冲区类型，1表示具体某个缓冲区，0表示全部
 */
void u4g_at_buffer_clear(int type)
{
    // ESP_LOGD(BUFFER_TAG, "执行清空缓冲区");
    if (xSemaphoreTake(u4g_at_buffer_mutex, portMAX_DELAY) == pdTRUE)
    {
        if (type > 0 && type <= BUFFER_COUNT)
        {
            int idx = type - 1;
            memset(AT_Cache.data[idx], 0, U4G_UART_RX_BUF_SIZE);
            AT_Cache.len[idx] = 0;
            AT_Cache.state[idx] = BUFFER_FREE;
            // 释放信号量
            if (xSemaphoreGive(buffer_available_sem) != pdTRUE)
            {
                ESP_LOGE(BUFFER_TAG, "at_buffer_clear 释放缓冲区信号量失败");
            }
            ESP_LOGI(BUFFER_TAG, "清空缓冲区：data%d 并释放信号量", type);
        }
        else
        {
            // 清空所有缓冲区
            for (int i = 0; i < BUFFER_COUNT; i++)
            {
                memset(AT_Cache.data[i], 0, U4G_UART_RX_BUF_SIZE);
                AT_Cache.len[i] = 0;
                AT_Cache.state[i] = BUFFER_FREE;
            }
            // 重置信号量为BUFFER_COUNT
            while (uxSemaphoreGetCount(buffer_available_sem) < BUFFER_COUNT)
            {
                // 释放信号量
                if (xSemaphoreGive(buffer_available_sem) != pdTRUE)
                {
                    ESP_LOGE(BUFFER_TAG, "at_buffer_clear buffer_available_sem释放缓冲区信号量失败");
                }
            }
            // ESP_LOGD(BUFFER_TAG, "清空所有缓冲区并重置信号量");
        }
        // 释放信号量
        if (xSemaphoreGive(u4g_at_buffer_mutex) != pdTRUE)
        {
            ESP_LOGE(BUFFER_TAG, "at_buffer_clear 释放缓冲区信号量失败");
        }
    }
}

/**
 * @brief 获取缓冲区实例
 * @return u4g_at_cache_t*
 */
u4g_at_cache_t *u4g_at_buffer_get(void)
{
    return &AT_Cache;
}
