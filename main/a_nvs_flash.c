#include "a_nvs_flash.h"
#include "nvs_flash.h"
#include "esp_log.h"

#define TAG "A-NVS" // 日志标签

/**
 * 初始化NVS程序
 * 确保NVS在程序开始时被正确初始化，并在必要时进行擦除以解决初始化失败的问题。通过这种方式，可以确保后续的NVS操作能够正常进行
 */
esp_err_t a_nvs_flash_init(void)
{
    esp_err_t ret = nvs_flash_init(); // 初始化NVS子系统（res是一个变量，用于存储函数返回的错误代码。esp_err_t是ESP-IDF中用于表示错误代码的类型）
    /**检查nvs_flash_init()的返回值
     * ESP_ERR_NVS_NO_FREE_PAGES表示NVS存储空间已满，没有可用的空闲页
     * ESP_ERR_NVS_NEW_VERSION_FOUND表示NVS存储的版本与当前使用的版本不兼容
     */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        /**ESP_ERROR_CHECK(nvs_flash_erase());
         * 初始化失败且错误代码是上述两种情况之一，则调用nvs_flash_erase()函数来擦除整个NVS分区
         * ESP_ERROR_CHECK是一个宏，用于检查函数调用是否成功。如果函数返回错误，程序将打印错误信息并终止
         */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init(); // 再次调用nvs_flash_init()以重新初始化NVS。此时，NVS应该已经被擦除并准备好使用
    }
    return ret; // 确保函数返回一个 esp_err_t 类型的值
}

/**
 * 打开NVS命名空间并写入数据
 * 作用是将一个键值对（"key": "value"）存储到ESP32的NVS中，以便在设备重启后仍然可以读取到该数据。NVS是一种可靠的存储方式，适合存储配置参数和其他需要持久化的数据
 */
esp_err_t a_nvs_flash_insert(const char *key, const char *value)
{
    nvs_handle_t my_handle; // 声明一个变量 my_handle，类型为 nvs_handle_t。这个变量用于存储NVS命名空间的句柄，句柄是用于操作NVS的标识符
    esp_err_t err;          // 变量

    /**
     * 调用 nvs_open() 函数来打开一个NVS命名空间
     * "storage" 是命名空间的名称，可以根据需要更改
     * NVS_READWRITE 指定打开模式为读写模式，允许读取和写入操作
     * &my_handle 是一个指针，nvs_open() 会通过这个指针返回命名空间的句柄
     * err 是一个变量，用于存储 nvs_open() 的返回值，返回值表示操作是否成功
     */
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    // 检查键是否已经存在
    size_t required_size = 0;
    err = nvs_get_str(my_handle, key, NULL, &required_size);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Key '%s' exists. Overwriting value.", key); // 存在 覆盖值
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "Key '%s' does not exist. Creating new entry.", key); // 不存在。创建新条目
    }
    else
    {
        ESP_LOGE(TAG, "Error (%s) checking key existence!", esp_err_to_name(err)); // 检查密钥是否存在
        nvs_close(my_handle);
        return err;
    }

    /**
     * 存储字符串
     * 调用 nvs_set_str() 函数在打开的命名空间中存储一个字符串
     * my_handle 是之前打开的命名空间的句柄
     * "key" 是数据的键，可以根据需要更改
     * "value" 是要存储的字符串值
     */
    err = nvs_set_str(my_handle, key, value);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) setting string!", esp_err_to_name(err)); // 设置字符串
        nvs_close(my_handle);
        return err;
    }

    // 提交更改
    err = nvs_commit(my_handle); // 将所有挂起的写入操作提交到NVS存储中（这一步确保数据被实际写入到存储中，而不仅仅是保存在缓存中）
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) committing changes!", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }
    ESP_LOGI(TAG, "Successfully inserted/updated key '%s' with value '%s'.", key, value);
    nvs_close(my_handle); // 关闭命名空间（关闭命名空间可以释放相关资源）
    return ESP_OK;
}

/**
 * 获取数据
 */
char *a_nvs_flash_get(const char *key)
{
    nvs_handle_t my_handle; // 声明变量
    esp_err_t err;          // 声明变量
    /**
     * 打开NVS命名空间
     * NVS_READONLY指定只读模式
     */
    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return NULL; // 处理错误
    }

    size_t required_size = 0; // 声明一个变量 required_size，用于存储所需的缓冲区大小
    /**
     * 获取字符串长度
     * 调用 nvs_get_str() 函数获取存储在键 "key" 下的字符串的长度
     * my_handle 是之前打开的NVS命名空间的句柄
     * NULL 表示不需要实际读取字符串，只获取其长度
     * &required_size 用于接收字符串的长度（包括终止符）
     */
    err = nvs_get_str(my_handle, key, NULL, &required_size);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key '%s' not found.", key);
        nvs_close(my_handle); // 关闭NVS命名空间，释放相关资源
        return NULL;          // 处理错误
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) getting string size!", esp_err_to_name(err));
        nvs_close(my_handle); // 关闭NVS命名空间，释放相关资源
        return NULL;          // 处理错误
    }

    // 动态分配内存以存储字符串
    char *value = malloc(required_size);
    if (value == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for NVS value");
        nvs_close(my_handle); // 关闭NVS命名空间，释放相关资源
        return NULL;          // 处理内存分配错误
    }

    /**
     * 获取实际字符串
     * 再次调用 nvs_get_str() 函数，这次提供 value 缓冲区来存储实际的字符串
     * required_size 确保缓冲区大小足够存储字符串
     */
    err = nvs_get_str(my_handle, key, value, &required_size);
    nvs_close(my_handle);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) getting string!", esp_err_to_name(err));
        free(value); // 释放内存
        return NULL; // 处理错误
    }

    return value; // 返回字符串
}

// 写入整数值
esp_err_t a_nvs_flash_insert_int(const char *key, int32_t value)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i32(my_handle, key, value); // 使用 nvs_set_i32 写入整数
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) setting integer!", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) committing changes!", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }
    ESP_LOGI(TAG, "Successfully inserted/updated key '%s' with integer value '%ld'.", key, value);
    nvs_close(my_handle);
    return ESP_OK;
}

// 获取整数值
esp_err_t a_nvs_flash_get_int(const char *key, int32_t *value)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_i32(my_handle, key, value); // 使用 nvs_get_i32 获取整数
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Key '%s' not found.", key);
        nvs_close(my_handle);
        return ESP_ERR_INVALID_ARG; // 无字段
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) getting integer!", esp_err_to_name(err));
        nvs_close(my_handle);
        return err;
    }

    nvs_close(my_handle);
    return ESP_OK;
}

/**
 * 获取多个NVS存储的字符串值
 *
 * @param keys        数组，包含要获取的键
 * @param values      数组，指向用于存储获取值的指针
 * @param num_keys    要获取的键的数量
 * @return esp_err_t  操作结果，ESP_OK表示成功
 */
// esp_err_t a_nvs_flash_get_multiple(const char **keys, char **values, size_t num_keys)
// {
//     nvs_handle_t my_handle;
//     esp_err_t err;

//     // 打开NVS命名空间
//     err = nvs_open("storage", NVS_READONLY, &my_handle);
//     if (err != ESP_OK)
//     {
//         ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
//         return err;
//     }

//     for (size_t i = 0; i < num_keys; ++i)
//     {
//         size_t required_size = 0;

//         // 获取每个键的字符串长度
//         err = nvs_get_str(my_handle, keys[i], NULL, &required_size);
//         if (err != ESP_OK)
//         {
//             ESP_LOGE(TAG, "Error (%s) getting string size for key: %s", esp_err_to_name(err), keys[i]);
//             // 继续尝试获取其他键的值
//             values[i] = NULL;
//             continue;
//         }

//         if (required_size == 0)
//         {
//             ESP_LOGW("NVS", "Key '%s' not found or empty", keys[i]);
//             values[i] = NULL;
//             continue;
//         }

//         // 为每个值分配内存
//         values[i] = malloc(required_size);
//         if (values[i] == NULL)
//         {
//             ESP_LOGE("NVS", "Failed to allocate memory for key: %s", keys[i]);
//             // 无法继续获取该键的值
//             continue;
//         }

//         // 获取实际字符串
//         err = nvs_get_str(my_handle, keys[i], values[i], &required_size);
//         if (err != ESP_OK)
//         {
//             ESP_LOGE("NVS", "Error (%s) getting string for key: %s", esp_err_to_name(err), keys[i]);
//             free(values[i]);
//             values[i] = NULL;
//         }
//     }

//     nvs_close(my_handle);
//     return ESP_OK;
// }

/**
 * 删除NVS中的指定键值对
 * 
 * @param key 要删除的键
 * @return esp_err_t 操作结果，ESP_OK表示成功，其他值表示失败
 */
esp_err_t a_nvs_flash_del(const char *key)
{
    nvs_handle_t my_handle; // 声明一个变量用于存储NVS命名空间的句柄
    esp_err_t err;          // 声明一个变量用于存储函数返回的错误代码

    /**
     * 打开NVS命名空间
     * "storage" 是命名空间的名称，可以根据需要更改
     * NVS_READWRITE 指定打开模式为读写模式，允许读取和写入操作
     */
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "无法打开NVS句柄，错误代码：%s", esp_err_to_name(err));
        return err;
    }

    /**
     * 删除指定键
     * 调用 nvs_erase_key() 函数删除指定的键值对
     * my_handle 是之前打开的NVS命名空间的句柄
     * key 是要删除的键
     */
    err = nvs_erase_key(my_handle, key);
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "键 '%s' 未找到，无需删除。", key);
        nvs_close(my_handle); // 关闭NVS命名空间，释放相关资源
        return ESP_OK;        // 由于键不存在，认为删除操作成功
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "删除键 '%s' 失败，错误代码：%s", key, esp_err_to_name(err));
        nvs_close(my_handle); // 关闭NVS命名空间，释放相关资源
        return err;            // 返回错误代码
    }

    /**
     * 提交更改
     * 调用 nvs_commit() 函数将删除操作提交到NVS存储中
     * 这一步确保删除操作被实际写入到存储中
     */
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "提交删除操作失败，错误代码：%s", esp_err_to_name(err));
        nvs_close(my_handle); // 关闭NVS命名空间，释放相关资源
        return err;            // 返回错误代码
    }

    ESP_LOGI(TAG, "成功删除键 '%s'。", key);
    nvs_close(my_handle); // 关闭NVS命名空间，释放相关资源
    return ESP_OK;        // 返回成功
}