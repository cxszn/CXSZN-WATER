#ifndef GPIO_FLUSH_H
#define GPIO_FLUSH_H

#include "esp_err.h"

typedef struct
{
    uint32_t POWER_ON;              // 开机冲洗xx秒（最大4294967295）
    uint32_t LOW_END;               // 退出低压后，执行x毫秒反冲洗
    uint32_t HIGH_START;            // 进入高压后，执行x毫秒反冲洗
    uint32_t HIGH_END;              // 退出高压后，执行x毫秒反冲洗
    uint32_t WATER_TOTAL;           // 累计制水x毫秒执行反冲洗）
    uint32_t WATER_PRODUCTION_TIME; // 到达累计制水时间后，执行反冲洗x秒
} flush_time_t;
extern flush_time_t FLUSH_TIME;

esp_err_t gpio_flush_data_update(void);
esp_err_t gpio_flush_init(void);
void gpio_flush_process(uint16_t time);

#endif
