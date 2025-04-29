#ifndef _A_NETWORK_H_ // 检查是否未定义
#define _A_NETWORK_H_ // 如果未定义，则定义

#include "esp_err.h" // 使用正确的ESP错误码定义
#include <stdbool.h>

#define WDT_TIMEOUT_SEC 10  // 整体看门狗超时时间
#define TASK_WDT_TIMEOUT 15 // 单个任务看门狗超时

void a_network_init(void *pvParameters);
void handle_device_auth(void);
void handle_device_imei(void);
void handle_module_check(void);
void handle_at_init(void);
esp_err_t network_auth_4g(void);

#endif