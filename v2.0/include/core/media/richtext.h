#ifndef ZS_CORE_RICHTEXT_H
#define ZS_CORE_RICHTEXT_H

#include "core/types.h"

void           rtf_to_plain(const wchar_t *rtf, wchar_t *out, size_t cch);
/* --------------------------------------------------------- richtext.c 接口 */
int    rt_init(void);
const wchar_t *rt_class(void);
int    rt_available(void);
void   rt_set_text(HWND h, const wchar_t *rtf, const wchar_t *plain);
int    rt_get_rtf(HWND h, wchar_t *out, int cch);
void   rt_apply(HWND h, const wchar_t *face, int pt,
                int bold, int italic, int underline,
                int colorSet, COLORREF color, int toAll);
void   rt_get_format(HWND h, wchar_t *face, int facecch,
                     int *pt, int *bold, int *italic, int *underline,
                     COLORREF *color, int *autoColor);
int    rt_enum_fonts(wchar_t fonts[][LF_FACESIZE], int maxFonts);

/* ------------------------------------------- 富文本分段（便笺逐段字号/字体） */
#define MAX_RUNS       320
#define MAX_RUN_FONTS  16
#define MAX_RUN_COLORS 24      /* RTF 颜色表上限（\cfN 用） */

typedef struct {
    int off, len;                 /* 在 RichDoc.text 中的范围 */
    int sizeHalf;                 /* 字号（半磅），0 = 用便笺默认 */
    int bold, italic, underline;
    int font;                     /* 字体表下标，-1 = 默认 */
    int color;                    /* 颜色表下标，0 = 自动（用便笺默认字色） */
} RichRun;

typedef struct {
    wchar_t  text[MAX_NOTE + 8];
    int      len;
    RichRun  runs[MAX_RUNS];
    int      runCount;
    wchar_t  faces[MAX_RUN_FONTS][LF_FACESIZE];
    int      faceCount;
    COLORREF colors[MAX_RUN_COLORS];   /* RTF \colortbl（下标 0 = 自动） */
    int      colorCount;
} RichDoc;
RichDoc *rich_doc(void);                 /* 静态实例（画制单线程） */
void rich_parse(const wchar_t *rtf, const wchar_t *plain, RichDoc *doc);
int  rich_draw(HDC hdc, const RichDoc *doc, const RECT *box,
               int baseHalfPt, int scalePct, COLORREF color, int draw);
void rich_fonts_free(void);

#endif /* ZS_CORE_RICHTEXT_H */
