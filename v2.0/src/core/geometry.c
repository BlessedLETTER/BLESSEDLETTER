/* ZSchedule v2.0 · core/geometry.c
   RECT/工作区/窗口居中：编辑器、CSV 对话框、主窗口共用。 */
#include "core/types.h"
#include "core/geometry.h"

void zs_work_area(RECT *rc)
{
    if (!rc) return;
    if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, rc, 0)) {
        rc->left = 0;
        rc->top = 0;
        rc->right = GetSystemMetrics(SM_CXSCREEN);
        rc->bottom = GetSystemMetrics(SM_CYSCREEN);
    }
}

void zs_center_window(HWND h)
{
    RECT wr, wa;
    MONITORINFO mi;
    if (!h) return;
    GetWindowRect(h, &wr);
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi))
        wa = mi.rcWork;
    else
        zs_work_area(&wa);
    SetWindowPos(h, NULL,
                 wa.left + (wa.right - wa.left - (wr.right - wr.left)) / 2,
                 wa.top + (wa.bottom - wa.top - (wr.bottom - wr.top)) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}
