#ifndef HEAD_H // 检查是否未定义
#define HEAD_H // 如果未定义，则定义

#include "driver/gpio.h" // 制水GPIO配置针脚声明
#include "driver/uart.h" // 包含 UART 驱动头文件

// ------- 联网相关信息 -------
// HTTP相关配置
#define HTTP_URL "https://api.iot.zsyxlife.cn"

// 制水GPIO配置
#define CONFIG_WATER_GPIO_NUM GPIO_NUM_4      // 制水接口
#define CONFIG_FLUSH_GPIO_NUM GPIO_NUM_5      // 冲洗阀接口
#define CONFIG_HIGH_GPIO_NUM GPIO_NUM_6       // 高压接口
#define CONFIG_LOW_GPIO_NUM GPIO_NUM_7        // 低压接口
// 第四版启用
// #define CONFIG_LEAK_GPIO_NUM GPIO_NUM_10      // 漏水接口-输入信号
// 第五版启用
#define CONFIG_LEAK_GPIO_NUM GPIO_NUM_47      // 漏水接口-输入信号

#define CONFIG_BUZZER_GPIO_NUM GPIO_NUM_21    // 蜂鸣器接口-输出
// 第四版启用
// #define CONFIG_FLOWMETER_GPIO_NUM GPIO_NUM_37 // 流量计
// 第五版启用
#define CONFIG_FLOWMETER_GPIO_NUM GPIO_NUM_38 // 流量计

#define CONFIG_BLACKOUT_GPIO_NUM GPIO_NUM_8   // 断电保存

// TDS 配置信息
#define CONFIG_TDS_UART_NUM UART_NUM_2
#define CONFIG_TDS_UART_BAUD_RATE 9600             // 波特率
#define CONFIG_TDS_UART_TX_PIN GPIO_NUM_1          // TX 发送引脚
#define CONFIG_TDS_UART_RX_PIN GPIO_NUM_2          // RX 接收引脚
#define CONFIG_TDS_UART_RTS_PIN UART_PIN_NO_CHANGE // RTS 引脚
#define CONFIG_TDS_UART_CTS_PIN UART_PIN_NO_CHANGE // CTS 引脚
#define CONFIG_TDS_UART_RX_BUF_SIZE (512)          // RX 接收缓冲区大小
#define CONFIG_TDS_UART_TX_BUF_SIZE (512)          // TX 发送缓冲区大小
#define CONFIG_TDS_UART_QUEUE_SIZE (5)             // 事件队列数量

// 系统配置
#define CONFIG_PROJECT_NAME "CXSZN-WATER"

typedef enum
{
    NOT_READY = 0, // 未就绪
    READY_RUN,     // 就绪中
    READY,         // 已就绪
    // OTA_UPDATE     // 版本更新中
} run_state_t;
typedef enum
{
    DEVICE_NETOFF = 0, // 未联网
    DEVICE_NETRUN,     // 联网中
    DEVICE_NETON       // 已联网
} network_t;
typedef enum
{
    EXPIRY = 0, // 已到期
    NOT_EXPIRY, // 未到期
} mode_expire_t;
// typedef enum
// {
//     NOT_EXISTS = 0, // 不存在
//     SINGLE_CHANNEL, // TDS单通道
//     DUAL_CHANNEL,   // TDS双通道
// } tds_channel_t;

// .noinit 段用于存储需要在系统重启时保留的变量。这些变量不会被初始化，因此它们的值在系统重启后仍然保持不变。通常用于保存状态机、配置等信息，但是断电后会丢失
// __attribute__((section(".noinit"))) int noInitVar;

typedef struct
{
    run_state_t RUNSTATE; // 运行状态
    network_t NETSTATE;   // 网络状态
    // tds_channel_t TDS_EXISTS; // TDS设备
    uint8_t charging;          // 计费模式 0永久 1计时
    mode_expire_t MODE_EXPIRY; // 设备到期模式
    char MCCID[32];            // 设备MCCID
    char IMEI[25];             // 设备IMEI-设备ID
    // char KEY[64];   // 设备秘钥
    uint64_t total_water_time;   // 累计制水时间
    bool WATER_LEAK;             // 设备状态
    volatile uint32_t flowmeter; // 流量计
    int32_t expire_time;         // 到期时间
    uint32_t duration_s;         // 步长(秒)
    uint8_t signal;              // 信号值
    // device_status_t STATUS; // 设备状态
} device_info_t;
extern device_info_t DEVICE;
// volatile
typedef struct
{
    bool feedback_init; // 反馈初始化标志
} device_status_t;
extern device_status_t DEVICE_STATUS; // 设备状态

typedef struct
{
    uint8_t pure_tds;       // 纯水TDS值
    float pure_temperature; // 纯水温度值
    uint8_t raw_tds;        // 原水TDS值
    float raw_temperature;  // 原水温度值
} device_tds_wd_t;
extern device_tds_wd_t DEVICE_TDSWD;

extern bool is_isr_service_installed; // 声明变量并初始化为false-安装中断服务标志

#endif