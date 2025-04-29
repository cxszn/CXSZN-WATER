#include "u4g_at_cmd.h"
#include "u4g_state.h"
#include "u4g_uart.h"
#include "u4g_data.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h" // 包含看门狗相关库
#include <string.h>       // 增加字符串操作支持
#include "driver/uart.h"  // 包含 UART 驱动头文件

#define TAG "U4G-AT-CMD"

static SemaphoreHandle_t cmd_mutex = NULL;          // 发送数据互斥锁
SemaphoreHandle_t u4g_at_cmd_semaphore_sync = NULL; // 发送数据响应同步信号量
u4g_at_cmd_handle_t u4g_at_cmd_handle = {0};

/**
 * @brief 初始化AT命令处理模块
 * @return emU4GResult 状态码，成功返回U4G_OK，否则返回错误码
 */
emU4GResult u4g_at_cmd_init(void)
{
    cmd_mutex = xSemaphoreCreateMutex();
    if (cmd_mutex == NULL)
    { // 互斥锁创建失败
        ESP_LOGE(TAG, "创建命令互斥锁失败");
        return U4G_FAIL;
    }
    u4g_at_cmd_semaphore_sync = xSemaphoreCreateBinary();
    if (!u4g_at_cmd_semaphore_sync)
    {
        ESP_LOGE(TAG, "发送数据响应同步信号量创建失败");
        vSemaphoreDelete(cmd_mutex);
        return U4G_FAIL;
    }
    if (u4g_data_init() != U4G_OK)
    {
        ESP_LOGE(TAG, "数据模块初始化失败");
        return U4G_FAIL;
    }
    ESP_LOGI(TAG, "AT 命令模块初始化完成");
    return U4G_OK;
}

/**
 * @brief 同步发送AT命令并等待响应
 * @param cmd 指向AT命令结构体的指针
 * @return emU4GResult 发送结果，成功返回U4G_OK，否则返回错误码
 */
emU4GResult u4g_at_cmd_sync(const u4g_at_cmd_t *config)
{
    ESP_LOGW(TAG, "u4g_at_cmd_sync 开始");
    if (config == NULL)
    {
        ESP_LOGE(TAG, "AT命令指针为空");
        return U4G_ERR_INVALID_ARG;
    }
    // 获取互斥锁成功，可以访问共享资源
    if (xSemaphoreTake(cmd_mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "u4g_at_cmd_sync-获取命令锁超时");
        xSemaphoreGive(cmd_mutex);
        return U4G_ERR_TIMEOUT;
    }
    vTaskDelay(pdMS_TO_TICKS(2500));

    // 获取当前任务的优先级
    UBaseType_t uxCurrentPriority = uxTaskPriorityGet(NULL);
    // 提升任务优先级
    vTaskPrioritySet(NULL, 15); // 假设 10 是较高的优先级
    ESP_LOGI(TAG, "为u4g_at_cmd_sync临时提升优先级原 %d|现%d", uxCurrentPriority, 15);

    // 初始化命令上下文
    u4g_at_cmd_handle.cmd_content = config;
    // u4g_at_cmd_handle.state = AT_MSG_NOT; // 无数据
    u4g_at_cmd_handle.result = U4G_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "发送 AT 指令: %s | 字节%d", config->cmd, config->cmd_len); // 发送AT命令
    int32_t send = u4g_uart_send(config->cmd, config->cmd_len);               // 发送命令
    if (send < U4G_OK)
    {
        ESP_LOGE(TAG, "发送AT命令失败: %ld", send);
        xSemaphoreGive(cmd_mutex);
        return AT_FAIL;
    }

    emU4GResult result = U4G_STATE_AT_GET_RSP_FAILED; // 响应超时
    // ESP_LOGW(TAG, "AT测试阶段：1");
    TickType_t start_time = xTaskGetTickCount();
    // ESP_LOGW(TAG, "AT测试阶段：2");
    while ((xTaskGetTickCount() - start_time) < pdMS_TO_TICKS(config->timeout + 1000))
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        // ESP_LOGW(TAG, "AT测试阶段：3");
        // esp_task_wdt_reset();
        // if (xSemaphoreTake(u4g_at_cmd_semaphore_sync, portMAX_DELAY)) // 等待信号量
        if (xSemaphoreTake(u4g_at_cmd_semaphore_sync, pdMS_TO_TICKS(3000))) // 等待信号量
        {
            // 获取信号量，处理数据
            ESP_LOGI(TAG, "获取信号量，处理数据");
            result = u4g_at_cmd_handle.result;
            // ESP_LOGW(TAG, "AT测试阶段：4");
            ESP_LOGD(TAG, "收到AT命令[%10s]响应: %d", u4g_at_cmd_handle.cmd_content->cmd, u4g_at_cmd_handle.result);
            break;
        }
        else
        {
            // ESP_LOGW(TAG, "AT测试阶段：4-1");
            ESP_LOGE(TAG, "AT命令[%20s]响应超时", config->cmd);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        // ESP_LOGW(TAG, "AT测试阶段：5");
    }
    // ESP_LOGW(TAG, "AT测试阶段：6");
    // 执行完成后恢复原始优先级
    vTaskPrioritySet(NULL, uxCurrentPriority);
    ESP_LOGI(TAG, "u4g_at_cmd_sync临时优先级已恢复为%d", uxCurrentPriority);
    // ESP_LOGW(TAG, "AT测试阶段：7");
    // 清理上下文
    u4g_at_cmd_handle.cmd_content = NULL;
    // ESP_LOGW(TAG, "AT测试阶段：8");
    xSemaphoreGive(cmd_mutex); // 释放互斥锁
    // ESP_LOGW(TAG, "AT测试阶段：9");
    return result;
}

// 发送AT指令
emU4GResult u4g_at(void)
{
    u4g_at_cmd_t config = {
        .name = U4G_AT_CMD,
        .cmd = "AT\r\n",
        .cmd_len = 4,
    };
    emU4GResult ret = u4g_at_cmd_sync(&config);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "AT命令-失败:%d", ret);
        return U4G_FAIL;
    }
    return U4G_OK;
}

static emU4GResult handler_imei(char *rsp)
{
    emU4GResult res = U4G_STATE_AT_RSP_WAITING;
    char *pt = strstr(rsp, "+CGSN: ");
    if (pt)
    {
        pt += strlen("+CGSN: ");
        sscanf(pt, "%s", u4g_data.imei);
        ESP_LOGI(TAG, "IMEI = %s", u4g_data.imei);
        res = U4G_OK;
    }
    else
    {
        ESP_LOGE(TAG, "未找到 CGSN 字符串或错误信息");
        res = U4G_FAIL;
    }
    return res;
}
// 获取IMEI
emU4GResult u4g_at_imei_get(void)
{
    u4g_at_cmd_t config = {
        .name = U4G_AT_CMD_IMEI,
        .cmd = "AT+CGSN=1\r\n",
        .cmd_len = 11,
        .timeout = 5000,
        .handler = handler_imei};
    emU4GResult ret = u4g_at_cmd_sync(&config);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "IMEI 命令回调-失败:%d", ret);
        return U4G_FAIL;
    }
    return U4G_OK;
}

static emU4GResult handler_cereg(char *rsp)
{
    char *pt = strstr(rsp, "+CEREG: ");
    if (pt)
    {
        pt += strlen("+CEREG: "); // 添加这行，跳过前缀
        int n = 0, stat = 0;
        if (sscanf(pt, "%d,%d", &n, &stat) == 2)
        {
            ESP_LOGI(TAG, "CEREG: n=%d, stat=%d", n, stat);
            if (stat == 1 || stat == 5) // 1: 已注册，本地网络; 5: 已注册，漫游
            {
                return U4G_OK;
            }
            else if (stat == 0)
            { // 未联网
                return U4G_STATE_AT_NET_NOT;
            }
            else
            {
                return U4G_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "字符串格式不正确，无法解析 CEREG 数据: [%s]", pt);
            return U4G_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "未找到 CEREG 字符串");
        return U4G_FAIL;
    }
}

// 驻网状态查询
emU4GResult u4g_at_netstatus(void)
{
    u4g_at_cmd_t config = {
        .name = U4G_AT_CMD_CEREG,
        .cmd = "AT+CEREG?\r\n",
        .cmd_len = 11,
        .handler = handler_cereg};
    emU4GResult ret = u4g_at_cmd_sync(&config);
    if (ret == U4G_STATE_AT_NET_NOT)
    {
        // 未联网
        ESP_LOGE(TAG, "驻网状态查询-未联网");
        return U4G_STATE_AT_NET_NOT;
    }
    else if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "驻网状态查询-失败:%d", ret);
        return U4G_FAIL;
    }
    return U4G_OK;
}

static emU4GResult handler_time(char *rsp)
{
    emU4GResult res = U4G_STATE_AT_RSP_WAITING;
    char *line = NULL;
    line = strstr(rsp, "+CCLK: ");
    if (line)
    {
        line += strlen("+CCLK: ");
        // 示例响应: +CCLK: "24/12/23,03:18:05+32"
        const char *start = strchr(line, '\"');
        if (!start)
        {
            ESP_LOGE(TAG, "无法找到引号，解析失败");
            res = U4G_FAIL;
        }
        start++; // 移动到引号后的第一个字符

        // 查找第二个引号
        const char *end = strchr(start, '\"');
        if (!end)
        {
            ESP_LOGE(TAG, "无法找到结束引号，解析失败");
            res = U4G_FAIL;
        }

        // 提取时间字符串
        size_t len = end - start;
        if (len >= sizeof(u4g_data.time))
        {
            ESP_LOGE(TAG, "时间字符串过长或缓冲区无效");
            res = U4G_FAIL;
        }

        strncpy(u4g_data.time, start, len);
        u4g_data.time[len] = '\0';
        res = U4G_OK;
        ESP_LOGI(TAG, "解析后的时间字符串: %s", u4g_data.time);
    }
    else
    {
        ESP_LOGE(TAG, "未找到 解析后的时间字符串");
        res = U4G_FAIL;
    }
    return res;
}
/**
 * 从NTP获取时间
 * time_out 时间戳输出
 */
emU4GResult u4g_at_time_get(void)
{
    u4g_at_cmd_t config = {
        .name = U4G_AT_CMD_TIME,
        .cmd = "AT+CCLK?\r\n",
        .cmd_len = 10,
        .handler = handler_time};
    emU4GResult ret = u4g_at_cmd_sync(&config);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "从NTP获取时间-失败:%d", ret);
        return U4G_FAIL;
    }
    return U4G_OK;
}

static emU4GResult handler_mccid(char *rsp)
{
    emU4GResult res = U4G_STATE_AT_RSP_WAITING;
    char *pt = NULL;
    pt = strstr(rsp, "+MCCID: ");
    if (pt)
    {
        pt += strlen("+MCCID: ");
        // int pt_len = strlen(pt); // 计算数据长度
        // 使用 sscanf 读取数据
        sscanf(pt, "%s", u4g_data.mccid);
        ESP_LOGI(TAG, "MCCID = %s", u4g_data.mccid);
        res = U4G_OK;
    }
    else
    {
        ESP_LOGE(TAG, "未找到 MCCID 字符串或错误信息");
        res = U4G_FAIL;
    }
    return res;
}
// 获取MCCID(SIM)号
emU4GResult u4g_at_mccid_get(void)
{
    u4g_at_cmd_t config = {
        .name = U4G_AT_CMD_MCCID,
        .cmd = "AT+MCCID\r\n",
        .cmd_len = 10,
        .handler = handler_mccid};
    emU4GResult ret = u4g_at_cmd_sync(&config);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "获取MCCID(SIM)-失败:%d", ret);
        return U4G_FAIL;
    }
    return U4G_OK;
}

// 4G模块软重启
emU4GResult u4g_at_reboot_soft(void)
{
    u4g_at_cmd_t config = {
        .name = U4G_AT_CMD_REBOOT,
        .cmd = "AT+MREBOOT\r\n",
        .cmd_len = 12,
    };
    emU4GResult ret = u4g_at_cmd_sync(&config);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "重启4G模块-失败:%d", ret);
        return U4G_FAIL;
    }
    return U4G_OK;
}
// 4G模块硬重启
emU4GResult u4g_at_reboot_hard(void)
{
    u4g_uart_reset();
    return U4G_OK;
}

static emU4GResult handler_csq(char *rsp)
{
    emU4GResult res = U4G_STATE_AT_RSP_WAITING;
    char *pt = NULL;
    pt = strstr(rsp, "+CSQ: ");
    if (pt)
    {
        pt += strlen("+CSQ: ");

        // 使用 sscanf 提取第一个数字（rssi）
        int rssi = 0;
        if (sscanf(pt, "%d,", &rssi) == 1)
        {
            // 安全地存储结果
            if (rssi >= sizeof(int))
            {
                u4g_data.csq = rssi;
                ESP_LOGI(TAG, "解析到CSQ值: %d", u4g_data.csq);
                res = U4G_OK;
            }
            else
            {
                ESP_LOGE(TAG, "CSQ结果缓冲区无效");
                res = U4G_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "字符串格式不正确，无法解析 CSQ 数据");
            res = U4G_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "未找到 +CSQ: 字符串");
        res = U4G_FAIL;
    }
    return res;
}
// 信号值
emU4GResult u4g_at_csq(void)
{
    u4g_at_cmd_t config = {
        .name = U4G_AT_CMD_CSQ,
        .cmd = "AT+CSQ\r\n",
        .cmd_len = 8,
        .handler = handler_csq};
    emU4GResult ret = u4g_at_cmd_sync(&config);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "CSQ信号值指令-失败:%d", ret);
        return U4G_FAIL;
    }
    return U4G_OK;
}