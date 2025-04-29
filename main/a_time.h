/*
 * @function: 
 * @Description: 
 * @Author: 初馨顺之诺
 * @contact: 微信：cxszn01
 * @link: shop.cxszn.com
 * @Date: 2024-12-23 11:36:07
 * @LastEditTime: 2024-12-23 11:37:17
 * @FilePath: \CXSZN-WATER\main\app_time.h
 * Copyright (c) 2024 by 临沂初馨顺之诺网络科技中心, All Rights Reserved. 
 */
#ifndef A_TIME_H
#define A_TIME_H

#include "esp_err.h"

esp_err_t a_time_sync(void);
esp_err_t a_time_sync_offline(void);
esp_err_t a_time_save(void);

#endif