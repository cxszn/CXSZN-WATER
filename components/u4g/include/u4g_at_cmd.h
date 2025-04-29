#ifndef U4G_AT_CMD_H
#define U4G_AT_CMD_H

#include <stddef.h> // 引入size_t、NULL类型
#include <stdint.h>
#include "u4g_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "u4g_core.h"

extern SemaphoreHandle_t u4g_at_cmd_semaphore_sync;

/**
 * @brief 接受到应答数据后的用户处理回调函数原型定义
 * @param[in] rsp       AT命令的应答数据
 * @return rsp_result_t
 */
typedef emU4GResult (*u4g_at_rsp_handler_t)(char *rsp);
// 定义AT指令枚举
typedef enum
{
    U4G_AT_CMD,               // 发送AT指令
    U4G_AT_CMD_CEREG,         // 网络注册状态查询
    U4G_AT_CMD_REBOOT,        // 重启4G模块-未开发
    U4G_AT_CMD_IMEI,          // 获取IMEI编号
    U4G_AT_CMD_ATE,           // 回显：0 关闭 1 开启
    U4G_AT_CMD_SSL_AUTH,      // SSL认证方式(默认无需认证)
    U4G_AT_CMD_HTTP_CREATE,   // HTTP客户端-创建HTTP客户端实例（对应 AT+MHTTPCREATE）
    U4G_AT_CMD_HTTP_HEADER,   // HTTP客户端-配置头部（对应 AT+MHTTPCFG="header",<httpid>,<header>）
    U4G_AT_CMD_HTTP_TIMEOUT,  // HTTP客户端-设置超时时间（AT+MHTTPCFG="timeout"[,<httpid>[,<conn_timeout>[,<rsp_timeout>[,<input_timeout>]]]]）
    U4G_AT_CMD_HTTP_ENCODING, // HTTP客户端-设置输入输出编码（AT+MHTTPCFG="encoding"[,<httpid>[,<input_format>[,<output_format>]]]）
    U4G_AT_CMD_HTTP_SSL,      // HTTP客户端-SSL模式设置（对应 AT+MHTTPCFG）
    U4G_AT_CMD_HTTP_FRAGMENT, // HTTP客户端-设置数据输出流控（AT+MHTTPCFG="fragment"[,<httpid>[,<frag_size>[,<interval>]]]）
    U4G_AT_CMD_HTTP_BODY,     // HTTP客户端-设置HTTP CONTENT数据
    U4G_AT_CMD_HTTP_REQUEST,  // HTTP客户端-发送HTTP请求（对应 AT+MHTTPREQUEST=<httpid>,<method>[,<length>[,<path>[,<local_path>]]]）
    U4G_AT_CMD_HTTP_DELETE,   // HTTP客户端-实例删除（对应 AT+MHTTPDEL=<httpid>）
    U4G_AT_CMD_TIME,          // NTP网络时间获取
    U4G_AT_CMD_MCCID,         // 读取ICCID(AT+MCCID)SIM卡号
    U4G_AT_CMD_CSQ,           // AT+CSQ信号值
} emATCmd;

typedef struct
{
    emATCmd name;                 // AT别名
    char *cmd;                    // 要发送的AT命令
    size_t cmd_len;               // AT命令的数据长度，不需要用户赋值
    uint32_t timeout;             // 得到应答的超时时间，达到超时时间为执行失败， 默认10000，即10秒
    u4g_at_rsp_handler_t handler; // 接受到应答数据后的用户自定义处理回调函数
} u4g_at_cmd_t;
/**
 * @brief AT组件上下文数据结构体
 * 这是AT组件的主要上下文结构，包含了AT模块的所有状态和配置信息。
 */
typedef struct
{
    const u4g_at_cmd_t *cmd_content; // 当前正在处理的AT命令
    // volatile emATMsg state;          // 消息状态
    emU4GResult result;              // 命令处理结果
} u4g_at_cmd_handle_t;
extern u4g_at_cmd_handle_t u4g_at_cmd_handle;


emU4GResult u4g_at_cmd_init(void);
emU4GResult u4g_at_cmd_sync(const u4g_at_cmd_t *config);
emU4GResult u4g_at(void);
emU4GResult u4g_at_imei_get(void);
emU4GResult u4g_at_netstatus(void);
emU4GResult u4g_at_time_get(void);
emU4GResult u4g_at_mccid_get(void);
emU4GResult u4g_at_reboot_soft(void);
emU4GResult u4g_at_reboot_hard(void);
emU4GResult u4g_at_csq(void);

#endif