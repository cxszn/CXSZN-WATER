#ifndef _U4G_STATE_H_
#define _U4G_STATE_H_

#define U4G_UART_RX_BUF_SIZE (1024) // RX 接收缓冲区大小
#define AT_RESPONSE_BIT (1 << 0)    // 全局事件组标志位

// 状态码用于表示AT模块内部的各种状态和错误情况
typedef enum
{
    /* 通用状态码 */
    U4G_OK = 0,                 // API执行成功
    U4G_FAIL = -1,              // 通用错误
    U4G_ERR_INVALID_ARG = -2,   // 参数无效/非法的空指针
    U4G_ERR_TIMEOUT = -3,       // 操作超时
    U4G_ERR_INVALID_SIZE = -4,  // 无效长度
    U4G_ERR_INVALID_STATE = -5, // 无效状态

    /** AT发送状态错误码 */
    U4G_STATE_AT_UART_TX_FAILED = (-4113),  // 表示UART发送失败
    U4G_STATE_AT_UART_TX_TIMEOUT = (-4106), // 表示发送超时
    U4G_STATE_AT_GET_RSP_FAILED = (-4102),  // 表示获取响应失败

    /* HTTP模块相关状态码 (-0x0400 ~ -0x04FF, 即 -1024 ~ -1279) */
    // U4G_STATE_HTTP_REQUEST_INVALID_ARG = -1025,     // HTTP请求参数无效
    // U4G_STATE_HTTP_NOT_CLIENT = -1024, // 无空闲客户端
    U4G_STATE_AT_NO_CLIENT_IDLE = -1024, // 无空闲客户端
    U4G_STATE_AT_NET_NOT,                // 未驻网/未联网

    // U4G_STATE_AT_ALREADY_INIT = (-4099),       // 表示AT模块已初始化
    // U4G_STATE_SYS_DEPEND_MALLOC_FAILED = -513, // 申请内存失败 (-0x0201)
    U4G_STATE_AT_RINGBUF_OVERFLOW = (-4103),   // 表示环形缓冲区溢出

    /** AT回调相关 */
    U4G_STATE_AT_RSP_WAITING = (-2000), // 继续等待命令回执
    // U4G_STATE_AT_

    U4G_STATE_AT_BUILDER_MEM_FAIL = (-3000),
    U4G_STATE_AT_BUILDER_OVERFLOW = (-3001),

} emU4GResult;

typedef enum
{
    AT_OK = 0,
    AT_FAIL = -1,
    AT_ERROR_RESPONSE,       // AT错误响应
    AT_ERROR_NO_RESPONSE,    // 无回应/超时
    AT_ERROR_NOT_RECOGNIZED, // 无法识别的回应字符
    // AT_ERROR_NOT_READY,      ///< 设备未准备好，无法发送指令
    // AT_ERROR_MAX_RETRIES,    ///< 达到最大重试次数，指令发送失败
    // AT_ERROR_TIMEOUT         ///< 超时
    AT_INFO_INCOMPLETE, // 消息还未完成，继续接收
    // AT_HTTP_CLIENT_NO_IDLE, // HTTP 客户端无空闲
    AT_ERR_NOT_ALLOWED,           // 不允许操作
    AT_ERR_NO_MEM,                // 内存分配失败
    AT_ERR_INVALID_ARG,           // 无效参数/参数错误
    AT_ERR_UNKNOWN,               // 未知错误
    AT_NO_CLIENT_IDLE,            // 无空闲客户端
    AT_NO_CLIENT_CREATE,          // 客户端未创建
    AT_CLIENT_BUSY,               // 客户端忙
    AT_ERR_URL_PARSING,           // URL解析失败
    AT_NOT_SSL_ENABLED,           // SSL未使能
    AT_ERR_LINK,                  // 连接失败
    AT_ERR_DATA_SEND,             // 数据发送失败
    AT_ON_FILE_FAIL,              // 打开文件失败
    AT_WRITE_FILE_FAIL,           // 写入文件失败
    AT_DATA_LOSS,                 // 数据丢包
    AT_CACHE_SPACE_LACK,          // 缓存空间不足
    AT_RECEIVE_DATA_PARSING_FAIL, // 接收数据解析失败
    AT_LINK_ABNORMAL_OFF,         // 连接异常断开
    AT_SSL_SHAKE_FAIL,            // SSL握手失败
    AT_ERROR_INVALID_ARG,         // 无效的缓冲区或长度
    AT_NET_NOT,                   // 未驻网/未联网
    AT_ERR_MCCID,                 // 读取SIM失败
    AT_ERR_MCCID_NOT,             // 读取SIM不存在/未插卡
    AT_ERR_MODULE_ERROR,          // 4G模块异常
    // AT_ERR_MQTT_CONNECT_OFFLINE, //
} emATResult;

#include <stdbool.h>

// extern bool task_wdt_reset_send;

#endif