#ifndef ZS_CORE_IMAGE_H
#define ZS_CORE_IMAGE_H

#include "core/types.h"

int            path_is_image(const wchar_t *path);
/* ------------------------------------------------------------ image.c 接口 */
int   img_init(void);
void  img_shutdown(void);
int   img_is_image(const wchar_t *path);
int   img_size(const wchar_t *path, int *pw, int *ph);
void  img_draw(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);
void  img_draw_cover(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);
void  img_draw_cover_full(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);
/* v1.9：等比“完整显示”（contain，允许放大、不裁剪）——桌面便笺的照片用这个 */
void  img_draw_fit(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);
int   sched_image_count(const Schedule *s);
const wchar_t *sched_image_at(const Schedule *s, int k, wchar_t *buf, size_t cch);

/* 便笺上图片框的几何（便笺墙与桌面便笺共用） */
#define IMG_MIN_W     72          /* 图片最小尺寸（逻辑像素） */
#define IMG_MIN_H     54
#define IMG_HDL       11          /* 手柄边长（逻辑像素） */
enum { IMG_HDL_NONE = 0, IMG_HDL_L = 1, IMG_HDL_R = 2, IMG_HDL_T = 4, IMG_HDL_B = 8 };

int   img_box_in(const RECT *content, const Schedule *s, int minTextH, int minImgH,
                 int minImgW, RECT *out);   /* 返回 1 = 有图片框 */
void  img_box_store(const RECT *content, const RECT *box, Schedule *s);
int   img_handle_at(const RECT *box, int x, int y, int hdl);   /* 命中哪个手柄 */
void  img_draw_zoom(HDC hdc, const wchar_t *path, int x, int y, int w, int h,
                    int zoomIn);
void  img_draw_file_icon(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);

#endif /* ZS_CORE_IMAGE_H */
