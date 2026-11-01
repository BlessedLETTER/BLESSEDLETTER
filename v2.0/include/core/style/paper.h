#ifndef ZS_CORE_PAPER_H
#define ZS_CORE_PAPER_H

#include "core/types.h"

/* -------------------------------------------------------------- paper.c */
void      paper_init(void);
void      paper_free(void);
HBRUSH    paper_brush(COLORREF base);           /* 带纤维纹理的纸纹刷 */
void      paper_fill(HDC hdc, const RECT *rc, COLORREF base, int alignOrg);
/* 纸张卡片：带柔和投影 + 细边 + 纹理 */
void      paper_card(HDC hdc, const RECT *rc, COLORREF base, int radius,
                     int lifted, COLORREF border);
void      paper_shadow_only(HDC hdc, const RECT *rc, int radius, int depth);
void      paper_fold_corner(HDC hdc, const RECT *rc, COLORREF base, int size);
void      paper_rules(HDC hdc, const RECT *rc, int top, int step, COLORREF col);
COLORREF  paper_shade(COLORREF c, int delta);
void      paper_notebook(HDC hdc, const RECT *client, int scale,
                         RECT *pageOut, int *contentLeftOut);   /* delta>0 变暗 */

#endif /* ZS_CORE_PAPER_H */
