#ifndef ZS_MOD_NOTES_H
#define ZS_MOD_NOTES_H

#include "core/types.h"

void   note_set_main(HWND h);          /* 桌面便笺把消息回到主窗口 */
/* ----------------------------------------------------------- noteview.c */
void      note_register_class(HINSTANCE hInst);
void      note_open(int schedId, int style, int colorIndex, int activate);
void      note_open_batch(const int *ids, int count, int style, int colorIndex);
void      note_close_all(void);
void      note_refresh_all(void);
void      note_save_state(void);
void      note_load_state(void);
int       note_is_open(int id);
int       note_count(void);
void      note_set_style(int id, int style);
void      note_toggle_top(int id);
void      note_set_color(int id, int colorIndex);
void      note_close_one(int id);
void      note_get_look(int id, int *style, int *colorIndex);
int       note_list(int *ids, int max);                /* 当前打开的便笺 id 列表 */
int       note_get_geom(int id, int *style, int *colorIndex, RECT *rc);
void      note_open_at(int id, int style, int colorIndex, const RECT *rc, int activate);
int       note_look_remembered(int id, int *style, int *colorIndex);
int       note_tt_auto_get(void);                      /* 课表便笺自动跟随 */
void      note_tt_auto_set(int on);
int       note_tt_target_get(void);                    /* 自动接力只改这一张便笺 */
void      note_tt_target_set(int id);
void      note_path_forget(void);                      /* 换数据目录后失效便笺状态文件路径 */
void      note_set_bookmark(int id, int on);           /* 书签形态：吸附屏幕边缘 */
int       note_is_bookmark(int id);
extern const COLORREF g_noteColors[NOTE_COLOR_COUNT];
extern const wchar_t *g_noteColorNames[NOTE_COLOR_COUNT];

#endif /* ZS_MOD_NOTES_H */
