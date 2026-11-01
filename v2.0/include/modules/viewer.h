#ifndef ZS_MOD_VIEWER_H
#define ZS_MOD_VIEWER_H

#include "core/types.h"

/* ---------------------------------------------------------- imgview.c 接口 */
void   iv_register_class(HINSTANCE hInst);
HWND   iv_open(HWND owner, const wchar_t *const *paths, int count, int startIndex);

#endif /* ZS_MOD_VIEWER_H */
