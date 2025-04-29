#ifndef U4G_DATA_H
#define U4G_DATA_H

#include <time.h>
#include "u4g_state.h"

typedef struct
{
    char imei[24];
    char time[64];
    char mccid[32];
    int8_t csq; // 信号值
    uint8_t data[U4G_UART_RX_BUF_SIZE];
} u4g_data_t;
extern u4g_data_t u4g_data;

// // 新增版本控制结构
// typedef struct {
//     uint32_t data_version; // 数据版本号
//     u4g_module_t data;     // 实际数据
// } u4g_module_safe_t;

emU4GResult u4g_data_init(void);
// u4g_data_t u4g_data_get(void);
u4g_data_t* u4g_data_get(void);

#endif