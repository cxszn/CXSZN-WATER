idf_component_register(
    SRCS "src/u4g_uart.c"
    "src/u4g_uart_recv.c"
    "src/u4g_at_http.c"
    "src/u4g_at_cmd.c"
    "src/u4g_data.c"
    "src/u4g_at_buffer.c"
    "src/u4g_core.c"
    "src/u4g_utils.c"
    "src/u4g.c"
    INCLUDE_DIRS "include"
    REQUIRES "driver" "freertos" "mbedtls" "app_update" "esp_timer"
)
