#ifndef ZS_MOD_WEEK_H
#define ZS_MOD_WEEK_H

#include "core/types.h"

/* --------------------------------------------------------- weekview.c 接口 */
void   week_register_class(HINSTANCE hInst);
HWND   week_create(HWND parent, HINSTANCE hInst, int id);
void   week_refresh(HWND h);
void   week_shift(HWND h, int weeks);
void   week_goto_today(HWND h);
void   week_set_sel(HWND h, int id);
int    week_sel_id(HWND h);
void   week_set_focus_time(HWND h, const SYSTEMTIME *st);
void   week_goto_time(HWND h, const SYSTEMTIME *st);
long long week_monday(HWND h);
void   week_zoom(HWND h, int delta);          /* 每档 +1，0 为恢复默认 */

#endif /* ZS_MOD_WEEK_H */
