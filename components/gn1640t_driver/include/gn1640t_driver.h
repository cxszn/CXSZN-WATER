#ifndef _GN1640T_DRIVER_H_
#define _GN1640T_DRIVER_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
// #include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <wchar.h>
/**
 * @brief GN1640T 驱动组件
 *
 * 该组件用于控制家用净水器中的 LED 显示，基于 GN1640T 芯片和 ESP32-S3 开发。
 *
 * 功能包括：
 * - 初始化 GN1640T 芯片
 * - 清空显示 RAM
 * - 设置显示亮度
 * - 控制不同功能的 LED 显示
 * - 管理 SEG1、SEG2、SEG3 的状态，并合并至 GRID
 */

// GN1640T 配置信息-需编辑
#define CONFIG_GN1640T_CLK_GPIO GPIO_NUM_12       // CLK连接的GPIO
#define CONFIG_GN1640T_DATA_GPIO GPIO_NUM_11      // DATA连接的GPIO
#define CONFIG_GN1640T_PULL_UP GPIO_PULLUP_ENABLE // 启用上拉电阻
#define CONFIG_GN1640T_DISPLAY_BRIGHTNESS 8       // LED亮度设置（0-8级）
#define CONFIG_GN1640T_DEBUG 0                    // 调试模式 1开启 0关闭

// 错误码
typedef enum
{
    GN1640T_OK = 0,               // 成功
    GN1640T_ERR_GPIO_INVALID_ARG, // GPIO错误无效参数
    GN1640T_ERR_SEND,             // 发送失败
    GN1640T_ERR_INVALID_SIZE,     // 无效大小
                              // GN1640T_ERR_NOT_INITED,       // 驱动未初始化
    GN1640T_ERR_MUTEX, // 互斥锁创建失败
} gn1640t_err_t;

// 控制命令类型
typedef enum
{
    GN1640T_CTRL_DATA = 0x40,    // 数据控制命令
    GN1640T_CTRL_DISPLAY = 0x80, // 显示控制命令
    GN1640T_CTRL_ADDRESS = 0xC0, // 地址控制命令
} gn1640t_ctrl_t;

// 数据模式定义
typedef enum
{
    GN1640T_DATA_ADDRESS_AUTOINCREMENT = 0x00,      // 地址自加1模式-0000
    GN1640T_DATA_ADDRESS_FIXED = 0x04,              // 固定地址模式-0100
    GN1640T_DATA_TEST_ADDRESS_AUTOINCREMENT = 0x08, // 测试(内部使用)-地址自加1模式-1000
    GN1640T_DATA_TEST_ADDRESS_FIXED = 0x0C,         // 测试(内部使用)-固定地址模式-1100
} gn1640t_data_mode_t;

// GRID 显示地址定义
typedef enum
{
    GN1640T_GRID1 = 0x00,
    GN1640T_GRID2 = 0x01,
    GN1640T_GRID3 = 0x02,
    GN1640T_GRID4 = 0x03,
    GN1640T_GRID5 = 0x04,
    GN1640T_GRID6 = 0x05,
    GN1640T_GRID7 = 0x06,
    GN1640T_GRID8 = 0x07,
    GN1640T_GRID9 = 0x08,
    GN1640T_GRID10 = 0x09,
    GN1640T_GRID11 = 0x0A,
    GN1640T_GRID12 = 0x0B,
    GN1640T_GRID13 = 0x0C,
    GN1640T_GRID14 = 0x0D,
    GN1640T_GRID15 = 0x0E,
    GN1640T_GRID16 = 0x0F
} gn1640t_grid_t;

// 滤芯 LED 级别定义
typedef enum
{
    GN1640T_FILTER_LEVEL_0 = 0, // 全部关闭
    GN1640T_FILTER_LEVEL_1,     // 级别1
    GN1640T_FILTER_LEVEL_2,     // 级别2
    GN1640T_FILTER_LEVEL_3,     // 级别3
    GN1640T_FILTER_LEVEL_4      // 级别4
} gn1640t_filter_level_t;

gn1640t_err_t gn1640t_init(void);
gn1640t_err_t gn1640t_clear_ram(void);
gn1640t_err_t gn1640t_led_filter_element(uint8_t level);

gn1640t_err_t gn1640t_led_tds_raw_water(uint8_t value);
gn1640t_err_t gn1640t_led_tds_pure_water(uint8_t value);
gn1640t_err_t gn1640t_led_water(bool state);
gn1640t_err_t gn1640t_led_low(bool state);
gn1640t_err_t gn1640t_led_high(bool state);
gn1640t_err_t gn1640t_led_flush(bool state);
gn1640t_err_t gn1640t_led_leak(bool state);
gn1640t_err_t gn1640t_led_icon(bool state);
gn1640t_err_t gn1640t_led_signal(bool state);

#endif // GN1640T_DRIVER_H
