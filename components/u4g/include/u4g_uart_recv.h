#ifndef _U4G_UART_RECV_H_
#define _U4G_UART_RECV_H_

#include "u4g_state.h" // 状态码
#include <stdbool.h>
#include <stdint.h>

// int32_t u4g_uart_recv_process(uint8_t *pdata, uint32_t size);
emU4GResult u4g_uart_recv_process(char *data, uint32_t size);

#endif