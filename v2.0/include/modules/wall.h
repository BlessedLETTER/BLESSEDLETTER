#ifndef ZS_MOD_WALL_H
#define ZS_MOD_WALL_H

#include "core/types.h"

/* ----------------------------------------------------------- stickies.c */
void   stickies_register_class(HINSTANCE hInst);
HWND   stickies_create(HWND parent, HINSTANCE hInst, int id);
void   stickies_set_items(HWND h, const int *ids, int count);
int    stickies_sel(HWND h);
void   stickies_set_sel(HWND h, int id);
void   stickies_save_pos(void);
void   stickies_refresh(HWND h);
int    stickies_is_pinned(int id);
void   stickies_set_pinned(int id, int on);
int    stickies_has_size(int id);
void   stickies_reset_size(int id);
void   stickies_reset_size_all(void);
void   stickies_reset_pos(int id);
void   stickies_reset_all(void);
int    stickies_is_hidden(int id);
void   stickies_set_hidden(int id, int on);
int    stickies_hidden_count(void);
void   stickies_show_all(void);
void   stickies_hide_all(int on);
void   stickies_path_forget(void);          /* 换数据目录后失效便笺墙状态文件路径 */
/* 便笺墙翻页（v1.6）：一页放不下就新开一页 */
int    stickies_page_get(HWND h);           /* 当前第几页（1 起） */
int    stickies_page_count(HWND h);         /* 一共几页 */
int    stickies_page_full(HWND h);          /* 1 = 这一页装满了 */
int    stickies_page_of(int id);            /* 这张便笺在第几页 */
void   stickies_page_set(HWND h, int page);
void   stickies_page_new(HWND h);           /* 新开一页并翻过去 */
void   stickies_move_page(int id, int delta);  /* 把便笺移到上/下一页（-1/+1） */

#endif /* ZS_MOD_WALL_H */
