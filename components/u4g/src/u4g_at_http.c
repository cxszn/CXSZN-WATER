#include "u4g_at_http.h"
#include "u4g_state.h"
#include "u4g_at_cmd.h"
#include "u4g_data.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdio.h>        //默认
#include <string.h>       // 增加字符串操作支持
#include "esp_task_wdt.h" // 包含看门狗相关库

#define TAG "U4G_AT_HTTP" // 日志TAG

static u4g_at_cmd_t config;                 // 本文件内静态变量
static SemaphoreHandle_t http_mutex = NULL; // 发送数据互斥锁

emU4GResult u4g_at_http_init(void)
{
    if (http_mutex)
    {
        return U4G_OK;
    }
    http_mutex = xSemaphoreCreateMutex();
    if (http_mutex == NULL)
    { // 互斥锁创建失败
        ESP_LOGE(TAG, "创建HTTP命令互斥锁失败");
        return U4G_FAIL;
    }
    return U4G_OK;
}

// 回调-客户端ID生成
static uint8_t httpid = 255; // HTTP客户端ID
static emU4GResult handler_http_client(char *rsp)
{
    emU4GResult res = U4G_STATE_AT_RSP_WAITING;
    char *line = strstr(rsp, "+MHTTPCREATE:");
    if (line != NULL && sscanf(line, "+MHTTPCREATE: %hhu\r\n", &httpid))
    {
        if (httpid > 4)
        {
            res = U4G_FAIL;
        }
        else
        {
            ESP_LOGI(TAG, "httpid: %d", httpid);
            res = U4G_OK;
        }
    }
    return res;
}

/**
 * HTTP客户端-实例删除（对应 AT+MHTTPDEL=<httpid>）
 * <httpid> 整型，HTTP客户端实例id，0-3
 */
static emU4GResult u4g_at_http_client_del(const uint8_t client_id)
{
    if (client_id > 3)
    {
        ESP_LOGE(TAG, "删除拒绝：非法ID %d (允许0-3)", client_id);
        return U4G_ERR_INVALID_ARG; // 使用标准错误码
    }

    u4g_at_cmd_t config2;
    config2.name = U4G_AT_CMD_HTTP_DELETE;
    char cmd[16];
    config2.cmd_len = snprintf(cmd, 16, "AT+MHTTPDEL=%d\r\n", client_id);
    config2.cmd = cmd;
    config2.timeout = 3000;
    emU4GResult ret = u4g_at_cmd_sync(&config2);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "删除客户端-失败:%d", ret);
        return U4G_FAIL;
    }
    return U4G_OK;
}

typedef struct
{
    uint32_t content_len; // 内容总长度
    uint32_t old_sum_len; // 上一批已下载数据长度
    uint32_t sum_len;     // 已下载数据长度
    uint32_t cur_len;     // 当前下载的数据长度
    uint32_t read_len;    // 剩余字节处理
} u4g_at_http_data_t;
static u4g_at_http_data_t httpData = {0};

static emU4GResult parse_data(char *rsp, size_t rsp_len)
{
    // 不同数据时
    if (httpData.old_sum_len != httpData.sum_len)
    {
        // 验证已下载数据长度是否一致
        if ((httpData.sum_len - httpData.old_sum_len) != httpData.cur_len)
        {
            httpData.old_sum_len = 0;
            ESP_LOGE(TAG, "下载数据数量不一致: sum_len=%lu, old_sum_len=%lu, cur_len=%lu", httpData.sum_len, httpData.old_sum_len, httpData.cur_len);
            return U4G_FAIL;
        }
        // 首次接收数据时
        if (httpData.cur_len == httpData.sum_len)
        {
            memset(u4g_data.data, 0, U4G_UART_RX_BUF_SIZE); // 清空缓冲区
        }
    }
    httpData.old_sum_len = httpData.sum_len; // 更新已下载数据长度

    // 去掉尾部的回车换行字符
    while (rsp_len > 0 && (rsp[rsp_len - 1] == '\r' || rsp[rsp_len - 1] == '\n'))
    {
        rsp_len--;
    }
    size_t copy_len = (rsp_len < U4G_UART_RX_BUF_SIZE - 1) ? rsp_len : (U4G_UART_RX_BUF_SIZE - 1);
    size_t current_len = strlen((char *)u4g_data.data); // 计算当前数据长度

    // 确保不会超出缓冲区
    if (current_len + copy_len > U4G_UART_RX_BUF_SIZE - 1)
    {
        ESP_LOGE(TAG, "HTTP回调缓冲区溢出，无法追加数据 复制copy_len: %d | current_len: %d | RX: %d", copy_len, current_len, U4G_UART_RX_BUF_SIZE - 1);
        return U4G_STATE_AT_RINGBUF_OVERFLOW;
    }

    ESP_LOGI(TAG, "[HTTP]提取所需数据[%d]: %s", copy_len, rsp);

    // 追加新数据
    strncpy((char *)u4g_data.data + current_len, rsp, copy_len);
    ((char *)u4g_data.data)[current_len + copy_len] = '\0'; // 添加字符串终止符

    ESP_LOGI(TAG, "[HTTP]追加后新数据[%d]: %s", strlen((char *)u4g_data.data), (char *)u4g_data.data);

    httpData.read_len += copy_len;
    if (httpData.read_len > httpData.cur_len) // 接收数据大于预期数据
    {
        ESP_LOGE(TAG, "[HTTP]接收数据大于预期数据");
        return U4G_FAIL;
    }
    else if (httpData.read_len != httpData.cur_len)
    {
        ESP_LOGW(TAG, "当前HTTP下载的数据长度有剩余-继续接收 | 预期长度=%lu，数据不完整，等待下一批数据", httpData.cur_len);
        return U4G_STATE_AT_RSP_WAITING; // 继续等待命令回执
    }

    // 检查是否已接收完所有内容
    if (httpData.content_len == httpData.sum_len)
    {
        ESP_LOGI(TAG, "已接收完所有数据");
        httpData.old_sum_len = 0;
        return U4G_OK;
    }
    return U4G_FAIL;
}

static emU4GResult handler_http(char *rsp)
{
    emU4GResult res = U4G_STATE_AT_RSP_WAITING;
    char *pt = strstr(rsp, "+MHTTPURC: \"content\"");
    if (pt)
    {
        pt += strlen("+MHTTPURC: \"content\"");
        // ESP_LOGI(TAG, "CMD_HTTP_REQUEST 指令 HTTP请求成功");
        // if (httpData.cur_len > 0)
        // {
        //     ESP_LOGD(TAG, "检测到不是全新数据，中断处理");
        //     httpData.cur_len = 0;
        //     return U4G_FAIL; // 继续等待命令回执
        // }

        // 格式: +MHTTPURC: "content",<httpid>,<content_len>,<sum_len>,<cur_len>,<data>
        int httpid = 0;
        int offset = 0;
        // 修正: 解析格式，跳过前缀后应该匹配逗号和四个数字
        if (sscanf(pt, ",%d,%ld,%ld,%ld,%n", &httpid, &httpData.content_len, &httpData.sum_len, &httpData.cur_len, &offset) == 4)
        {
            ESP_LOGW(TAG, "HTTP请求| httpid: %d | content_len: %ld | sum_len: %ld | cur_len: %ld | offset: %d", httpid, httpData.content_len, httpData.sum_len, httpData.cur_len, offset);
            if (offset > 0 && offset < strlen(pt))
            {
                httpData.read_len = 0; // 重置剩余字节处理
                // 计算数据起始位置并安全地复制数据
                char *data_ptr = pt + offset;
                res = parse_data(data_ptr, strlen(data_ptr));
            }
            else
            {
                ESP_LOGE(TAG, "HTTP数据偏移量无效: %d", offset);
                res = U4G_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "字符串格式不正确，无法解析HTTP数据: [%s]", pt);
            res = U4G_FAIL;
        }
    }
    else
    {
        ESP_LOGI(TAG, "HTTP请求数据-接收剩余数据处理");
        res = parse_data(rsp, strlen(rsp));
    }
    return res;
}

// HTTP客户端协议配置
#define AT_CMD_MAX_URL_LEN 128
#define AT_CMD_HTTP_CLIENT_BUFFER_SIZE (sizeof("AT+MHTTPCREATE=\"") - 1 + AT_CMD_MAX_URL_LEN + sizeof("\"\r\n") - 1 + 1)
// // 编译期静态检查
// _Static_assert(AT_CMD_HTTP_CLIENT_BUFFER_SIZE == 16 + 128 + 4, "HTTP客户端缓冲区尺寸计算错误");
static bool task_wdt_reset_send = false;

/** HTTP 请求 */
emU4GResult u4g_at_http_request(const char *url, const char *path, const char *body)
{
    ESP_LOGD(TAG, "开始HTTP 请求");
    if (!url || !path || strlen(url) == 0 || strlen(path) == 0)
    {
        ESP_LOGE(TAG, "无效参数 url 或 path");
        return U4G_ERR_INVALID_ARG;
    }
    if (strlen(url) > AT_CMD_MAX_URL_LEN)
    {
        ESP_LOGE(TAG, "URL 长度超限 %d > %d", strlen(url), AT_CMD_MAX_URL_LEN);
        return U4G_ERR_INVALID_SIZE;
    }
    // 获取锁
    if (xSemaphoreTake(http_mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "HTTP 锁获取超时");
        // xSemaphoreGive(http_mutex);
        return U4G_ERR_TIMEOUT;
    }
    // esp_task_wdt_add(NULL);

    esp_task_wdt_reset();

    emU4GResult ret;
    // ATE回显关闭
    config.name = U4G_AT_CMD_ATE;
    config.cmd = "ATE0\r\n";
    config.cmd_len = 6;
    config.timeout = 3000;
    ret = u4g_at_cmd_sync(&config);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "关闭回显-失败:%d", ret);
        return ret;
    }

    /**
     * @brief 生成SSL认证配置指令-无需认证
     * AT+MSSLCFG="auth"[,<ssl_id>[,<cert_verify>]]
     *  * <ssl_id> 整型，SSL连接ID，范围：0~5
     * <cert_verify> 整型，证书认证方式，默认值0
     * -0无身份认证，默认值 -1单向认证 -2双向认证
     */
    config.name = U4G_AT_CMD_SSL_AUTH;
    config.cmd = "AT+MSSLCFG=\"auth\",0,0\r\n";
    config.cmd_len = 23;
    config.timeout = 3000;
    ret = u4g_at_cmd_sync(&config);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "设置SSL认证方式无需认证-失败:%d", ret);
        return ret;
    }

    /**
     * @brief HTTP客户端-创建HTTP客户端实例（对应 AT+MHTTPCREATE）
     * @param url 字符串，HTTP服务器地址，例如：https://api.iot.zsyxlife.cn
     * @param timeout 超时，单位毫秒
     * httpid 返回值：-1 失败，其他值为httpid 0-3，用于后续的HTTP请求
     * @return at_cmd_t
     */
    static char buf[256];
    int len = snprintf(buf, sizeof(buf), "AT+MHTTPCREATE=\"%s\"\r\n", url);
    u4g_at_cmd_t cmd_table_httpcreate = {
        .name    = U4G_AT_CMD_HTTP_CREATE,
        .cmd     = buf,
        .cmd_len = len,
        .timeout = 3000,
        .handler = handler_http_client
    };
    // 带重试机制执行
    ret = U4G_ERR_INVALID_STATE;
    esp_task_wdt_reset();
    for (uint8_t retry = 0; retry < 3; retry++)
    {
        // ESP_LOGI("AT_CMD", "发送命令: %.*s", config.cmd_len, config.cmd);
        ret = u4g_at_cmd_sync(&cmd_table_httpcreate);
        esp_task_wdt_reset();
        if (ret == U4G_OK)
        {
            break;
        }
        else if (ret == U4G_STATE_AT_NO_CLIENT_IDLE)
        {
            ESP_LOGE(TAG, "创建HTTP客户端实例-执行删除客户端ID为0:%d", ret);
            ret = u4g_at_http_client_del(0); // 删除客户端
            vTaskDelay(pdMS_TO_TICKS(100));
            if (ret == U4G_OK)
            {
                httpid = 0;
                continue; // 继续循环
            }
            else
            {
                ESP_LOGE(TAG, "删除客户端失败");
            }
        }
        else
        {
            ESP_LOGE(TAG, "创建HTTP客户端实例-失败:%d", ret);
        }
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(100 * (retry + 1))); // 指数退避
    }
    // ESP_LOGE(TAG, "测试到这里");

    /**
     * HTTP客户端-SSL模式设置（对应 AT+MHTTPCFG）
     * <ssl> 整型，0-关闭，1-开启，默认值0
     * <cert_verify> 整型，证书认证方式，0-无认证，1-单向认证，2-双向认证
     */
    config.name = U4G_AT_CMD_HTTP_SSL;
    config.cmd_len = snprintf(config.cmd, 26, "AT+MHTTPCFG=\"ssl\",%u,1,0\r\n", httpid);
    config.timeout = 3000;
    config.handler = NULL;
    ret = u4g_at_cmd_sync(&config);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "HTTP客户端-SSL模式设置-失败:%d", ret);
        return U4G_FAIL;
    }

    /**
     * HTTP客户端-设置输入输出编码
     * AT+MHTTPCFG="encoding"[,<httpid>[,<input_format>[,<output_format>]]]
     * <input_format> 数据输入编码模式，默认值0。模组将按设置的编码格式把输入数据转换为原始数据
     * -0ASCII字符串（原始数据） -1HEX字符串 -2带转义的字符串
     * <output_format> 数据输出编码模式，默认值0。模组将按设置的编码格式把原始数据转换为指定编码格式数据然后输出
     * -0ASCII字符串（原始数据） -1HEX字符串
     */
    config.name = U4G_AT_CMD_HTTP_ENCODING;
    config.cmd_len = snprintf(config.cmd, 31, "AT+MHTTPCFG=\"encoding\",%u,%d,%d\r\n", httpid, 0, 0);
    config.timeout = 3000;
    config.handler = NULL;
    ret = u4g_at_cmd_sync(&config);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "HTTP客户端-设置输入输出编码-失败:%d", ret);
        return U4G_FAIL;
    }

    /**
     * HTTP客户端-设置数据输出流控
     * AT+MHTTPCFG="fragment"[,<httpid>[,<frag_size>[,<interval>]]]
     * <frag_size> 接收到数据后，数据上报的最大分包大小，0~1024，默认值0（实际接收包大小输出）
     * <interval> 接收到数据后，数据分包输出的时间间隔，0~2000ms，默认值0（无间隔）
     */
    config.name = U4G_AT_CMD_HTTP_FRAGMENT;
    config.cmd_len = snprintf(config.cmd, 37, "AT+MHTTPCFG=\"fragment\",%u,%d,%d\r\n", httpid, 0, 100);
    config.timeout = 3000;
    config.handler = NULL;
    ret = u4g_at_cmd_sync(&config);
    vTaskDelay(pdMS_TO_TICKS(100));
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "HTTP客户端-设置数据输出流控-失败:%d", ret);
        return U4G_FAIL;
    }

    if (body == NULL)
    {
        /**
         * HTTP客户端-发送HTTP请求（对应 AT+MHTTPREQUEST=<httpid>,<method>[,<length>[,<path>[,<local_path>]]]）
         * <httpid> 整型，HTTP客户端实例id，0~3
         * <method> 整型，1-GET，2-POST，3-PUT，4-DELETE，5-HEAD
         * <path> 字符串，HTTP请求路径，例如：/api/device/info
         */
        config.name = U4G_AT_CMD_HTTP_REQUEST;
        // Step 1: 计算需要的缓冲区大小（包含转义字符）
        int required_space = snprintf(NULL, 0, "AT+MHTTPREQUEST=%d,1,0,\"", httpid) + 1; // 基础部分
        required_space += strlen(path) * 2;                                              // 预留双倍空间（应对特殊字符转义）
        required_space += 3;                                                             // 结尾的 "\"\r\n" 和终止符

        // Step 2: 动态分配缓冲区（避免栈溢出）
        char *temp_buffer = malloc(required_space);
        if (!temp_buffer)
        {
            ESP_LOGE(TAG, "Memory allocation failed");
            return ESP_ERR_NO_MEM;
        }
        // Step 3: 转义双引号并构建路径
        char *escaped_path = temp_buffer + snprintf(temp_buffer, required_space, "AT+MHTTPREQUEST=%d,1,0,\"", httpid);
        for (const char *src = path; *src; src++)
        {
            if (*src == '"')
            { // 转义双引号
                *escaped_path++ = '\\';
                *escaped_path++ = '"';
            }
            else
            {
                *escaped_path++ = *src;
            }
        }
        *escaped_path++ = '"';
        *escaped_path++ = '\r';
        *escaped_path++ = '\n';
        *escaped_path = '\0'; // 终止符

        // Step 4: 验证总长度是否超出模块限制
        size_t final_len = strlen(temp_buffer);
        if (final_len > 512)
        { // 根据模块手册调整
            ESP_LOGE(TAG, "AT command exceeds module limit (%d > 512)", final_len);
            free(temp_buffer);
            return ESP_ERR_INVALID_SIZE;
        }
        config.cmd = temp_buffer;
        config.cmd_len = final_len;
        ESP_LOGI(TAG, "链接长度%s-字节%d-指令:%s", path, strlen(path), config.cmd);

        // config.cmd_len = 155;
        config.handler = handler_http;
        //     // config.handler = handler_http_client;
        config.timeout = 10000;
        ret = u4g_at_cmd_sync(&config);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (ret != U4G_OK)
        {
            ESP_LOGE(TAG, "HTTP客户端-发送HTTP请求-失败:%d", ret);
            return U4G_FAIL;
        }
    }
    else
    {
        // POST请求
        /**
         * @brief HTTP客户端-配置头部（对应 AT+MHTTPCFG="header",<httpid>,<header>）
         * @param httpid 整型，HTTP客户端实例id，0-3
         * @param header 字符串，HTTP头部，例如：Content-Type: application/json
         */
        config.name = U4G_AT_CMD_HTTP_HEADER;
        config.cmd_len = snprintf(config.cmd, 61, "AT+MHTTPCFG=\"header\",%u,\"%s\"\r\n", httpid, "Content-Type: application/json");
        config.timeout = 3000;
        config.handler = NULL;
        ret = u4g_at_cmd_sync(&config);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (ret != U4G_OK)
        {
            ESP_LOGE(TAG, "HTTP客户端-配置头部-失败:%d", ret);
            return U4G_FAIL;
        }

        /**
         * HTTP客户端-配置body参数（对应 AT+MHTTPCONTENT=<httpid>,<eof>,<length>,<data>）
         * <httpid> 整型，HTTP客户端实例id，0-3
         * <eof> 数据输入指示
         * -0-content输入结束标记，需请求完成或清空后才可再次输入
         * -1后续还有content数据输入
         * -2清空已有content内容，只对非Chunked模式有效
         * <length> 整型，body长度，0-4096
         * <data> 字符串，body参数，例如：{"key":"value"}
         */
        if ((strlen(body) + 61) > 512)
        {
            ESP_LOGE(TAG, "HTTP客户端-配置头部-超过512");
            return U4G_ERR_INVALID_SIZE;
        }
        config.name = U4G_AT_CMD_HTTP_BODY;
        config.cmd_len = snprintf(config.cmd, (strlen(body) + 61), "AT+MHTTPCONTENT=%d,0,0,\"%s\"\r\n", httpid, body);
        config.timeout = 3000;
        config.handler = NULL;
        ret = u4g_at_cmd_sync(&config);
        vTaskDelay(pdMS_TO_TICKS(100));
        if (ret != U4G_OK)
        {
            ESP_LOGE(TAG, "HTTP客户端-发送HTTP请求-失败:%d", ret);
            return U4G_FAIL;
        }

        /**
         * HTTP客户端-发送HTTP请求（对应 AT+MHTTPREQUEST=<httpid>,<method>[,<length>[,<path>[,<local_path>]]]）
         * <httpid> 整型，HTTP客户端实例id，0~3
         * <method> 整型，1-GET，2-POST，3-PUT，4-DELETE，5-HEAD
         * <path> 字符串，HTTP请求路径，例如：/api/device/info
         */
        config.name = U4G_AT_CMD_HTTP_REQUEST;

        // 1. 添加保护符号后的格式化字符串
        const char *format = "#AT+MHTTPREQUEST=%d,%d,0,\"%s\"#\r\n"; // 前后添加#
        // 2. 临时缓冲区（比目标大2倍，防止溢出）
        char temp_buffer[283 * 2] = {0};
        // 3. 使用snprintf生成带保护符号的指令
        int required_len = snprintf(temp_buffer, sizeof(temp_buffer),
                                    format, httpid, 2, path);
        // 4. 检查snprintf是否失败或溢出
        if (required_len < 0)
        {
            ESP_LOGE(TAG, "snprintf格式化失败");
            return U4G_FAIL;
        }
        if (required_len >= sizeof(temp_buffer))
        {
            ESP_LOGE(TAG, "临时缓冲区溢出! 需要%d字节", required_len + 1);
            return U4G_FAIL;
        }
        // 5. 查找保护符号位置
        char *start_ptr = strchr(temp_buffer, '#'); // 第一个#
        char *end_ptr = strrchr(temp_buffer, '#');  // 最后一个#
        // 6. 校验保护符号完整性
        if (!start_ptr || !end_ptr || start_ptr >= end_ptr)
        {
            ESP_LOGE(TAG, "保护符号#缺失或位置错误");
            ESP_LOG_BUFFER_HEXDUMP(TAG, temp_buffer, strlen(temp_buffer), ESP_LOG_ERROR);
            return U4G_FAIL;
        }
        // 7. 计算有效指令范围
        size_t cmd_len = end_ptr - start_ptr - 1; // 去掉两个#
        if (cmd_len >= 283)
        {
            ESP_LOGE(TAG, "有效指令超出缓冲区! 最大允许%d字节", 283 - 1);
            return U4G_FAIL;
        }
        // 8. 拷贝有效部分到目标缓冲区
        strncpy(config.cmd, start_ptr + 1, cmd_len); // 跳过第一个#
        config.cmd[cmd_len] = '\0';                  // 确保终止符

        // 9. 添加换行符
        strncat(config.cmd, "\r\n", sizeof(config.cmd) - strlen(config.cmd) - 1); // 添加换行符
        config.cmd_len = strlen(config.cmd);                                      // 更新长度
        // config.cmd_len = cmd_len;
        // 9. 调试日志（可选）
        ESP_LOGI(TAG, "原始指令: %s", temp_buffer);
        ESP_LOGI(TAG, "处理后指令: %s (长度=%d)", config.cmd, config.cmd_len);

        // config.cmd_len = snprintf(config.cmd, 283, "#AT+MHTTPREQUEST=%d,%d,0,\"%s#\"\r\n", httpid, 2, path);
        config.handler = handler_http;
        config.timeout = 10000;
        ret = u4g_at_cmd_sync(&config);
        if (ret != U4G_OK)
        {
            ESP_LOGE(TAG, "HTTP客户端-发送HTTP请求-失败:%d", ret);
            return U4G_FAIL;
        }
    }

    ret = u4g_at_http_client_del(httpid);
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "HTTP客户端-删除客户端-失败:%d", ret);
        return U4G_FAIL;
    }
    xSemaphoreGive(http_mutex);
    ESP_LOGI(TAG, "HTTP-请求执行完成...");
    return U4G_OK;
}