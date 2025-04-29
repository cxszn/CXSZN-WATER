#include "u4g_utils.h" // 工具类
#include "u4g_state.h" // 状态码
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"

#define TAG "4G-UTILS" // 日志标签

// 内部函数：替换字符串中的子串
static char *str_replace(const char *src, const char *find, const char *replace)
{
    if (!src || !find || !replace)
        return NULL;

    size_t find_len = strlen(find);
    size_t replace_len = strlen(replace);
    size_t src_len = strlen(src);

    // 计算替换后的最大可能长度
    size_t max_len = src_len + (src_len / find_len + 1) * (replace_len - find_len) + 1;
    char *result = malloc(max_len);
    if (!result)
        return NULL;

    char *dst = result;
    const char *p = src;
    while (*p)
    {
        if (strncmp(p, find, find_len) == 0)
        {
            memcpy(dst, replace, replace_len);
            dst += replace_len;
            p += find_len;
        }
        else
        {
            *dst++ = *p++;
        }
    }
    *dst = '\0';
    return result;
}

/**
 * @brief 转义字符串中的特殊字符（如 " -> \"）
 * @param input 原始字符串
 * @return 转义后的字符串（需调用者释放）
 */
char *u4g_utils_at_escape_string(const char *input)
{
    if (!input)
        return NULL;

    // 分步骤转义特殊字符
    char *temp = str_replace(input, "\"", "\\\""); // 转义双引号
    if (!temp)
        return NULL;

    // 可扩展其他转义规则，例如：
    // temp = str_replace(temp, "\r", "\\r");
    return temp;
}

/**
 * @brief 构建安全的AT指令字符串（需显式传递转义后的参数）
 * @param format AT指令格式字符串，必须包含与参数对应的格式说明符（如%s）
 * @param max_length 模块支持的最大AT指令长度
 * @param out_cmd 输出生成的AT指令（需调用者释放）
 * @param out_len 输出指令长度
 * @param ... 格式字符串参数（必须包含所有转义后的参数）
 * @return 错误码
 */
emU4GResult u4g_utils_at_cmd_build(const char *format,
                                   int max_length,
                                   char **out_cmd,
                                   size_t *out_len,
                                   ...)
{
    if (!format || !out_cmd || !out_len || max_length <= 0)
    {
        ESP_LOGE(TAG, "Invalid arguments");
        return U4G_ERR_INVALID_ARG;
    }

    va_list args;
    va_start(args, out_len);

    // Step 1: 计算所需缓冲区大小
    va_list size_args;
    va_copy(size_args, args);
    int required = vsnprintf(NULL, 0, format, size_args) + 1; // +1 for '\0'
    va_end(size_args);

    if (required > max_length)
    {
        ESP_LOGE(TAG, "指令过长 (限制:%d 需要:%d)", max_length, required);
        va_end(args);
        return U4G_STATE_AT_BUILDER_OVERFLOW;
    }

    // Step 2: 分配内存并生成指令
    char *cmd = malloc(required);
    if (!cmd)
    {
        ESP_LOGE(TAG, "内存分配失败");
        va_end(args);
        return U4G_STATE_AT_BUILDER_MEM_FAIL;
    }

    vsnprintf(cmd, required, format, args);
    va_end(args);

    // Step 3: 返回结果
    *out_cmd = cmd;
    *out_len = strlen(cmd);
    return U4G_OK;
}
