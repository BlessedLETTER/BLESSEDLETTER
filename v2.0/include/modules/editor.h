#ifndef ZS_MOD_EDITOR_H
#define ZS_MOD_EDITOR_H

#include "core/types.h"

/* ---------------------------------------------------------- ui_edit.c 接口 */
int    edit_show(HWND owner, Schedule *item, int isNew);
int    edit_show_at(HWND owner, Schedule *item, const SYSTEMTIME *prefStart);
void   edit_register_class(HINSTANCE hInst);

#endif /* ZS_MOD_EDITOR_H */
