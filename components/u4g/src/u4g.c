#include "u4g.h"
#include "u4g_state.h"
#include "u4g_uart.h"
#include "u4g_at_cmd.h"
#include "u4g_at_http.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define TAG "U4G"

emU4GResult u4g_init(void)
{
    // 初始化UART
    if (u4g_uart_init() != U4G_OK)
    {
        ESP_LOGE(TAG, "UART初始化失败");
        return U4G_FAIL;
    }

    // AT命令初始化
    if (u4g_at_cmd_init() != U4G_OK)
    {
        ESP_LOGE(TAG, "UART初始化失败");
        return U4G_FAIL;
    }

    if (u4g_at_http_init() != U4G_OK)
    {
        ESP_LOGE(TAG, "u4g_at_http_init初始化失败");
        return U4G_FAIL;
    }
    return U4G_OK;
}
