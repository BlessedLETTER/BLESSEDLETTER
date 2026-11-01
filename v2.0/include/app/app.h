#ifndef ZS_APP_APP_H
#define ZS_APP_APP_H

#include "core/types.h"
#include "core/media/sound.h"

/* ---------------------------------------------------------- ui_main.c 接口 */
extern HINSTANCE g_hInst;

HWND   main_create(void);
void   main_register_classes(HINSTANCE hInst);
void   main_rebuild_views(void);
void   main_tick(void);
int    main_selected_id(void);
int    main_handle_shortcut(MSG *msg, int isDialogMsg);
void   main_set_status(const wchar_t *msg);
/* 提醒与开机自启动 */
void   remind_tick(void);
void   remind_show_popup(const Schedule *s);
int    app_autostart_get(void);
void   app_autostart_set(int on);
void   main_refresh_all(void);      /* 数据变更后刷新全部界面（含便笺窗） */
int    main_pick_ids(int *out, int maxOut);   /* 取当前选中的日程（支持多选） */

#endif /* ZS_APP_APP_H */
