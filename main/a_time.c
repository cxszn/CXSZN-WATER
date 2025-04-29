#include "a_time.h"
#include "head.h"
#include "u4g_at_cmd.h"
#include "u4g_data.h"
#include "lwip/apps/sntp.h"
#include "a_nvs_flash.h" // nvs_flash应用类
#include "esp_system.h"
#include "esp_log.h"
#include <time.h>

#define TAG "AT_TIME" // 日志TAG

// 联网同步时间
esp_err_t a_time_sync(void)
{
    // &raw_time
    emU4GResult ret = u4g_at_time_get();
    if (ret != U4G_OK)
    {
        ESP_LOGE(TAG, "AT获取时间失败，错误码：%d", ret);
        return ESP_FAIL;
    }

    char *time_input = u4g_data_get()->time;
    ESP_LOGE(TAG, "4G模块，获取网络时间: %s", time_input);

    // 分离日期时间和时区
    char datetime_part[20];
    char timezone_part[4];
    if (sscanf(time_input, "%[^+]+%s", datetime_part, timezone_part) != 2)
    {
        ESP_LOGE(TAG, "解析日期时间和增量时区失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "日期时间部分: %s", datetime_part);
    ESP_LOGI(TAG, "增量时部分: %s", timezone_part);

    // 解析日期时间
    int year, month, day, hour, minute, second;
    if (sscanf(datetime_part, "%d/%d/%d,%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
    {
        ESP_LOGE(TAG, "解析日期时间失败");
        return ESP_FAIL;
    }
    // 处理年份，假设年份为2000年以后
    if (year < 70)
    { // 例如 "24" 表示 2024
        year += 2000;
    }
    else
    {
        year += 1900;
    }
    ESP_LOGI(TAG, "解析后的时间: %04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);

    // 解析时区，时区单位为1/4小时
    int tz_offset_quarter;
    if (sscanf(timezone_part, "%d", &tz_offset_quarter) != 1)
    {
        ESP_LOGE(TAG, "解析增量小时时失败");
        return ESP_FAIL;
    }
    // 计算时区偏移量（小时）
    int tz_offset_hours = tz_offset_quarter / 4;
    int tz_offset_minutes = (tz_offset_quarter % 4) * 15;
    ESP_LOGI(TAG, "小时偏移量: %d小时 %d分钟", tz_offset_hours, tz_offset_minutes);

    // 计算实际时间（UTC 时间）
    struct tm tm_info;
    memset(&tm_info, 0, sizeof(struct tm));
    tm_info.tm_year = year - 1900;
    tm_info.tm_mon = month - 1;
    tm_info.tm_mday = day;
    tm_info.tm_hour = hour; // 偏移小时
    // tm_info.tm_hour = hour - tz_offset_hours; // 偏移时区
    tm_info.tm_min = minute; // 偏移分
    // tm_info.tm_min = minute - tz_offset_minutes; // 偏移分区
    tm_info.tm_sec = second;

    time_t raw_time = mktime(&tm_info);
    // *raw_time = mktime(&tm_info);
    if (raw_time == -1)
    {
        ESP_LOGE(TAG, "时间转换失败");
        return ESP_FAIL;
    }
    // ESP_LOGI(TAG, "转换时间为 UTC: %lld", *raw_time);
    // 增加时间增量
    raw_time += tz_offset_hours * 3600 + tz_offset_minutes * 60;
    // time_t *time_out = raw_time;
    // ESP_LOGI(TAG, "原始时间戳: %lld，转换后时间戳：%lld", raw_time, time_out);

    ESP_LOGI(TAG, "设置系统时间为 UTC: %lld", raw_time);
    // 设置系统时间
    struct timeval tv;
    tv.tv_sec = raw_time;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    // 验证设置的时间
    time(&raw_time);
    struct tm *current_time = localtime(&raw_time);
    ESP_LOGI(TAG, "当前系统时间: %04d-%02d-%02d %02d:%02d:%02d",
             current_time->tm_year + 1900,
             current_time->tm_mon + 1,
             current_time->tm_mday,
             current_time->tm_hour,
             current_time->tm_min,
             current_time->tm_sec);
    return ESP_OK;
}

/**
 * 离线同步时间
 * 从nvs-time中获取
 */
esp_err_t a_time_sync_offline(void)
{
    int32_t offline_time;
    // 读取保存的最后时间-格式：时间戳
    if (a_nvs_flash_get_int("time", &offline_time) != ESP_OK)
    {
        ESP_LOGE(TAG, "未设置离线时间，无法执行离线同步时间");
        return ESP_OK;
    }
    ESP_LOGI(TAG, "已获取到的离线时间为：%ld", offline_time);

    time_t raw_time = (time_t)offline_time;

    // char *offline_time = app_nvs_flash_get("time"); // 读取保存的最后时间-格式：时间戳
    // if (offline_time == NULL)
    // {
    //     ESP_LOGE(TAG, "未设置离线时间，无法执行离线同步时间");
    //     return ESP_OK;
    // }
    // ESP_LOGD(TAG, "已获取到的离线时间为：%s",offline_time);
    // // 将字符串转换为 long 类型
    // char *endptr; // 用于指向转换结束的位置
    // long long_time = strtoll(offline_time, &endptr, 10); // 以10进制转换
    // // 检查转换是否成功
    // if (*endptr != '\0') {
    //     ESP_LOGE(TAG, "时间戳转换失败，输入字符串无效");
    //     return ESP_FAIL; // 或者其他错误处理
    // }
    // // 将 long 类型赋值给 time_t
    // time_t raw_time = (time_t)long_time;

    // 设置系统时间
    struct timeval tv;
    tv.tv_sec = raw_time;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    // 验证设置的时间
    time(&raw_time);
    struct tm *current_time = localtime(&raw_time);
    ESP_LOGI(TAG, "当前系统时间: %04d-%02d-%02d %02d:%02d:%02d",
             current_time->tm_year + 1900,
             current_time->tm_mon + 1,
             current_time->tm_mday,
             current_time->tm_hour,
             current_time->tm_min,
             current_time->tm_sec);
    return ESP_OK;
}

esp_err_t a_time_save(void)
{
    time_t now;
    time(&now); // 获取当前时间戳
    ESP_LOGW(TAG, "开始保存 时间戳: %llu至NVS", now);
    if (a_nvs_flash_insert_int("time", now) != ESP_OK)
    {
        ESP_LOGI(TAG, "保存为离线时间失败");
        return ESP_FAIL;
    }

    if (DEVICE.NETSTATE == DEVICE_NETON)
    {
        ESP_LOGI(TAG, "处于联网状态，执行重置离线时间记忆");
        a_nvs_flash_del("offline_time");
    }
    else
    {
        int32_t offline_time;
        // 检查是否有字段
        if (a_nvs_flash_get_int("offline_time", &offline_time) != ESP_OK)
        {
            ESP_LOGI(TAG, "写入首次离线时间：%llu", now);
            a_nvs_flash_insert_int("offline_time", now); // 替换为实际时间
        }
    }
    return ESP_OK;
}

// 获取当前时间
// time_t now;
// struct tm timeinfo;
// char strftime_buf[64];
// // 获取当前时间戳
// time(&now);
// ESP_LOGI(TAG, "当前 时间戳: %llu", now);
// // 将时间戳转换为本地时间
// localtime_r(&now, &timeinfo);
// // 格式化时间字符串
// strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
// ESP_LOGI(TAG, "当前本地时间: %s", strftime_buf);

// // 获取 UTC 时间
// gmtime_r(&now, &timeinfo);
// strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S UTC", &timeinfo);
// ESP_LOGI(TAG, "当前 UTC 时间: %s", strftime_buf);