#ifndef U4G_AT_BUFFER_H
#define U4G_AT_BUFFER_H

#include "u4g_state.h" // 状态码
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>

// #define RX_BUFFER_SIZE 2048                    // 单个缓冲区的字节数
#define BUFFER_COUNT 2                         // 缓冲区数量
extern SemaphoreHandle_t u4g_at_buffer_mutex; // 缓冲区互斥信号量
extern QueueHandle_t u4g_data_queue; // 全局数据队列句柄

// 缓冲区状态枚举
typedef enum
{
    BUFFER_FREE,      // 缓冲区空闲，可写入
    BUFFER_WRITING,   // 正在写入数据
    BUFFER_READY,     // 数据准备好，等待处理
    BUFFER_PROCESSING // 数据正在被处理
} u4g_at_buffer_state_t;

// 缓存区结构体
typedef struct
{
    char data[BUFFER_COUNT][U4G_UART_RX_BUF_SIZE];   // 缓存区数据
    size_t len[BUFFER_COUNT];                  // 数据长度
    u4g_at_buffer_state_t state[BUFFER_COUNT]; // 缓存区状态
} u4g_at_cache_t;

esp_err_t u4g_at_buffer_init(void);
bool u4g_at_buffer_write(int *buffer_index, size_t *len, TickType_t wait_ticks);
bool u4g_at_buffer_read(int *buffer_index, size_t *len);
bool u4g_at_buffer_switch(int buffer_index);
void u4g_at_buffer_release(int buffer_index);
void u4g_at_buffer_clear(int type);
u4g_at_cache_t *u4g_at_buffer_get(void);

#endif