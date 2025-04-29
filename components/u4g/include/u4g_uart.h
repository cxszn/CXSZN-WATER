/*
 * @function:
 * @Description:
 * @Author: 初馨顺之诺
 * @contact: 微信：cxszn01
 * @link: shop.cxszn.com
 * @Date: 2024-12-04 02:44:37
 * @LastEditTime: 2024-12-04 20:27:38
 * @FilePath: \CXSZN-WATER\components\uart_at\include\uart.h
 * Copyright (c) 2024 by 临沂初馨顺之诺网络科技中心, All Rights Reserved.
 */
#ifndef UART_H
#define UART_H

#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern TaskHandle_t uart_recv_task_handle;

esp_err_t u4g_uart_init(void);
int32_t u4g_uart_send(const char *p_data, uint16_t len);
void u4g_uart_reset(void);

#endif