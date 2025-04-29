#include "u4g_uart_recv.h"
#include "u4g_at_cmd.h"
#include "esp_log.h"
#include <stdio.h>  //默认
#include <stddef.h> // 引入size_t、NULL类型
#include <string.h>

#define TAG "U4G-UART_RECV"

emU4GResult u4g_uart_recv_process(char *data, uint32_t size)
{
    if (data == NULL || size == 0)
    {
        ESP_LOGE(TAG, "接收数据为空或长度为0");
        return 0;
    }
    // int32_t res = (int32_t)size; // 初始化消费长度为接收长度
    ESP_LOGI(TAG, "接收到数据(%ld字节): %.*s", size, (int)size, data);

    if (u4g_at_cmd_handle.cmd_content == NULL)
    {
        ESP_LOGE(TAG, "无活动指令");
        return 0;
    }
    const u4g_at_cmd_t *cmd_content = u4g_at_cmd_handle.cmd_content;

    emU4GResult result = U4G_STATE_AT_RSP_WAITING;
    // 根据当前AT命令类型进行不同的处理
    switch (cmd_content->name)
    {
    case U4G_AT_CMD:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "AT 指令处理完成");
            result = U4G_OK;
        }
        else if (strstr(data, "ERROR\r\n"))
        {
            ESP_LOGE(TAG, "AT 指令 错误需重启");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGI(TAG, "AT 指令处理失败");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_IMEI:
    {
        char *pt = strstr(data, "+CGSN: ");
        if (pt)
        {
            emU4GResult handler_res = cmd_content->handler(data);
            if (handler_res == U4G_OK)
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_IMEI-指令回调成功");
                result = U4G_OK;
            }
            else
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_IMEI-指令回调失败");
                result = U4G_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "未找到 CGSN 字符串");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_CEREG:
    {
        char *pt = strstr(data, "+CEREG: ");
        if (pt)
        {
            emU4GResult handler_res = cmd_content->handler(data); // ✅
            if (handler_res == U4G_OK)
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_CEREG-联网成功");
                result = U4G_OK;
            }
            else if (handler_res == U4G_STATE_AT_NET_NOT)
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_CEREG-未联网");
                result = U4G_STATE_AT_NET_NOT;
            }
            else
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_CEREG-指令回调失败");
                result = U4G_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "未找到 CEREG 字符串");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_MCCID:
    {
        char *pt = strstr(data, "+MCCID: ");
        if (pt)
        {
            emU4GResult handler_res = cmd_content->handler(data); // ✅
            if (handler_res == U4G_OK)
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_MCCID-指令回调成功");
                result = U4G_OK;
            }
            else
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_MCCID-指令回调失败");
                result = U4G_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "未找到 MCCID 字符串");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_REBOOT:
    {
        result = U4G_OK;
        break;
    }
    case U4G_AT_CMD_TIME:
    {
        char *pt = strstr(data, "+CCLK: ");
        if (pt)
        {
            emU4GResult handler_res = cmd_content->handler(data); // ✅
            if (handler_res == U4G_OK)
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_TIME-指令回调成功");
                result = U4G_OK;
            }
            else
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_TIME-指令回调失败");
                result = U4G_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "未找到 CCLK 字符串");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_CSQ:
    {
        char *pt = strstr(data, "+CSQ: ");
        if (pt)
        {
            emU4GResult handler_res = cmd_content->handler(data); // ✅
            if (handler_res == U4G_OK)
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_CSQ-指令回调成功");
                result = U4G_OK;
            }
            else
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_CSQ-指令回调失败");
                result = U4G_FAIL;
            }
        }
        else
        {
            ESP_LOGE(TAG, "未找到 CSQ 字符串");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_ATE:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "ATE 指令处理完成");
            result = U4G_OK;
        }
        else if (strstr(data, "ERROR\r\n"))
        {
            ESP_LOGE(TAG, "ATE 指令 已执行");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGE(TAG, "ATE 指令 未找到");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_SSL_AUTH:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "U4G_AT_CMD_SSL_AUTH 指令处理完成");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGE(TAG, "U4G_AT_CMD_SSL_AUTH 指令 未找到");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_HTTP_CREATE:
    {
        char *pt = strstr(data, "+MHTTPCREATE:");
        if (pt)
        {
            emU4GResult handler_res = cmd_content->handler(data); // ✅
            if (handler_res == U4G_OK)
            {
                result = U4G_OK;
            }
            else
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_CREATE-无法解析HTTP实例ID");
                result = U4G_FAIL;
            }
        }
        else
        {
            pt = strstr(data, "+CME ERROR: ");
            if (pt)
            {
                pt += strlen("+CME ERROR: ");
                int err = 0;
                sscanf(pt, "%d", &err);
                if (err == 651)
                {
                    ESP_LOGE(TAG, "HTTP 客户端实例无空闲客户端");
                    result = U4G_STATE_AT_NO_CLIENT_IDLE;
                }
                else
                {
                    ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_CREATE-无法解析HTTP实例ID +CME ERROR");
                    result = U4G_FAIL;
                }
            }
            else
            {
                ESP_LOGI(TAG, "HTTP 客户端实例创建失败");
                result = U4G_FAIL;
            }
        }
        break;
    }
    case U4G_AT_CMD_HTTP_SSL:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_SSL 指令处理完成");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGE(TAG, "U4G_AT_CMD_HTTP_SSL 指令 未找到");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_HTTP_ENCODING:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_ENCODING 指令处理完成");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGE(TAG, "U4G_AT_CMD_HTTP_ENCODING 指令 未找到");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_HTTP_FRAGMENT:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_FRAGMENT 指令处理完成");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGE(TAG, "U4G_AT_CMD_HTTP_FRAGMENT 指令 未找到");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_HTTP_HEADER:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_HEADER 指令处理完成");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGE(TAG, "U4G_AT_CMD_HTTP_HEADER 指令 未找到");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_HTTP_BODY:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_BODY 指令处理完成");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGE(TAG, "U4G_AT_CMD_HTTP_BODY 指令 未找到");
            result = U4G_FAIL;
        }
        break;
    }
    case U4G_AT_CMD_HTTP_REQUEST:
    {
        static bool data_state = false;
        char *pt = strstr(data, "+MHTTPURC: \"content\"");
        if (pt)
        {
            data_state = true; // 检测到内容-首次处理
        }
        else if (strstr(data, "+MHTTPURC: \"err\"") || strstr(data, "+CME ERROR:"))
        {
            ESP_LOGE(TAG, "HTTP请求处理错误响应: %s", data);
            result = U4G_FAIL;
        }
        else if (strstr(data, "OK\r\n"))
        {
            ESP_LOGE(TAG, "继续接收指令");
            result = U4G_STATE_AT_RSP_WAITING;
        }
        else
        {
            // result = U4G_FAIL;
            if(data_state == false){
                ESP_LOGI(TAG, "HTTP请求未知数据");
            }
        }
        if (data_state)
        {
            emU4GResult handler_res = cmd_content->handler(data); // ✅
            if (handler_res == U4G_OK)
            {
                data_state = false; // 接收完毕，关闭数据处理
                result = U4G_OK;
            }
            else if (handler_res == U4G_STATE_AT_RSP_WAITING)
            {
                result = U4G_STATE_AT_RSP_WAITING;
            }
            else if (handler_res == U4G_STATE_AT_RINGBUF_OVERFLOW)
            {
                data_state = false; // 接收完毕，关闭数据处理
                result = U4G_STATE_AT_RINGBUF_OVERFLOW;
            }
            else
            {
                ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_REQUEST-无法解析HTTP内容");
                data_state = false; // 接收完毕，关闭数据处理
                result = U4G_FAIL;
            }
        }
        break;
    }
    case U4G_AT_CMD_HTTP_DELETE:
    {
        if (strstr(data, "OK\r\n"))
        {
            ESP_LOGI(TAG, "U4G_AT_CMD_HTTP_DELETE 指令处理完成");
            result = U4G_OK;
        }
        else
        {
            ESP_LOGE(TAG, "U4G_AT_CMD_HTTP_DELETE 指令 未找到");
            result = U4G_FAIL;
        }
        break;
    }
    default:
        ESP_LOGW(TAG, "默认的命令类型: %d", cmd_content->name);
        break;
    }
    if (result == U4G_STATE_AT_RSP_WAITING)
    { // 继续等待命令回执
        return U4G_STATE_AT_RSP_WAITING;
    }
    u4g_at_cmd_handle.result = result;
    xSemaphoreGive(u4g_at_cmd_semaphore_sync); // 释放信号量，通知消费者

    // if (result == U4G_OK)
    // {
    //     u4g_at_cmd_handle.state = AT_MSG;
    //     xEventGroupSetBits(at_event_group, AT_RESPONSE_BIT);
    // }
    // u4g_at_cmd_handle.state = AT_MSG;
    // xEventGroupSetBits(at_event_group, AT_RESPONSE_BIT);

    return result;
}

// 在 u4g_uart_recv_process 中触发缓冲区切换
// int32_t u4g_uart_recv_process(uint8_t *pdata, uint32_t size) {
//     // ...原有解析逻辑...

//     // 示例：HTTP请求数据完整接收后切换缓冲区
//     if (cmd_content->name == U4G_AT_CMD_HTTP_REQUEST) {
//         int buf_idx;
//         size_t len;
//         // 获取当前写入的缓冲区
//         if (u4g_at_buffer_write(&buf_idx, &len, 0)) {
//             memcpy(AT_Cache.data[buf_idx], data, size);
//             AT_Cache.len[buf_idx] = size;
//             u4g_at_buffer_switch(buf_idx); // 触发队列发送
//         }
//     }

//     return res;
// }

// int32_t u4g_uart_recv_process2(uint8_t *pdata, uint32_t size)
// {
//     if (pdata == NULL || size == 0)
//     {
//         ESP_LOGE(TAG, "接收数据为空或长度为0");
//         return 0;
//     }

//     int32_t res = (int32_t)size; // 初始化消费长度为接收长度

//     char *data = (char *)pdata;

//     // 先打印接收到的数据（调试用）
//     ESP_LOGI(TAG, "接收到数据(%ld字节): %.*s", size, (int)size, data);

//     bool data_res = false;
//     if (u4g_at_cmd_handle.cmd_content)
//     {
//         // // 特殊处理HTTP请求
//         if (u4g_at_cmd_handle.cmd_content->name == U4G_AT_CMD_HTTP_REQUEST)
//         {
//             ESP_LOGI(TAG, "检测到HTTP请求相关数据: %.*s", (int)size, data);
//             // ESP_LOGI(TAG, "检测到HTTP请求相关数据");
//             // 只有当收到内容或错误时才设置处理标志
//             if (strstr(data, "+MHTTPURC: \"content\""))
//             {
//                 ESP_LOGI(TAG, "检测到HTTP内容响应");
//                 data_res = true;
//             }
//             else if (strstr(data, "+MHTTPURC: \"err\"") || strstr(data, "+CME ERROR:"))
//             {
//                 ESP_LOGI(TAG, "检测到HTTP错误响应");
//                 data_res = true;
//             }
//             else if (strstr(data, "OK\r\n"))
//             {
//                 // OK只是命令接受确认，不是最终响应
//                 ESP_LOGI(TAG, "检测到HTTP请求命令确认(OK)，继续等待实际响应");
//                 // 不要直接清空原始数据，而是复制一份再处理
//                 // char local_buffer[128]; // 足够存储"OK"响应的小缓冲区
//                 // strncpy(local_buffer, data, sizeof(local_buffer) - 1);
//                 // local_buffer[sizeof(local_buffer) - 1] = '\0';
//                 // // 只设置状态，不清空数据
//                 // at_result_data.state = AT_MSG_PARTIAL; // 添加一个新状态表示部分响应
//                 // memset(data, 0, U4G_UART_RX_BUF_SIZE);
//             }
//             // else if (strstr(data, "OK\r\n") && !strstr(data, "+MHTTPURC:"))
//             // {
//             //     // 如果只收到OK且没有其他HTTP相关内容，仅记录日志但不触发完成                ESP_LOGI(TAG, "HTTP请求已被接受，等待数据返回");
//             //     // 不要清空，保留响应
//             // }
//             else if (strstr(data, "+MHTTPURC: \"header\""))
//             {
//                 // 收到HTTP头部，可以考虑设置超时时间
//                 ESP_LOGI(TAG, "检测到HTTP头部信息，继续等待实际响应");
//                 memset(data, 0, U4G_UART_RX_BUF_SIZE);
//             }
//             else if (strstr(data, "+MHTTPURC:"))
//             {
//                 // 任何其他MHTTPURC响应可能也包含有效数据
//                 ESP_LOGI(TAG, "检测到其他HTTP相关响应");
//                 data_res = true;
//             }
//             else if (size > 20 && strstr(data, "{") && strstr(data, "}"))
//             {
//                 // 可能是JSON数据
//                 ESP_LOGI(TAG, "可能收到直接的JSON数据");
//                 data_res = true;
//             }
//             else
//             {
//                 // ESP_LOGW(TAG, "未识别的HTTP响应内容: %.*s", (int)size, data);
//                 ESP_LOGW(TAG, "未识别的HTTP响应内容，继续等待");
//                 memset(data, 0, U4G_UART_RX_BUF_SIZE);
//             }
//         }
//         else
//         {
//             // 普通AT命令处理
//             if (strstr(data, "OK\r\n") || strstr(data, "ERROR\r\n") ||
//                 strstr(data, "+CEREG: ") || strstr(data, "+CCLK: ") ||
//                 strstr(data, "+MQTTSTATE:") || strstr(data, "+MCCID:") || strstr(data, "+CME ERROR:"))
//             {
//                 ESP_LOGI(TAG, "检测到普通指令[%s]响应：%.*s", at_cmd_current->cmd, (int)size, data);
//                 data_res = true;
//             }
//             else
//             {
//                 ESP_LOGI(TAG, "未知响应内容，继续接收，缓冲区内容(%ld字节): %.*s", size, (int)size, data);
//                 memset(data, 0, U4G_UART_RX_BUF_SIZE);
//             }
//         }

//         // 处理有效响应
//         if (data_res)
//         {
//             ESP_LOGI(TAG, "处理有效AT响应");
//             receive_process_data(data, res);

//             at_result_data.state = AT_MSG; // 有数据

//             //     // 设置事件通知
//             //     // if (at_event_group != NULL)
//             //     // {
//             //     //     xEventGroupSetBits(at_event_group, AT_RESPONSE_BIT);
//             //     //     ESP_LOGI(TAG, "已设置AT响应事件位");
//             //     // }
//             //     // else
//             //     // {
//             //     //     ESP_LOGE(TAG, "事件组句柄为NULL，无法设置事件位");
//             //     // }

//             //     // xSemaphoreGive(AT_CALLBACK_SIGNAL); // 同步响应
//             //     // 修改：使用任务通知代替信号量

//             memset(data, 0, U4G_UART_RX_BUF_SIZE);
//         }
//     }
//     else
//     {
//         ESP_LOGW(TAG, "收到数据但当前无活动AT命令");
//     }

//     ESP_LOGD(TAG, "u4g_uart_recv_process-返回处理后的数据长度: %ld", res);
//     return 1; // 返回处理后的数据长度
// }

// static emATResult receive_process_data(void *pdata, size_t plen)
// {
//     if (pdata == NULL)
//     {
//         ESP_LOGE(TAG, "接收到的数据指针为NULL");
//         return AT_ERR_UNKNOWN;
//     }

//     // 处理数据的实现
//     char *data = (char *)pdata;
//     ESP_LOGI(TAG, "receive_process_data接收到数据：%s", data);

//     emATResult result = AT_OK;

//     if (at_cmd_current == NULL)
//     {
//         ESP_LOGW(TAG, "receive_process_data当前AT命令为空");
//         return AT_ERR_UNKNOWN;
//     }

//     // 根据当前AT命令类型进行不同的处理
//     switch (at_cmd_current->cmdName)
//     {
//     case CMD_AT:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "AT 指令处理完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "AT 指令处理失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_REBOOT:
//     {
//         at_result_data.result = AT_OK;
//         break;
//     }
//     case CMD_CSQ:
//     {
//         char *pt = strstr(data, "+CSQ: "); // 移动指针到数据部分
//         if (pt)
//         {
//             pt += strlen("+CSQ: ");

//             // 使用 sscanf 提取第一个数字（rssi）
//             int rssi = 0;
//             if (sscanf(pt, "%d,", &rssi) == 1)
//             {
//                 // 安全地存储结果
//                 if (at_result_data.data != NULL && at_result_data.data_len >= sizeof(int))
//                 {
//                     *((int *)at_result_data.data) = rssi;
//                     ESP_LOGI(TAG, "解析到CSQ值: %d", rssi);
//                     at_result_data.result = AT_OK;
//                 }
//                 else
//                 {
//                     ESP_LOGE(TAG, "CSQ结果缓冲区无效");
//                     at_result_data.result = AT_FAIL;
//                 }
//             }
//             else
//             {
//                 ESP_LOGE(TAG, "字符串格式不正确，无法解析 CSQ 数据");
//                 at_result_data.result = AT_FAIL;
//             }

//             // int pt_len = strlen(pt); // 计算数据长度
//             // if (at_result_data.data_len > 0 && pt_len > at_result_data.data_len)
//             // {
//             //     ESP_LOGD(TAG, "CSQ 超出传递size");
//             //     at_result_data.result = AT_FAIL;
//             //     break;
//             // }
//             // // 使用 sscanf 提取第一个数字（rssi）
//             // if (sscanf(pt, "%d,%*d", (int *)at_result_data.data) == 1)
//             // {
//             //     at_result_data.result = AT_OK;
//             //     break;
//             // }
//             // ESP_LOGE(TAG, "字符串格式不正确，无法解析 CSQ 数据");
//         }
//         else
//         {
//             ESP_LOGE(TAG, "未找到 +CSQ: 字符串");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_CEREG:
//     {
//         char *pt = strstr(data, "+CEREG: ");
//         if (pt)
//         {
//             pt += strlen("+CEREG: "); // 添加这行，跳过前缀
//             int n = 0, stat = 0;
//             // 修正：匹配格式为"数字,数字"
//             if (sscanf(pt, "%d,%d", &n, &stat) == 2)
//             {
//                 ESP_LOGI(TAG, "CEREG: n=%d, stat=%d", n, stat);
//                 if (stat == 1 || stat == 5) // 1: 已注册，本地网络; 5: 已注册，漫游
//                 {
//                     at_result_data.result = AT_OK;
//                 }
//                 else if (stat == 0)
//                 { // 未联网
//                     at_result_data.result = AT_NET_NOT;
//                 }
//                 else
//                 {
//                     at_result_data.result = AT_FAIL;
//                 }
//             }
//             else
//             {
//                 ESP_LOGE(TAG, "字符串格式不正确，无法解析 CEREG 数据: [%s]", pt);
//                 at_result_data.result = AT_FAIL;
//             }
//         }
//         else
//         {
//             ESP_LOGE(TAG, "未找到 CEREG 字符串");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_TIME:
//     {
//         char *pt = strstr(data, "+CCLK: ");
//         if (pt)
//         {
//             pt += strlen("+CCLK: ");
//             // 示例响应: +CCLK: "24/12/23,03:18:05+32"
//             const char *start = strchr(pt, '\"');
//             if (!start)
//             {
//                 ESP_LOGE(TAG, "无法找到引号，解析失败");
//                 at_result_data.result = AT_FAIL;
//                 break;
//             }
//             start++; // 移动到引号后的第一个字符

//             // 查找第二个引号
//             const char *end = strchr(start, '\"');
//             if (!end)
//             {
//                 ESP_LOGE(TAG, "无法找到结束引号，解析失败");
//                 at_result_data.result = AT_FAIL;
//                 break;
//             }

//             // 提取时间字符串
//             size_t len = end - start;
//             if (at_result_data.data == NULL || len >= at_result_data.data_len)
//             {
//                 ESP_LOGE(TAG, "时间字符串过长或缓冲区无效");
//                 at_result_data.result = AT_FAIL;
//                 break;
//             }

//             strncpy((char *)at_result_data.data, start, len);
//             ((char *)at_result_data.data)[len] = '\0';
//             at_result_data.result = AT_OK;
//             ESP_LOGI(TAG, "解析后的时间字符串: %s", (char *)at_result_data.data);
//         }
//         else
//         {
//             ESP_LOGE(TAG, "未找到 解析后的时间字符串");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_ATE:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "ATE 指令处理完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGE(TAG, "ATE 指令 未找到");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_IMEI:
//     {
//         // +CGSN: 869951040379202\r\nOK\r\n
//         char *pt = strstr(data, "+CGSN:");
//         if (pt)
//         {
//             pt += strlen("+CGSN:");
//             int pt_len = strlen(pt); // 计算数据长度
//             if (at_result_data.data_len > 0 && pt_len > at_result_data.data_len)
//             {
//                 ESP_LOGD(TAG, "CGSN 超出传递size");
//                 at_result_data.result = AT_FAIL;
//                 break;
//             }
//             sscanf(pt, "%17s", (char *)at_result_data.data);
//             ESP_LOGI(TAG, "IMEI接收数据 %s | 接收字节数 %d | 传递包大小 %d", (char *)at_result_data.data, plen, at_result_data.data_len);
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGE(TAG, "未找到 CGSN 字符串");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_MCCID: // 读取ICCID(AT+MCCID)SIM
//     {
//         char *pt = strstr(data, "+MCCID: ");
//         if (pt)
//         {
//             pt += strlen("+MCCID: ");
//             int pt_len = strlen(pt); // 计算数据长度

//             if (at_result_data.data_len > 0 && pt_len > at_result_data.data_len)
//             {
//                 ESP_LOGD(TAG, "MCCID 超出传递size");
//                 at_result_data.result = AT_FAIL;
//                 break;
//             }
//             // 使用 sscanf 读取数据
//             sscanf(pt, "%s", (char *)at_result_data.data);
//             ESP_LOGI(TAG, "MCCID = %s", (char *)at_result_data.data);
//             at_result_data.result = AT_OK;
//             break;
//         }
//         pt = strstr(data, "+CME ERROR: ");
//         if (pt)
//         {
//             pt += strlen("+CME ERROR: ");
//             sscanf(pt, "%s", (char *)at_result_data.data);
//             ESP_LOGI(TAG, "MCCID-ERROR: %s", (char *)at_result_data.data);
//             if (strcmp((char *)at_result_data.data, "10") == 0)
//             {
//                 // 如果 out_data 指向的字符串是 "10"
//                 ESP_LOGE(TAG, "读取MCCID(SIM)不存在未插卡");
//                 at_result_data.result = AT_ERR_MCCID_NOT; // 读取SIM不存在
//             }
//             else
//             {
//                 ESP_LOGE(TAG, "读取MCCID(SIM)失败 错误码: %s", (char *)at_result_data.data);
//                 at_result_data.result = AT_ERR_MCCID;
//             }
//             break;
//         }

//         ESP_LOGE(TAG, "未找到 MCCID 字符串或错误信息");
//         at_result_data.result = AT_FAIL;
//         break;
//     }
//     case CMD_SSL_AUTH:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "CMD_SSL_AUTH 指令处理完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "CMD_SSL_AUTH 指令处理失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_HTTP_CREATE:
//     {
//         char *pt = strstr(data, "+MHTTPCREATE:");
//         if (pt)
//         {
//             pt += strlen("+MHTTPCREATE: ");
//             int pt_len = strlen(pt); // 计算数据长度
//             if (at_result_data.data_len > 0 && pt_len > at_result_data.data_len)
//             {
//                 ESP_LOGD(TAG, "MHTTPCREATE 超出传递size");
//                 at_result_data.result = AT_FAIL;
//                 break;
//             }

//             // sscanf(pt, "+MHTTPCREATE: %hhu", &AT_Info.httpid);
//             // 修正格式，移除了重复的"+MHTTPCREATE: "
//             if (sscanf(pt, "%hhu", &AT_Info.httpid) == 1)
//             {
//                 ESP_LOGI(TAG, "HTTP 客户端实例 ID = %hhu", AT_Info.httpid);
//                 at_result_data.result = AT_OK;
//             }
//             else
//             {
//                 ESP_LOGE(TAG, "无法解析HTTP实例ID");
//                 at_result_data.result = AT_FAIL;
//             }
//         }
//         else
//         {
//             pt = strstr(data, "+CME ERROR: ");
//             if (pt)
//             {
//                 pt += strlen("+CME ERROR: ");
//                 int err = 0;
//                 sscanf(pt, "%d", &err);
//                 if (err == 651)
//                 {
//                     at_result_data.result = AT_NO_CLIENT_IDLE;
//                     ESP_LOGE(TAG, "HTTP 客户端实例无空闲客户端");
//                 }
//                 else
//                 {
//                     at_result_data.result = AT_FAIL;
//                 }
//             }
//             else
//             {
//                 ESP_LOGI(TAG, "HTTP 客户端实例创建失败");
//                 at_result_data.result = AT_FAIL;
//             }
//         }
//         break;
//     }
//     case CMD_HTTP_HEADER:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_HEADER 配置完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_HEADER 配置失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_HTTP_SSL:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "HTTP SSL配置完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "HTTP SSL配置失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_HTTP_ENCODING:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_ENCODING 配置完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_ENCODING 配置失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_HTTP_TIMEOUT:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_TIMEOUT 配置完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_TIMEOUT 配置失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_HTTP_FRAGMENT:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_FRAGMENT 配置完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_FRAGMENT 配置失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_HTTP_BODY:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_BODY 配置完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_BODY 配置失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_HTTP_REQUEST:
//     {
//         char *pt = strstr(data, "+MHTTPURC: \"content\"");
//         if (pt)
//         {
//             pt += strlen("+MHTTPURC: \"content\"");
//             // int pt_len = strlen(pt); // 计算数据长度
//             ESP_LOGI(TAG, "CMD_HTTP_REQUEST 指令 HTTP请求成功");

//             // 格式: +MHTTPURC: "content",<httpid>,<content_len>,<sum_len>,<cur_len>,<data>
//             int httpid = 0, content_len = 0, sum_len = 0, cur_len = 0;
//             int offset = 0;
//             // 修正: 解析格式，跳过前缀后应该匹配逗号和四个数字
//             if (sscanf(pt, ",%d,%d,%d,%d,%n", &httpid, &content_len, &sum_len, &cur_len, &offset) == 4)
//             {
//                 if (offset > 0 && offset < strlen(pt))
//                 {
//                     // 计算数据起始位置并安全地复制数据
//                     const char *data_ptr = pt + offset;
//                     size_t data_len = strlen(data_ptr);

//                     if (at_result_data.data != NULL && at_result_data.data_len > 0)
//                     {
//                         size_t copy_len = (data_len < at_result_data.data_len - 1) ? data_len : (at_result_data.data_len - 1);

//                         strncpy((char *)at_result_data.data, data_ptr, copy_len);
//                         ((char *)at_result_data.data)[copy_len] = '\0'; // 添加字符串终止符

//                         ESP_LOGI(TAG, "提取的HTTP数据: %s", (char *)at_result_data.data);
//                         at_result_data.result = AT_OK;
//                     }
//                     else
//                     {
//                         ESP_LOGE(TAG, "HTTP数据缓冲区无效");
//                         at_result_data.result = AT_FAIL;
//                     }
//                 }
//                 else
//                 {
//                     ESP_LOGE(TAG, "HTTP数据偏移量无效: %d", offset);
//                     at_result_data.result = AT_FAIL;
//                 }
//             }
//             else
//             {
//                 ESP_LOGE(TAG, "字符串格式不正确，无法解析HTTP数据: [%s]", pt);
//                 at_result_data.result = AT_FAIL;
//             }

//             // // 使用 sscanf 解析固定格式的字符串，%n 用于获取剩余字符串的位置
//             // int offset = 0;
//             // // 修正sscanf格式，因为已经跳过了前缀
//             // if (sscanf(pt, ",%d,%d,%d,%d,%n", &httpid, &content_len, &sum_len, &cur_len, &offset) != 4)
//             // {
//             //     ESP_LOGE(TAG, "字符串格式不正确，无法解析 HTTP data");
//             //     at_result_data.result = AT_FAIL;
//             //     break;
//             // }

//             // // 计算数据起始位置并复制数据
//             // const char *data_ptr = pt + offset;
//             // size_t data_len = strlen(data_ptr);
//             // size_t copy_len = (data_len < at_result_data.data_len - 1) ? data_len : (at_result_data.data_len - 1);
//             // strncpy((char *)at_result_data.data, data_ptr, copy_len);
//             // ((char *)at_result_data.data)[copy_len] = '\0'; // 手动添加字符串终止符

//             // ESP_LOGI(TAG, "提取的data: %s", (char *)at_result_data.data);
//             // at_result_data.result = AT_OK;
//         }
//         else if (strstr(data, "+MHTTPURC: \"err\"") || strstr(data, "+CME ERROR:"))
//         {
//             ESP_LOGE(TAG, "HTTP请求处理错误响应: %s", data);
//             at_result_data.result = AT_FAIL;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "HTTP请求未知数据，内容: %s", data);
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     case CMD_HTTP_DELETE:
//     {
//         if (strstr(data, "OK\r\n"))
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_DELETE 配置完成");
//             at_result_data.result = AT_OK;
//         }
//         else
//         {
//             ESP_LOGI(TAG, "CMD_HTTP_DELETE 配置失败");
//             at_result_data.result = AT_FAIL;
//         }
//         break;
//     }
//     default:
//         ESP_LOGW(TAG, "默认的命令类型: %d", at_cmd_current->cmdName);
//         break;
//     }
//     return result;
// }