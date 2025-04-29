#include "gn1640t_driver.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
// #include "freertos/task.h"
// #include <string.h>
// #include <stdio.h>
#include "esp_log.h" // 添加日志头文件

#define TAG "GN1640T" // 日志标签

static SemaphoreHandle_t mutex = NULL; // 发送命令互斥锁

// 定义每个 SEG 的 GRID 状态结构体
typedef struct
{
    uint8_t grid[16]; // GRID1 - GRID16
} gn1640t_segment_t;

// 定义 GN1640T 驱动的内部状态结构体
typedef struct
{
    gn1640t_segment_t seg1;
    gn1640t_segment_t seg2;
    gn1640t_segment_t seg3;
    uint8_t final_grid[16]; // 合并后的 GRID 状态
} gn1640t_driver_t;
// 初始化驱动状态
static gn1640t_driver_t gn1640t_driver = {
    .seg1 = {.grid = {0}},
    .seg2 = {.grid = {0}},
    .seg3 = {.grid = {0}},
    .final_grid = {0}};

uint8_t reset_num = 0; // 记录LED切换次数，增加稳定性

static gn1640t_err_t gn1640t_send_data(gn1640t_ctrl_t ctrl, uint8_t data);
static gn1640t_err_t gn1640t_send_start(void);
static gn1640t_err_t gn1640t_write_byte(uint8_t data);
static gn1640t_err_t gn1640t_send_end(void);
static gn1640t_err_t gn1640t_write_sram(uint8_t *write_buf, size_t buf_size, uint8_t offset);
static gn1640t_err_t gn1640t_set_display_brightness(uint8_t value);

/**
 * @brief 延时函数封装，使用 vTaskDelay
 * @param ms 毫秒数
 */
static void delay_ms(uint16_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

// 初始化GPIO
static gn1640t_err_t gn1640t_gpio_init(void)
{
    gpio_config_t io_conf_output = {
        .pin_bit_mask = (1ULL << CONFIG_GN1640T_CLK_GPIO)    // 时钟引脚
                        | (1ULL << CONFIG_GN1640T_DATA_GPIO) // 数据引脚
        ,                                                    // 选择要配置的引脚
        .mode = GPIO_MODE_OUTPUT,                            // 将GPIO引脚配置为输出模式
        .pull_up_en = CONFIG_GN1640T_PULL_UP,                // 禁用上拉电阻
        .pull_down_en = GPIO_PULLDOWN_DISABLE,               // 禁用下拉电阻
        .intr_type = GPIO_INTR_DISABLE                       // 禁用GPIO中断
    };
    if (gpio_config(&io_conf_output) != ESP_OK)
    {
        return GN1640T_ERR_GPIO_INVALID_ARG; // GPIO无效
    }

    // 初始化 GPIO 电平为高电平（共阳极）
    if (gpio_set_level(CONFIG_GN1640T_CLK_GPIO, 1) != ESP_OK) // 将时钟引脚拉高
    {
        return GN1640T_ERR_GPIO_INVALID_ARG; // GPIO无效
    }
    if (gpio_set_level(CONFIG_GN1640T_DATA_GPIO, 1) != ESP_OK) // 将数据引脚拉高
    {
        return GN1640T_ERR_GPIO_INVALID_ARG; // GPIO无效
    }
    ESP_LOGI(TAG, "GPIO 初始化为高电平"); // 调试日志

    return GN1640T_OK;
}

// 发送起始信号函数
static gn1640t_err_t gn1640t_send_start(void)
{
    // 发送数据起始信号
    if (gpio_set_level(CONFIG_GN1640T_DATA_GPIO, 0) != ESP_OK)
    {
        return GN1640T_ERR_SEND;
    }
    // 发送时钟起始信号
    if (gpio_set_level(CONFIG_GN1640T_CLK_GPIO, 0) != ESP_OK)
    {
        return GN1640T_ERR_SEND;
    }

    return GN1640T_OK; // 返回成功
}

/**
 * @brief 发送一个字节的数据
 * @param data 要发送的数据字节
 * @return gn1640t_err_t GN1640T_OK 成功，其他错误码失败
 */
static gn1640t_err_t gn1640t_write_byte(uint8_t data)
{
    // ESP_LOGD(TAG, "发送字节: 0x%02X", data);

#ifndef CONFIG_GN1640T_DEBUG
    // 将 data 转换为二进制字符串
    char binaryStr[9];
    for (int i = 7; i >= 0; i--)
    {
        binaryStr[7 - i] = ((data >> i) & 1) ? '1' : '0';
    }
    binaryStr[8] = '\0';                             // 添加字符串结束符
    ESP_LOGD(TAG, "发送数据(二进制)=%s", binaryStr); // 输出二进制表示
#endif

    for (int i = 0; i < 8; i++)
    {
        // 从最高位到最低位发送
        // uint8_t bit = (data >> (7 - i)) & 0x01;

        // 设置 DATA 引脚状态（从最低位开始发送）
        uint8_t bit = (data >> i) & 0x01;
        gpio_set_level(CONFIG_GN1640T_DATA_GPIO, bit);
        delay_ms(0.1);

        // CLK 引脚拉高，锁存 DATA
        gpio_set_level(CONFIG_GN1640T_CLK_GPIO, 1);
        delay_ms(0.1);

        // CLK 引脚拉低，准备下一个数据位
        gpio_set_level(CONFIG_GN1640T_CLK_GPIO, 0);
        delay_ms(0.1);
    }
    return GN1640T_OK;
}

// 发送结束信号函数
static gn1640t_err_t gn1640t_send_end(void)
{
    if (gpio_set_level(CONFIG_GN1640T_DATA_GPIO, 0) != ESP_OK) // 将数据引脚拉低
    {
        return GN1640T_ERR_SEND;
    }

    if (gpio_set_level(CONFIG_GN1640T_CLK_GPIO, 1) != ESP_OK) // 发送CLK时钟结束信号
    {
        return GN1640T_ERR_SEND;
    }
    // delay_ms(1)// 可选的延时（目前被注释掉）

    // 发送数据结束信号
    if (gpio_set_level(CONFIG_GN1640T_DATA_GPIO, 1) != ESP_OK) // 将 数据 引脚拉高
    {
        return GN1640T_ERR_SEND;
    }
    return GN1640T_OK; // 返回成功
}

// 发送命令和数据
static gn1640t_err_t gn1640t_send_data(gn1640t_ctrl_t ctrl, uint8_t data)
{
    // ESP_LOGD(TAG, "发送命令=0x%02X, 数据=0x%02X", ctrl, data); // 调试日志

    gn1640t_err_t ret = gn1640t_send_start(); // 发送起始信号
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "发送起始信号失败");
        return GN1640T_ERR_GPIO_INVALID_ARG;
    }

    ret = gn1640t_write_byte(ctrl | data); // 发送控制命令和数据
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "发送数据字节失败");
        return GN1640T_ERR_GPIO_INVALID_ARG;
    }

    ret = gn1640t_send_end(); // 发送结束信号
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "发送结束信号失败");
        return GN1640T_ERR_GPIO_INVALID_ARG;
    }
    return GN1640T_OK;
}

/**
 * @brief 清空显示 RAM
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_clear_ram(void)
{
    ESP_LOGI(TAG, "开始清空显示 RAM"); // 信息日志
    for (gn1640t_grid_t addr = GN1640T_GRID1; addr <= GN1640T_GRID16; addr++)
    {
        // ESP_LOGD(TAG, "清空地址: 0x%02X", addr); // 调试日志

        gn1640t_driver.final_grid[addr] = 0x00; // 初始化亮度显示关闭

        gn1640t_err_t ret = gn1640t_send_data(GN1640T_CTRL_ADDRESS, 0);
        if (ret != GN1640T_OK)
        {
            ESP_LOGE(TAG, "清空地址 0x%02X 失败: %d", addr, ret); // 错误日志
            return ret;
        }
        delay_ms(1); // 延时以确保数据稳定
    }
    ESP_LOGI(TAG, "显示 RAM 清空完成"); // 信息日志
    return GN1640T_OK;
}

/**
 * @brief 设置显示亮度
 * @param value 亮度值（0-8）
 * @return gn1640t_err_t 返回执行结果
 */
static gn1640t_err_t gn1640t_set_display_brightness(uint8_t value)
{
    // 限制亮度值不超过8
    value = value > 8 ? 8 : value;
    ESP_LOGI(TAG, "设置 LED 亮度为: %d", value);

    if (value != 0)
    {
        /**
         * 如果亮度值大于0，构造显示开启命令并设置亮度（亮度值减1，因为亮度范围为0-7）
         * 0x08 显示开启
         */
        uint8_t brightness_cmd = 0x08 | (value - 1);
        return gn1640t_send_data(GN1640T_CTRL_DISPLAY, brightness_cmd);
    }
    else
    {
        /**
         * 如果亮度值为0，构造显示关闭命令
         * 0x00 显示关闭
         */
        return gn1640t_send_data(GN1640T_CTRL_DISPLAY, 0x00);
    }
}

/**
 * @brief 初始化 GN1640T 驱动
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_init(void)
{
    ESP_LOGI(TAG, "GN1640T 初始化开始"); // 信息日志
    gn1640t_err_t ret = GN1640T_OK;      // 定义返回值变量并初始化为0（ESP_OK）

    mutex = xSemaphoreCreateMutex();
    if (mutex == NULL)
    {
        // 互斥锁创建失败
        ESP_LOGE(TAG, "gn1640t_driver互斥锁创建失败");
        return GN1640T_ERR_GPIO_INVALID_ARG;
    }

    ret = gn1640t_gpio_init(); // 初始化GPIO
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "GPIO 配置失败: %d", ret); // 错误日志
        return ret;
    }
    delay_ms(0.1); // 等待系统稳定

    // 设置数据模式为地址自加模式
    ret = gn1640t_send_data(GN1640T_CTRL_DATA, GN1640T_DATA_ADDRESS_AUTOINCREMENT);
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "设置数据模式失败");
        return ret;
    }

    // 清空显示 RAM
    ret = gn1640t_clear_ram();
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "清空显示 RAM 失败");
        return ret;
    }

    // 设置显示亮度
    // ESP_LOGI(TAG, "LED亮度数值：%d", CONFIG_GN1640T_DISPLAY_BRIGHTNESS);
    ret = gn1640t_set_display_brightness(CONFIG_GN1640T_DISPLAY_BRIGHTNESS);
    if (ret != GN1640T_OK)
    {
        // ESP_LOGE(TAG, "LED亮度配置失败: %s", esp_err_to_name(ret)); // 错误日志
        ESP_LOGE(TAG, "设置显示亮度失败");
        return ret;
    }
    ESP_LOGI(TAG, "GN1640T 初始化完成");
    return GN1640T_OK;
}

/**
 * @brief 向 SRAM 写入数据
 * @param write_buf 写入的数据缓冲区
 * @param buf_size 缓冲区大小（最大16字节）
 * @param offset 写入的起始地址偏移
 * @return gn1640t_err_t GN1640T_OK 成功，其他错误码失败
 */
static gn1640t_err_t gn1640t_write_sram(uint8_t *write_buf, size_t buf_size, uint8_t offset)
{
    // if (!gn1640t_driver.inited)
    // {
    //     ESP_LOGE(TAG, "驱动未初始化");
    //     return GN1640T_ERR_NOT_INITED;
    // }
    // 获取互斥锁成功，可以访问共享资源
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(5000)) != pdTRUE)
    {
        // 获取互斥锁失败，可能超时
        ESP_LOGE(TAG, "获取gn1640t互斥锁失败，超时");
        return GN1640T_ERR_MUTEX;
    }

    if (offset + (uint8_t)buf_size > 16)
    {
        ESP_LOGE(TAG, "写入数据超过 SRAM 大小限制");
        return GN1640T_ERR_INVALID_SIZE;
    }

    // 设置数据模式-固定地址模式
    // gn1640t_err_t ret = gn1640t_send_data(GN1640T_CTRL_DATA, GN1640T_DATA_ADDRESS_FIXED);
    // if (ret != GN1640T_OK)
    // {
    //     ESP_LOGE(TAG, "设置数据模式失败");
    //     return ret;
    // }
    gn1640t_err_t ret;
    // ESP_LOGD(TAG, "向 SRAM 写入数据，偏移: %02X, 大小: %02X", offset, buf_size); // 调试日志

    /******** 设置地址 *********/
    // ESP_LOGD(TAG, "发送: 命令=0x%02X, 数据=0x%02X", GN1640T_CTRL_ADDRESS, offset); // 调试日志
    // 发送起始信号
    ret = gn1640t_send_start();
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "发送起始信号失败");
        return ret;
    }
    // 构造地址命令：设置起始地址
    ret = gn1640t_write_byte(GN1640T_CTRL_ADDRESS | offset);
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "发送字节失败");
        return ret;
    }
    // 逐字节发送写入缓冲区的数据
    for (size_t i = 0; i < buf_size; i++)
    {
        ret = gn1640t_write_byte(GN1640T_CTRL_DATA | write_buf[i]); // 发送每一个字节
        if (ret != GN1640T_OK)
        {
            ESP_LOGE(TAG, "写入数据失败");
            return ret;
        }
        // delay_ms(0.1); // 避免发送过快
    }
    ret = gn1640t_send_end(); // 发送结束信号
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "发送结束信号失败");
        return ret;
    }

    // ESP_LOGD(TAG, "SRAM 写入完成，起始地址偏移为 %d，写入大小为 %d 字节", offset, buf_size);

    xSemaphoreGive(mutex); // 释放命令互斥锁
    return GN1640T_OK;
}

// 更新 final_grid 为 SEG1, SEG2, SEG3 的组合
static void gn1640t_update_final_grid(void)
{
    gn1640t_err_t ret;
    if (reset_num >= 255)
    {
        // 清空显示RAM
        ret = gn1640t_clear_ram();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "gn1640t_clear_ram 失败: %d", ret); // 错误日志
            return;
        }
        reset_num = 0;
    }
    else
    {
        ++reset_num;
    }

    /**
     * 遍历所有 GRID
     * 共阳极接法，逻辑低电平点亮 LED
     * GRID1-16从上到下获取SEG1-8进行更新对应位值
     */
    for (int i = 0; i < 16; ++i)
    {
        // 合并各 SEG 的状态
        if (gn1640t_driver.seg1.grid[i])
        {
            gn1640t_driver.final_grid[i] |= (1 << 0); // SEG1
        }
        else
        {
            gn1640t_driver.final_grid[i] &= ~(1 << 0); // 清除 SEG1 对应位为0（LED点亮）
        }

        if (gn1640t_driver.seg2.grid[i])
        {
            gn1640t_driver.final_grid[i] |= (1 << 1); // SEG2
        }
        else
        {
            gn1640t_driver.final_grid[i] &= ~(1 << 1); // SEG2
        }

        if (gn1640t_driver.seg3.grid[i])
        {
            gn1640t_driver.final_grid[i] |= (1 << 2); // SEG3
        }
        else
        {
            gn1640t_driver.final_grid[i] &= ~(1 << 2); // SEG3
        }
        // 如果有更多 SEG，可继续扩展
#ifndef CONFIG_GN1640T_DEBUG
// 将 data 转换为二进制字符串
// char binaryStr[9];
// for (int j = 7; j >= 0; j--)
// {
//     binaryStr[7 - j] = ((gn1640t_driver.final_grid[grid_index] >> j) & 1) ? '1' : '0';
// }
// binaryStr[8] = '\0';                             // 添加字符串结束符
// ESP_LOGD(TAG, "发送数据(二进制)=%s", binaryStr); // 输出二进制表示
#endif
    }
}

// 控制 SEG3 的 GRID 的状态
static gn1640t_err_t set_grid_seg3_state(gn1640t_grid_t grid, uint8_t value)
{
    if (grid >= GN1640T_GRID16)
    {
        ESP_LOGE(TAG, "无效的 GRID 索引: %d", grid);
        return GN1640T_ERR_INVALID_SIZE;
    }

    gn1640t_driver.seg3.grid[grid] = value ? 1 : 0;
    // ESP_LOGD(TAG, "设置 SEG3 的 GRID%d 为 %s", grid + 1, value ? "ON" : "OFF");
    gn1640t_update_final_grid(); // 更新合并

    gn1640t_err_t ret = gn1640t_write_sram(gn1640t_driver.final_grid, 16, 0); // 传递指针，并设置偏移量为 i
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "gn1640t_led_water err: %d", ret); // 错误日志
        return ret;
    }
    return GN1640T_OK;
}

//-----------------------------------
/**
 * @brief 控制制水 LED
 * @param state true 点亮，false 关闭
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_water(bool state)
{
    // ESP_LOGD(TAG, "设置制水 LED: %s", state ? "ON" : "OFF");
    return set_grid_seg3_state(GN1640T_GRID1, (state ? 1 : 0));
}

/**
 * @brief 控制缺水 LED
 * @param state true 点亮，false 关闭
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_low(bool state)
{
    // ESP_LOGD(TAG, "设置缺水 LED: %s", state ? "ON" : "OFF");
    return set_grid_seg3_state(GN1640T_GRID2, (state ? 1 : 0));
}

/**
 * @brief 控制水满 LED
 * @param state true 点亮，false 关闭
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_high(bool state)
{
    // ESP_LOGD(TAG, "设置水满 LED: %s", state ? "ON" : "OFF");
    return set_grid_seg3_state(GN1640T_GRID3, (state ? 1 : 0));
}

/**
 * @brief 控制冲洗 LED
 * @param state true 点亮，false 关闭
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_flush(bool state)
{
    // ESP_LOGD(TAG, "设置冲洗 LED: %s", state ? "ON" : "OFF");
    return set_grid_seg3_state(GN1640T_GRID4, (state ? 1 : 0));
}

/**
 * @brief 控制漏水 LED
 * @param state true 点亮，false 关闭
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_leak(bool state)
{
    // ESP_LOGD(TAG, "设置漏水 LED: %s", state ? "ON" : "OFF");
    return set_grid_seg3_state(GN1640T_GRID5, (state ? 1 : 0));
}

/**
 * @brief 控制文字图标 LED
 * @param state true 点亮，false 关闭
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_icon(bool state)
{
    // ESP_LOGD(TAG, "设置文字图标 LED: %s", state ? "ON" : "OFF");
    return set_grid_seg3_state(GN1640T_GRID10, (state ? 1 : 0));
}

/**
 * @brief 控制信号 LED
 * @param state true 点亮，false 关闭
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_signal(bool state)
{
    return set_grid_seg3_state(GN1640T_GRID11, (state ? 1 : 0));
}

/**
 * @brief 控制滤芯 LED 级别
 * @param level 滤芯级别（0-4）
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_filter_element(uint8_t level)
{
    // 限制级别不超过4
    level = level > 4 ? 4 : level;
    ESP_LOGI(TAG, "设置滤芯 LED 级别: %d", level);

    // 根据级别设置对应的 GRID 状态
    switch (level)
    {
    case GN1640T_FILTER_LEVEL_1:
        gn1640t_driver.seg3.grid[GN1640T_GRID6] = 0; // GRID6-滤芯级别4 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID7] = 0; // GRID7-滤芯级别3 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID8] = 0; // GRID8-滤芯级别2 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID9] = 1; // GRID9-滤芯级别1 点亮
        break;
    case GN1640T_FILTER_LEVEL_2:
        gn1640t_driver.seg3.grid[GN1640T_GRID6] = 0; // GRID6-滤芯级别4 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID7] = 0; // GRID7-滤芯级别3 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID8] = 1; // GRID8-滤芯级别2 点亮
        gn1640t_driver.seg3.grid[GN1640T_GRID9] = 1; // GRID9-滤芯级别1 点亮
        break;
    case GN1640T_FILTER_LEVEL_3:
        gn1640t_driver.seg3.grid[GN1640T_GRID6] = 0; // GRID6-滤芯级别4 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID7] = 1; // GRID7-滤芯级别3 点亮
        gn1640t_driver.seg3.grid[GN1640T_GRID8] = 1; // GRID8-滤芯级别2 点亮
        gn1640t_driver.seg3.grid[GN1640T_GRID9] = 1; // GRID9-滤芯级别1 点亮
        break;
    case GN1640T_FILTER_LEVEL_4:
        gn1640t_driver.seg3.grid[GN1640T_GRID6] = 1; // GRID6-滤芯级别4 点亮
        gn1640t_driver.seg3.grid[GN1640T_GRID7] = 1; // GRID7-滤芯级别3 点亮
        gn1640t_driver.seg3.grid[GN1640T_GRID8] = 1; // GRID8-滤芯级别2 点亮
        gn1640t_driver.seg3.grid[GN1640T_GRID9] = 1; // GRID9-滤芯级别1 点亮
        break;
    case GN1640T_FILTER_LEVEL_0: // 全部关闭
    default:
        gn1640t_driver.seg3.grid[GN1640T_GRID6] = 0; // GRID6-滤芯级别4 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID7] = 0; // GRID7-滤芯级别3 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID8] = 0; // GRID8-滤芯级别2 关闭
        gn1640t_driver.seg3.grid[GN1640T_GRID9] = 0; // GRID9-滤芯级别1 关闭
        break;
    }
    gn1640t_update_final_grid(); // 更新合并后的 final_grid
#ifndef CONFIG_GN1640T_DEBUG
// 二进制转换字符串日志-调试
// char binaryStr[9];
// char binaryStr2[9];
// for (int j = 7; j >= 0; j--)
// {
//     binaryStr[7 - j] = ((gn1640t_driver.final_grid[6] >> j) & 1) ? '1' : '0';
//     binaryStr2[7 - j] = ((gn1640t_driver.final_grid[7] >> j) & 1) ? '1' : '0';
// }
// binaryStr[8] = '\0'; // 添加字符串结束符
// binaryStr2[8] = '\0';
// ESP_LOGD(TAG, "发送数据(二进制)6=%s", binaryStr);  // 输出二进制表示
// ESP_LOGD(TAG, "发送数据(二进制)7=%s", binaryStr2); // 输出二进制表示
#endif

    gn1640t_err_t ret = gn1640t_write_sram(gn1640t_driver.final_grid, 16, 0); // 传递指针，并设置偏移量为 i
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "led_filter_element 写入失败");
        return ret;
    }

    return GN1640T_OK;
}

// 数字到段的映射
const uint8_t digit_to_segments[10] = {
    0b0111111, // 0: a, b, c, d, e, f
    0b0000110, // 1: b, c
    0b1011011, // 2: a, b, d, e, g
    0b1001111, // 3: a, b, c, d, g
    0b1100110, // 4: b, c, f, g
    0b1101101, // 5: a, c, d, f, g
    0b1111101, // 6: a, c, d, e, f, g
    0b0000111, // 7: a, b, c
    0b1111111, // 8: a, b, c, d, e, f, g
    0b1101111  // 9: a, b, c, d, f, g
};

/**
 * @brief 控制纯水 TDS LED 显示
 * @param value 要显示的 TDS 数值（0-188）
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_tds_pure_water(uint8_t value)
{
    value = value > 188 ? 188 : value; // 限制数值
    // ESP_LOGD(TAG, "设置原水 TDS 数值: %d", value);

    // 清空 SEG2 的 GRID 状态
    for (int i = 0; i < 16; i++)
    {
        gn1640t_driver.seg2.grid[i] = 0;
    }

    // 接收来自value的值转为数字LED显示，最大188-将数字拆分为百位、十位、个位
    uint8_t hundreds = value / 100;    // 百位
    uint8_t tens = (value % 100) / 10; // 十位
    uint8_t units = value % 10;        // 个位

    // 个位分段
    const uint8_t units_segments[] = {
        GN1640T_GRID10, // a
        GN1640T_GRID11, // b
        GN1640T_GRID12, // c
        GN1640T_GRID13, // d
        GN1640T_GRID14, // e
        GN1640T_GRID15, // f
        GN1640T_GRID16  // g
    };
    // 十位分段
    const uint8_t tens_segments[] = {
        GN1640T_GRID3, // a
        GN1640T_GRID4, // b
        GN1640T_GRID5, // c
        GN1640T_GRID9, // d
        GN1640T_GRID7, // e
        GN1640T_GRID8, // f
        GN1640T_GRID6  // g
    };
    // 百位分段
    const uint8_t hundreds_segment = GN1640T_GRID2;

    // 定义一个数组来存储每个段的状态
    uint8_t segments[3][7] = {0}; // [digit][segment]

    // 设置个位段状态
    if (units < 10)
    {
        for (int i = 0; i < 7; i++)
        {
            uint8_t digit_to_s = (digit_to_segments[units] >> i);
            // segments[2][i] = (digit_to_segments[units] >> i) & 0x01;
            segments[2][i] = digit_to_s & 0x01;
#ifndef CONFIG_GN1640T_DEBUG
            // 将 data 转换为二进制字符串
            char binaryStr[9];
            for (int i = 7; i >= 0; i--)
            {
                binaryStr[7 - i] = ((digit_to_s >> i) & 1) ? '1' : '0';
            }
            binaryStr[8] = '\0';                             // 添加字符串结束符
            ESP_LOGD(TAG, "发送数据(二进制)=%s", binaryStr); // 输出二进制表示
#endif
        }
    }
    // 设置十位段状态
    if (tens < 10 && tens != 0)
    {
        for (int i = 0; i < 7; i++)
        {
            segments[1][i] = (digit_to_segments[tens] >> i) & 0x01;
        }
    }
    // 设置百位段状态（仅 a 段）
    if (hundreds > 0)
    {
        segments[0][0] = 1; // a 段
    }

    // 设置个位段
    for (int i = 0; i < 7; i++)
    {
        gn1640t_driver.seg2.grid[units_segments[i]] = segments[2][i];
    }

    // 设置十位段
    for (int i = 0; i < 7; i++)
    {
        gn1640t_driver.seg2.grid[tens_segments[i]] = segments[1][i];
    }

    // 设置百位段
    if (hundreds > 0)
    {
        // 仅 a 段
        gn1640t_driver.seg2.grid[hundreds_segment] = segments[0][0]; // 共阳极，低电平点亮
    }

    if (value > 0)
    {
        gn1640t_driver.seg2.grid[GN1640T_GRID1] = 1; // 启用文字图案
    }
    else
    {
        gn1640t_driver.seg2.grid[GN1640T_GRID1] = 0;
    }

    // 更新 final_grid 为 SEG1, SEG2, SEG3 的组合
    gn1640t_update_final_grid();

    // 将 final_grid 写入 SRAM
    gn1640t_err_t ret = gn1640t_write_sram(gn1640t_driver.final_grid, 16, 0);
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "gn1640t_led_tds_pure_water 写入失败");
        return ret;
    }

    // ESP_LOGD(TAG, "TDS 显示已更新: %d", value);
    return GN1640T_OK;
}

/**
 * @brief 控制原水 TDS LED 显示
 * @param value 要显示的 TDS 数值（0-188）
 * @return gn1640t_err_t 返回执行结果
 */
gn1640t_err_t gn1640t_led_tds_raw_water(uint8_t value)
{
    value = value > 188 ? 188 : value; // 限制数值-接收来自value的值转为数字LED显示，最大188
    // ESP_LOGD(TAG, "设置纯水 TDS 数值: %d", value);

    // 清空 SEG1 的 GRID 状态
    for (int i = 0; i < 16; i++)
    {
        gn1640t_driver.seg1.grid[i] = 0;
    }

    // 接收来自value的值转为数字LED显示，最大188-将数字拆分为百位、十位、个位
    uint8_t hundreds = value / 100;    // 百位
    uint8_t tens = (value % 100) / 10; // 十位
    uint8_t units = value % 10;        // 个位

    // 个位分段
    const uint8_t units_segments[] = {
        GN1640T_GRID10, // a
        GN1640T_GRID11, // b
        GN1640T_GRID12, // c
        GN1640T_GRID13, // d
        GN1640T_GRID14, // e
        GN1640T_GRID15, // f
        GN1640T_GRID16  // g
    };
    // 十位分段
    const uint8_t tens_segments[] = {
        GN1640T_GRID3, // a
        GN1640T_GRID4, // b
        GN1640T_GRID5, // c
        GN1640T_GRID9, // d
        GN1640T_GRID7, // e
        GN1640T_GRID8, // f
        GN1640T_GRID6  // g
    };
    // 百位分段
    const uint8_t hundreds_segment = GN1640T_GRID2;

    // 定义一个数组来存储每个段的状态
    uint8_t segments[3][7] = {0}; // [digit][segment]

    // 设置个位段状态
    if (units < 10)
    {
        for (int i = 0; i < 7; i++)
        {
            uint8_t digit_to_s = (digit_to_segments[units] >> i);
            segments[2][i] = digit_to_s & 0x01;
#ifndef CONFIG_GN1640T_DEBUG
            // 将 data 转换为二进制字符串
            char binaryStr[9];
            for (int i = 7; i >= 0; i--)
            {
                binaryStr[7 - i] = ((digit_to_s >> i) & 1) ? '1' : '0';
            }
            binaryStr[8] = '\0';                             // 添加字符串结束符
            ESP_LOGD(TAG, "发送数据(二进制)=%s", binaryStr); // 输出二进制表示
#endif
        }
    }
    // 设置十位段状态
    if (tens < 10 && tens != 0)
    {
        for (int i = 0; i < 7; i++)
        {
            segments[1][i] = (digit_to_segments[tens] >> i) & 0x01;
        }
    }
    // 设置百位段状态（仅 a 段）
    if (hundreds > 0)
    {
        segments[0][0] = 1; // a 段
    }

    // 设置个位段
    for (int i = 0; i < 7; i++)
    {
        gn1640t_driver.seg1.grid[units_segments[i]] = segments[2][i];
    }

    // 设置十位段
    for (int i = 0; i < 7; i++)
    {
        gn1640t_driver.seg1.grid[tens_segments[i]] = segments[1][i];
    }

    // 设置百位段
    if (hundreds > 0)
    {
        // 仅 a 段
        gn1640t_driver.seg1.grid[hundreds_segment] = segments[0][0]; // 共阳极，低电平点亮
    }

    if (value > 0)
    {
        gn1640t_driver.seg1.grid[GN1640T_GRID1] = 1; // 启用文字图案
    }
    else
    {
        gn1640t_driver.seg1.grid[GN1640T_GRID1] = 0;
    }

    // 更新 final_grid 为 SEG1, SEG2, SEG3 的组合
    gn1640t_update_final_grid();

    // 将 final_grid 写入 SRAM
    gn1640t_err_t ret = gn1640t_write_sram(gn1640t_driver.final_grid, 16, 0);
    if (ret != GN1640T_OK)
    {
        ESP_LOGE(TAG, "gn1640t_led_tds_pure_water 写入失败");
        return ret;
    }
    // ESP_LOGD(TAG, "纯水TDS 显示已更新: %d", value);
    return GN1640T_OK;
}
