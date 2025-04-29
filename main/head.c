#include "head.h"

device_info_t DEVICE = {
    .RUNSTATE = NOT_READY,
    .total_water_time = 0,
    .NETSTATE = DEVICE_NETOFF,
    // .TDS_EXISTS = NOT_EXISTS
    .charging = 0,
    .MODE_EXPIRY = NOT_EXPIRY,
    .flowmeter = 0,
    .expire_time = 0,
    .WATER_LEAK = false,
    .duration_s = 15,
};

device_tds_wd_t DEVICE_TDSWD = {
    .pure_tds = 0,
    .pure_temperature = 0,
    .raw_tds = 0,
    .raw_temperature = 0,
};

device_status_t DEVICE_STATUS = {
    .feedback_init = false,
};

bool is_isr_service_installed = false;
