/* ZSchedule v2.0 · core/status.c
   状态栏/刷新服务总线：modules 只调用这里，app 在启动时注册真正的实现。 */
#include "core/types.h"
#include "core/status.h"

static void (*g_status_fn)(const wchar_t *msg);
static void (*g_refresh_fn)(void);

void zs_status_set_handler(void (*fn)(const wchar_t *msg))
{
    g_status_fn = fn;
}

void zs_status(const wchar_t *msg)
{
    if (g_status_fn) g_status_fn(msg);
}

void zs_refresh_set_handler(void (*fn)(void))
{
    g_refresh_fn = fn;
}

void zs_request_refresh(void)
{
    if (g_refresh_fn) g_refresh_fn();
}
