#ifndef U4G_AT_HTTP_H
#define U4G_AT_HTTP_H

#include "u4g_state.h"

emU4GResult u4g_at_http_init(void);
emU4GResult u4g_at_http_request(const char *url, const char *path, const char *body);

#endif