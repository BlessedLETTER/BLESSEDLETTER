#ifndef ZS_CORE_STATUS_H
#define ZS_CORE_STATUS_H

#include "core/types.h"

/* 状态栏消息总线：app 注入 */

void zs_status_set_handler(void (*fn)(const wchar_t *msg));
void zs_status(const wchar_t *msg);
void zs_refresh_set_handler(void (*fn)(void));
void zs_request_refresh(void);

#endif /* ZS_CORE_STATUS_H */
