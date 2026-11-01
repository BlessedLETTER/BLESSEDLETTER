#ifndef ZS_UI_FLAT_H
#define ZS_UI_FLAT_H

#include "core/types.h"

/* ------------------------------------------------------------- 扁平按钮 */
void   flat_register_class(HINSTANCE hInst);
void   flat_set_kind(HWND h, int primary, int toggle);
HWND   flat_create(HWND parent, const wchar_t *text, int id, int primary, int toggle);
void   flat_check(HWND h, int checked);

#endif /* ZS_UI_FLAT_H */
