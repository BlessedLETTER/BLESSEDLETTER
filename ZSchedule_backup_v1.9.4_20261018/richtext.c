/* ============================================================================
 *  ZSchedule - richtext.c
 *  RichEdit 富文本支持：控件类装载、RTF 读写、字符格式（字体/字号/粗斜下划线）、
 *  系统字体枚举。
 *
 *  注：RichEdit 是系统自带组件（msftedit.dll / riched20.dll），这里动态装载，
 *      避免静态导入导致在极老系统上无法启动。
 * ==========================================================================*/
#include "zsched.h"
#include <stdio.h>
#include <richedit.h>

static HMODULE  g_rtDll;
static wchar_t  g_rtClass[64];
static int      g_rtReady;

int rt_init(void)
{
    if (g_rtReady) return 1;

    /* 优先 RichEdit 4.1（msftedit），退回 2.0 */
    g_rtDll = LoadLibraryW(L"Msftedit.dll");
    if (g_rtDll) {
        wcscpy(g_rtClass, MSFTEDIT_CLASS);          /* RICHEDIT50W */
        g_rtReady = 1;
        return 1;
    }
    g_rtDll = LoadLibraryW(L"Riched20.dll");
    if (g_rtDll) {
        wcscpy(g_rtClass, RICHEDIT_CLASSW);         /* RichEdit20W */
        g_rtReady = 1;
        return 1;
    }
    g_rtClass[0] = 0;
    g_rtReady = 0;
    return 0;
}

const wchar_t *rt_class(void) { return g_rtClass; }
int rt_available(void) { return g_rtReady; }

/* ----------------------------------------------------------- RTF 流回调 */
typedef struct {
    char  *buf;
    size_t len;
    size_t cap;
} ByteBuf;

static DWORD CALLBACK rt_out_cb(DWORD_PTR cookie, LPBYTE pb, LONG cb, LONG *pcb)
{
    ByteBuf *bb = (ByteBuf *)cookie;
    *pcb = 0;
    if (!bb || cb <= 0) return 0;
    if (bb->len + (size_t)cb + 1 > bb->cap) {
        size_t nc = bb->cap ? bb->cap : 1024;
        char *np;
        while (nc < bb->len + (size_t)cb + 1) nc *= 2;
        np = (char *)realloc(bb->buf, nc);
        if (!np) return 1;                     /* 返回非 0 表示出错，停止 */
        bb->buf = np;
        bb->cap = nc;
    }
    memcpy(bb->buf + bb->len, pb, (size_t)cb);
    bb->len += (size_t)cb;
    bb->buf[bb->len] = 0;
    *pcb = cb;
    return 0;
}

static DWORD CALLBACK rt_in_cb(DWORD_PTR cookie, LPBYTE pb, LONG cb, LONG *pcb)
{
    ByteBuf *bb = (ByteBuf *)cookie;
    size_t left;
    *pcb = 0;
    if (!bb) return 0;
    left = bb->cap - bb->len;                  /* 以 cap 作为数据长度 */
    if (left == 0) return 0;
    if (left > (size_t)cb) left = (size_t)cb;
    memcpy(pb, bb->buf + bb->len, left);
    bb->len += left;
    *pcb = (LONG)left;
    return 0;
}

/* 取控件内容的 RTF 源码（宽字符）；返回 1 成功 */
int rt_get_rtf(HWND h, wchar_t *out, int cch)
{
    ByteBuf bb;
    EDITSTREAM es;
    int n;

    if (!h || !out || cch <= 0) return 0;
    out[0] = 0;
    memset(&bb, 0, sizeof(bb));
    memset(&es, 0, sizeof(es));
    es.dwCookie = (DWORD_PTR)&bb;
    es.pfnCallback = rt_out_cb;
    SendMessageW(h, EM_STREAMOUT, (WPARAM)(SF_RTF), (LPARAM)&es);
    if (es.dwError || !bb.buf || bb.len == 0) {
        if (bb.buf) free(bb.buf);
        return 0;
    }
    /* RichEdit 输出的是当前 ANSI 代码页字节流（非 ANSI 字符会写成 \'xx），
       按 CP_ACP 转到宽字符即可无歧义地往返。 */
    n = MultiByteToWideChar(CP_ACP, 0, bb.buf, (int)bb.len, out, cch - 1);
    if (n < 0) n = 0;
    out[n] = 0;
    free(bb.buf);
    return n > 0;
}

/* 把 RTF（或纯文本）装进控件；rtf 为空时按纯文本处理 */
void rt_set_text(HWND h, const wchar_t *rtf, const wchar_t *plain)
{
    if (!h) return;
    if (rtf && rtf[0] && !wcsncmp(rtf, L"{\\rtf", 5)) {
        ByteBuf bb;
        EDITSTREAM es;
        int need = WideCharToMultiByte(CP_ACP, 0, rtf, -1, NULL, 0, NULL, NULL);
        if (need > 1) {
            memset(&bb, 0, sizeof(bb));
            bb.buf = (char *)malloc((size_t)need);
            if (bb.buf) {
                WideCharToMultiByte(CP_ACP, 0, rtf, -1, bb.buf, need, NULL, NULL);
                bb.len = 0;                    /* 读取游标 */
                bb.cap = (size_t)need - 1;     /* 数据长度 */
                memset(&es, 0, sizeof(es));
                es.dwCookie = (DWORD_PTR)&bb;
                es.pfnCallback = rt_in_cb;
                SendMessageW(h, EM_STREAMIN, (WPARAM)(SF_RTF), (LPARAM)&es);
                free(bb.buf);
                if (!es.dwError) return;
            }
        }
    }
    /* 退回纯文本 */
    SendMessageW(h, WM_SETTEXT, 0, (LPARAM)(plain ? plain : L""));
}

/* --------------------------------------------------------------- 格式应用 */
void rt_apply(HWND h, const wchar_t *face, int pt,
              int bold, int italic, int underline,
              int colorSet, COLORREF color, int toAll)
{
    CHARFORMAT2W cf;
    DWORD mask = 0;

    if (!h) return;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);

    if (face && face[0]) {
        wcsncpy(cf.szFaceName, face, LF_FACESIZE - 1);
        mask |= CFM_FACE;
    }
    if (pt > 0) {
        cf.yHeight = pt * 20;                 /* 单位：1/20 磅 */
        mask |= CFM_SIZE;
    }
    if (bold >= 0)      mask |= CFM_BOLD;
    if (italic >= 0)    mask |= CFM_ITALIC;
    if (underline >= 0) mask |= CFM_UNDERLINE;
    if (bold > 0)      cf.dwEffects |= CFE_BOLD;
    if (italic > 0)    cf.dwEffects |= CFE_ITALIC;
    if (underline > 0) cf.dwEffects |= CFE_UNDERLINE;
    if (colorSet) {                       /* 文字颜色（0 = 不动，1 = 设为 color，2 = 恢复自动） */
        mask |= CFM_COLOR;
        cf.crTextColor = color;
        if (colorSet == 2) cf.dwEffects |= CFE_AUTOCOLOR;
    }

    /* 未指定项保持原样：用 GET 填充再一次性 SET，避免误清其它属性 */
    if (mask != (CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_COLOR)) {
        CHARFORMAT2W cur;
        memset(&cur, 0, sizeof(cur));
        cur.cbSize = sizeof(cur);
        SendMessageW(h, EM_GETCHARFORMAT,
                     toAll ? SCF_ALL : SCF_SELECTION, (LPARAM)&cur);
        if (!(mask & CFM_FACE)) {
            wcsncpy(cf.szFaceName, cur.szFaceName, LF_FACESIZE - 1);
            mask |= CFM_FACE;
        }
        if (!(mask & CFM_SIZE)) { cf.yHeight = cur.yHeight; mask |= CFM_SIZE; }
        if (!(mask & CFM_BOLD)) { cf.dwEffects |= (cur.dwEffects & CFE_BOLD); mask |= CFM_BOLD; }
        if (!(mask & CFM_ITALIC)) { cf.dwEffects |= (cur.dwEffects & CFE_ITALIC); mask |= CFM_ITALIC; }
        if (!(mask & CFM_UNDERLINE)) { cf.dwEffects |= (cur.dwEffects & CFE_UNDERLINE); mask |= CFM_UNDERLINE; }
        if (!(mask & CFM_COLOR)) {
            cf.crTextColor = cur.crTextColor;
            cf.dwEffects |= (cur.dwEffects & CFE_AUTOCOLOR);
            mask |= CFM_COLOR;
        }
    }
    cf.dwMask = mask;
    SendMessageW(h, EM_SETCHARFORMAT,
                 toAll ? SCF_ALL : SCF_SELECTION, (LPARAM)&cf);
}

/* 读取光标/选区处的字符格式 */
void rt_get_format(HWND h, wchar_t *face, int facecch,
                   int *pt, int *bold, int *italic, int *underline,
                   COLORREF *color, int *autoColor)
{
    CHARFORMAT2W cf;
    if (face && facecch) face[0] = 0;
    if (pt) *pt = 0;
    if (bold) *bold = 0;
    if (italic) *italic = 0;
    if (underline) *underline = 0;
    if (color) *color = 0;
    if (autoColor) *autoColor = 1;
    if (!h) return;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    SendMessageW(h, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    if (face && facecch) {
        wcsncpy(face, cf.szFaceName, (size_t)facecch - 1);
        face[facecch - 1] = 0;
    }
    if (pt) *pt = (int)(cf.yHeight / 20);
    if (bold) *bold = (cf.dwEffects & CFE_BOLD) ? 1 : 0;
    if (italic) *italic = (cf.dwEffects & CFE_ITALIC) ? 1 : 0;
    if (underline) *underline = (cf.dwEffects & CFE_UNDERLINE) ? 1 : 0;
    if (color) *color = cf.crTextColor;
    if (autoColor) *autoColor = (cf.dwEffects & CFE_AUTOCOLOR) ? 1 : 0;
}

/* ------------------------------------------------------------- 字体枚举 */
typedef struct {
    wchar_t (*names)[LF_FACESIZE];
    int      max;
    int      count;
} FontList;

static int font_already(const FontList *fl, const wchar_t *name)
{
    int i;
    for (i = 0; i < fl->count; i++)
        if (!wcscmp(fl->names[i], name)) return 1;
    return 0;
}

static int CALLBACK enum_font_cb(const LOGFONTW *lf, const TEXTMETRICW *tm,
                                 DWORD type, LPARAM lp)
{
    FontList *fl = (FontList *)lp;
    (void)tm; (void)type;
    if (!lf || !lf->lfFaceName[0]) return 1;
    if (lf->lfFaceName[0] == L'@') return 1;         /* 竖排字体跳过 */
    if (font_already(fl, lf->lfFaceName)) return 1;
    if (fl->count >= fl->max) return 0;
    wcsncpy(fl->names[fl->count], lf->lfFaceName, LF_FACESIZE - 1);
    fl->names[fl->count][LF_FACESIZE - 1] = 0;
    fl->count++;
    return 1;
}

int rt_enum_fonts(wchar_t fonts[][LF_FACESIZE], int maxFonts)
{
    HDC hdc;
    LOGFONTW lf;
    FontList fl;

    if (maxFonts <= 0) return 0;
    hdc = GetDC(NULL);
    if (!hdc) return 0;
    memset(&lf, 0, sizeof(lf));
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfFaceName[0] = 0;
    fl.names = fonts;
    fl.max = maxFonts;
    fl.count = 0;
    EnumFontFamiliesExW(hdc, &lf, (FONTENUMPROCW)enum_font_cb, (LPARAM)&fl, 0);
    ReleaseDC(NULL, hdc);
    return fl.count;
}


/* ==========================================================================
 *  富文本分段解析 + 逐段绘制
 *  —— 让便笺上的正文按编辑窗里设定的“逐段字号 / 字体 / 粗斜下划线”原样显示。
 *  解析对象是 RichEdit 输出的 RTF：\fN 选字体表、\fsN 定字号（半磅）、
 *  \b \i \ul 控制字形，分组 {} 之间保存/恢复状态。
 * ========================================================================*/
#define RF_HALF_MIN  10            /* 5pt */
#define RF_HALF_MAX  260           /* 130pt */

typedef struct {
    int sizeHalf, bold, italic, underline, font, skip, color;
} RState;

typedef struct {
    wchar_t face[LF_FACESIZE];
    int     half, bold, italic, underline, scale;
    HFONT   hf;
    int     lh;
} RFont;

#define RFONT_CACHE 64
static RFont   g_rf[RFONT_CACHE];
static int     g_rfCount;
static RichDoc g_rich;

RichDoc *rich_doc(void)
{
    return &g_rich;
}

void rich_fonts_free(void)
{
    int i;
    for (i = 0; i < g_rfCount; i++)
        if (g_rf[i].hf) DeleteObject(g_rf[i].hf);
    g_rfCount = 0;
}

static const RFont *rfont_get(const wchar_t *face, int half, int b, int i, int u, int scale)
{
    RFont *f;
    int k, dpi;
    HDC hdc;

    if (half < RF_HALF_MIN) half = RF_HALF_MIN;
    if (half > RF_HALF_MAX) half = RF_HALF_MAX;
    if (scale < 40) scale = 40;
    if (scale > 1600) scale = 1600;
    if (!face || !face[0]) face = L"Microsoft YaHei UI";
    for (k = 0; k < g_rfCount; k++) {
        f = &g_rf[k];
        if (f->half == half && f->bold == b && f->italic == i &&
            f->underline == u && f->scale == scale && !_wcsicmp(f->face, face))
            return f;
    }
    if (g_rfCount >= RFONT_CACHE) rich_fonts_free();
    hdc = GetDC(NULL);
    dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    f = &g_rf[g_rfCount++];
    wcsncpy(f->face, face, LF_FACESIZE - 1);
    f->face[LF_FACESIZE - 1] = 0;
    f->half = half; f->bold = b; f->italic = i; f->underline = u; f->scale = scale;
    /* half 是半磅：高度 = half/2 pt → -MulDiv(half, dpi*scale, 19200) */
    f->hf = CreateFontW(-MulDiv(half, dpi * scale, 19200), 0, 0, 0,
                        b ? FW_BOLD : FW_NORMAL, i ? TRUE : FALSE, u ? TRUE : FALSE, 0,
                        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH, f->face);
    if (!f->hf) {
        g_rfCount--;
        return rfont_get(L"Microsoft YaHei UI", half, b, i, u, scale);
    }
    {
        TEXTMETRICW tm;
        HDC h2 = GetDC(NULL);
        HFONT old = (HFONT)SelectObject(h2, f->hf);
        tm.tmHeight = 0;
        GetTextMetricsW(h2, &tm);
        SelectObject(h2, old);
        ReleaseDC(NULL, h2);
        f->lh = tm.tmHeight > 0 ? tm.tmHeight : 16;
    }
    return f;
}

/* 跳过目标组（这些组里的内容不是正文） */
static int rich_skip_dest(const wchar_t *w)
{
    return (wcsncmp(w, L"fonttbl", 7) == 0 || wcsncmp(w, L"colortbl", 8) == 0 ||
            wcsncmp(w, L"stylesheet", 10) == 0 || wcsncmp(w, L"info", 4) == 0 ||
            wcsncmp(w, L"pict", 4) == 0 || wcsncmp(w, L"object", 6) == 0 ||
            wcsncmp(w, L"header", 6) == 0 || wcsncmp(w, L"footer", 6) == 0 ||
            wcsncmp(w, L"footnote", 8) == 0 || wcsncmp(w, L"field", 5) == 0 ||
            wcsncmp(w, L"generator", 9) == 0 || wcsncmp(w, L"listtable", 9) == 0 ||
            wcsncmp(w, L"listoverridetable", 17) == 0 ||
            wcsncmp(w, L"rsidtbl", 7) == 0) ? 1 : 0;
}

static const wchar_t *rich_word(const wchar_t *q, int *pval, int *hasval)
{
    int n = 0, v = 0, sign = 1;
    wchar_t w[24];
    while (q[n] && ((q[n] >= L'a' && q[n] <= L'z') || (q[n] >= L'A' && q[n] <= L'Z')) && n < 23) {
        w[n] = q[n];
        n++;
    }
    w[n] = 0;
    q += n;
    *hasval = 0;
    if (*q == L'-') { sign = -1; q++; *hasval = 1; }
    while (*q >= L'0' && *q <= L'9') { v = v * 10 + (*q - L'0'); *hasval = 1; q++; }
    *pval = sign * v;
    if (*q == L' ') q++;
    /* 把词本身通过返回值传出去：调用方用 w 比较不方便，这里用一个静态缓冲 */
    {
        static wchar_t buf[24];
        int k;
        for (k = 0; k <= n; k++) buf[k] = w[k];
        return buf;
    }
}

static int rich_is(const wchar_t *word, const wchar_t *ref)
{
    return !wcscmp(word, ref);
}

/* 字体名里也会出现 \'hh（RichEdit 对非 ASCII 字体名如“华文楷体”就这么写），
   这里把它正确译回文字，否则建字体时回落到默认字体。 */
static void rich_decode_name(const wchar_t *p, const wchar_t *end, wchar_t *out, int cch)
{
    BYTE bytes[512];
    int nb = 0, n = 0;
    while (p < end && n < cch - 1) {
        if (p[0] == L'\\' && p[1] == L'\'' && p + 3 < end + 1) {
            int h1 = p[2], h2 = p[3], v = 0, ok = 1;
            if (h1 >= L'0' && h1 <= L'9') v = h1 - L'0';
            else if (h1 >= L'a' && h1 <= L'f') v = h1 - L'a' + 10;
            else if (h1 >= L'A' && h1 <= L'F') v = h1 - L'A' + 10;
            else ok = 0;
            v <<= 4;
            if (h2 >= L'0' && h2 <= L'9') v |= h2 - L'0';
            else if (h2 >= L'a' && h2 <= L'f') v |= h2 - L'a' + 10;
            else if (h2 >= L'A' && h2 <= L'F') v |= h2 - L'A' + 10;
            else ok = 0;
            if (ok) {
                if (nb < (int)sizeof(bytes) - 2) bytes[nb++] = (BYTE)v;
                p += 4;
                continue;
            }
        }
        if (nb > 0) {
            wchar_t tmp[512];
            int cnt = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)bytes, nb, tmp, 512);
            int k;
            for (k = 0; k < cnt && n < cch - 1; k++) out[n++] = tmp[k];
            nb = 0;
        }
        out[n++] = *p++;
    }
    if (nb > 0) {
        wchar_t tmp[512];
        int cnt = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)bytes, nb, tmp, 512);
        int k;
        for (k = 0; k < cnt && n < cch - 1; k++) out[n++] = tmp[k];
    }
    while (n > 0 && (out[n - 1] == L' ' || out[n - 1] == L';')) n--;
    out[n] = 0;
}

void rich_parse(const wchar_t *rtf, const wchar_t *plain, RichDoc *doc)
{
    RState st, stack[48];
    int sp = 0;
    const wchar_t *p;
    BYTE bytes[1024];
    int nb = 0;
    int guard = 0;

    memset(doc, 0, sizeof(*doc));

#define RD_SAME(r) ((r)->sizeHalf == st.sizeHalf && (r)->bold == st.bold && \
                    (r)->italic == st.italic && (r)->underline == st.underline && \
                    (r)->font == st.font && (r)->color == st.color)

#define RD_PUT(ch)                                                            \
    do {                                                                      \
        if (doc->len < MAX_NOTE) {                                            \
            RichRun *rr = (doc->runCount > 0) ? &doc->runs[doc->runCount - 1] : NULL; \
            if (rr && rr->off + rr->len == doc->len && RD_SAME(rr))           \
                rr->len++;                                                    \
            else {                                                            \
                if (doc->runCount < MAX_RUNS) {                               \
                    rr = &doc->runs[doc->runCount++];                         \
                    rr->off = doc->len; rr->len = 1; rr->sizeHalf = st.sizeHalf; \
                    rr->bold = st.bold; rr->italic = st.italic;               \
                    rr->underline = st.underline; rr->font = st.font;         \
                    rr->color = st.color;                                     \
                }                                                            \
            }                                                                \
            doc->text[doc->len++] = (ch);                                     \
        }                                                                    \
    } while (0)

#define RD_FLUSH()                                                            \
    do {                                                                      \
        if (nb > 0) {                                                         \
            wchar_t tmp[1026];                                                \
            int cnt = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)bytes, nb, tmp, 1024); \
            int kk;                                                           \
            if (cnt > 0) for (kk = 0; kk < cnt; kk++) RD_PUT(tmp[kk]);        \
            nb = 0;                                                           \
        }                                                                     \
    } while (0)

    if (!rtf || wcsncmp(rtf, L"{\\rtf", 5) != 0) {           /* 纯文本：一段 */
        int n = 0;
        if (plain) for (; plain[n] && n < MAX_NOTE; n++) doc->text[n] = plain[n];
        doc->text[n] = 0;
        doc->len = n;
        if (n > 0) {
            doc->runs[0].off = 0;
            doc->runs[0].len = n;
            doc->runs[0].font = -1;
            doc->runs[0].color = 0;
            doc->runCount = 1;
        }
        return;
    }

    /* ---- 字体表：{\fN\fnil\fcharset134 字体名;} ---- */
    {
        const wchar_t *ft = wcsstr(rtf, L"{\\fonttbl");
        if (ft) {
            const wchar_t *q = ft + 10;
            while (*q && doc->faceCount < MAX_RUN_FONTS && guard++ < 20000) {
                if (*q == L'\\' && q[1] == L'f' && q[2] >= L'0' && q[2] <= L'9') {
                    int idx = (int)wcstol(q + 2, NULL, 10);
                    const wchar_t *nm = q + 2;
                    const wchar_t *e;
                    while (*nm && *nm != L' ' && *nm != L';' && *nm != L'}' && *nm != L'{') nm++;
                    if (*nm == L' ') nm++;
                    while (*nm == L'\\' && nm[1] != L'\'') {   /* 跳过 \fnil \fcharset134（但别吃掉 \'hh） */
                        nm++;
                        while (*nm && *nm != L' ' && *nm != L'\\' && *nm != L';') nm++;
                        if (*nm == L' ') nm++;
                    }
                    e = nm;
                    while (*e && *e != L';' && *e != L'}' && *e != L'{') e++;
                    if (idx >= 0 && idx < MAX_RUN_FONTS && e > nm) {
                        rich_decode_name(nm, e, doc->faces[idx], LF_FACESIZE);
                        if (doc->faces[idx][0] && idx + 1 > doc->faceCount)
                            doc->faceCount = idx + 1;
                    }
                    q = e;
                    continue;
                }
                if (*q == L'}') {
                    const wchar_t *z = wcsstr(q, L"\\f");
                    if (!z || z > q + 2) break;
                }
                q++;
            }
        }
    }

    /* ---- 颜色表：{\colortbl;\red255\green0\blue0;...}（下标 0 = 自动） ---- */
    {
        const wchar_t *ct = wcsstr(rtf, L"{\\colortbl");
        if (ct) {
            const wchar_t *q = ct + 10;
            int idx = 0, stored = 0;
            doc->colors[0] = 0;
            doc->colorCount = 1;
            /* \colortbl;\red..;\red..; 第一个分号结束“自动色”那一项 */
            while (*q && *q != L'}' && guard++ < 20000) {
                if (*q == L'\\' && q[1] == L'r' && !wcsncmp(q + 1, L"red", 3)) {
                    int rr = 0, gg = 0, bb = 0;
                    const wchar_t *z = q + 4;
                    while (*z >= L'0' && *z <= L'9') { rr = rr * 10 + (*z - L'0'); z++; }
                    while (*z && *z != L'\\' && *z != L';') z++;
                    if (!wcsncmp(z, L"\\green", 6)) {
                        z += 6;
                        while (*z >= L'0' && *z <= L'9') { gg = gg * 10 + (*z - L'0'); z++; }
                    }
                    while (*z && *z != L'\\' && *z != L';') z++;
                    if (!wcsncmp(z, L"\\blue", 5)) {
                        z += 5;
                        while (*z >= L'0' && *z <= L'9') { bb = bb * 10 + (*z - L'0'); z++; }
                    }
                    if (idx >= 0 && idx < MAX_RUN_COLORS) {
                        doc->colors[idx] = RGB(rr > 255 ? 255 : rr, gg > 255 ? 255 : gg, bb > 255 ? 255 : bb);
                        if (idx + 1 > doc->colorCount) doc->colorCount = idx + 1;
                        idx++;
                        stored = 1;
                    }
                    q = z;
                    continue;
                }
                if (*q == L';') {
                    if (!stored) idx++;      /* 空项（比如开头那个分号）才推进下标 */
                    stored = 0;
                    q++;
                    continue;
                }
                q++;
            }
        }
    }

    /* ---- 正文 ---- */
    st.sizeHalf = 0; st.bold = st.italic = st.underline = 0; st.font = -1; st.skip = 0; st.color = 0; st.color = 0;
    p = rtf;
    guard = 0;
    while (*p && guard++ < 400000) {
        if (*p == L'{') {
            if (sp < 47) stack[sp++] = st;
            if (p[1] == L'\\') {
                if (p[2] == L'*' || rich_skip_dest(p + 2)) st.skip = 1;
            }
            p++;
            continue;
        }
        if (*p == L'}') {
            RD_FLUSH();
            if (sp > 0) st = stack[--sp];
            p++;
            continue;
        }
        if (st.skip) { p++; continue; }
        if (*p == L'\\') {
            const wchar_t *q = p + 1;
            if (*q == L'\\' || *q == L'{' || *q == L'}') {
                RD_FLUSH();
                RD_PUT(*q);
                p = q + 1;
                continue;
            }
            if (*q == L'\'') {
                int h1 = q[1], h2 = q[2], v = 0, ok = 1;
                if (h1 >= L'0' && h1 <= L'9') v = h1 - L'0';
                else if (h1 >= L'a' && h1 <= L'f') v = h1 - L'a' + 10;
                else if (h1 >= L'A' && h1 <= L'F') v = h1 - L'A' + 10;
                else ok = 0;
                v <<= 4;
                if (h2 >= L'0' && h2 <= L'9') v |= h2 - L'0';
                else if (h2 >= L'a' && h2 <= L'f') v |= h2 - L'a' + 10;
                else if (h2 >= L'A' && h2 <= L'F') v |= h2 - L'A' + 10;
                else ok = 0;
                if (ok) {
                    if (nb >= (int)sizeof(bytes) - 4) RD_FLUSH();
                    bytes[nb++] = (BYTE)v;
                }
                p = q + 3;
                continue;
            }
            if (*q == L'*') { p = q + 1; continue; }
            if (*q == L'~') { RD_FLUSH(); RD_PUT(L' '); p = q + 1; continue; }
            if (*q == L'-' || *q == L'_') { p = q + 1; continue; }
            if ((*q == L'u' || *q == L'U') &&
                (q[1] == L'-' || (q[1] >= L'0' && q[1] <= L'9'))) {
                const wchar_t *d = q + 1;
                int sign = 1, v = 0;
                RD_FLUSH();
                if (*d == L'-') { sign = -1; d++; }
                while (*d >= L'0' && *d <= L'9') { v = v * 10 + (*d - L'0'); d++; }
                if (*d == L'?') d++;
                if (v > 0) RD_PUT((wchar_t)(short)(sign * v));
                p = d;
                continue;
            }
            {
                const wchar_t *d;
                int val = 0, has = 0;
                const wchar_t *word = rich_word(q, &val, &has);
                const wchar_t *q2 = q;
                while (*q2 && ((*q2 >= L'a' && *q2 <= L'z') || (*q2 >= L'A' && *q2 <= L'Z'))) q2++;
                if (*q2 == L'-') q2++;
                while (*q2 >= L'0' && *q2 <= L'9') q2++;
                if (*q2 == L' ') q2++;
                d = q2;
                RD_FLUSH();
                if (rich_is(word, L"par") || rich_is(word, L"line")) RD_PUT(L'\n');
                else if (rich_is(word, L"tab")) RD_PUT(L'\t');
                else if (rich_is(word, L"b"))  st.bold = has ? (val != 0) : 1;
                else if (rich_is(word, L"i"))  st.italic = has ? (val != 0) : 1;
                else if (rich_is(word, L"ul")) st.underline = has ? (val != 0) : 1;
                else if (rich_is(word, L"ulnone")) st.underline = 0;
                else if (rich_is(word, L"f") && has) st.font = val;
                else if (rich_is(word, L"fs") && has) st.sizeHalf = val;
                else if (rich_is(word, L"cf") && has) st.color = (val > 0) ? val : 0;
                p = d;
                continue;
            }
        }
        if (*p == L'\r' || *p == L'\n') { p++; continue; }
        RD_FLUSH();
        RD_PUT(*p);
        p++;
    }
    RD_FLUSH();
#undef RD_PUT
#undef RD_FLUSH
#undef RD_SAME
}

/* 逐段绘制（draw=0 时只量高度）；返回内容总高 */
int rich_draw(HDC hdc, const RichDoc *doc, const RECT *box,
              int baseHalfPt, int scalePct, COLORREF color, int draw)
{
    int x = box->left, y = box->top;
    int right = box->right;
    int i, lineH = 0;
    HFONT old = NULL;

    if (!doc || doc->runCount <= 0 || right <= x) return 0;
    if (draw) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, color);
    }
    for (i = 0; i < doc->runCount; i++) {
        const RichRun *r = &doc->runs[i];
        const wchar_t *face = L"Microsoft YaHei UI";
        const RFont *rf;
        const wchar_t *t;
        int len, k = 0, half;

        if (r->font >= 0 && r->font < MAX_RUN_FONTS && doc->faces[r->font][0])
            face = doc->faces[r->font];
        half = (r->sizeHalf > 0) ? r->sizeHalf : baseHalfPt;
        rf = rfont_get(face, half, r->bold, r->italic, r->underline, scalePct);
        if (!rf || !rf->hf) continue;
        t = doc->text + r->off;
        len = r->len;
        if (len <= 0) continue;
        old = (HFONT)SelectObject(hdc, rf->hf);
        if (draw) {                            /* 逐段颜色（\cfN），0 = 用便笺默认字色 */
            COLORREF rc2 = color;
            if (r->color > 0 && r->color < doc->colorCount)
                rc2 = doc->colors[r->color];
            SetTextColor(hdc, rc2);
        }
        if (lineH < rf->lh) lineH = rf->lh;

        while (k < len) {
            int remain, fit = 0, q, cut;
            SIZE wsz;

            if (t[k] == L'\n') {                 /* 段落结束 */
                x = box->left;
                y += lineH;
                lineH = rf->lh;
                k++;
                continue;
            }
            remain = right - x;
            if (remain < 4) {                     /* 这一行到头了：换行 */
                x = box->left;
                y += lineH;
                lineH = rf->lh;
                continue;
            }
            cut = len - k;                        /* 最多取到本 run 结束 */
            for (q = 0; q < cut; q++)             /* 行内不能跨过换行符 */
                if (t[k + q] == L'\n') { cut = q; break; }
            if (cut <= 0) {
                x = box->left;
                y += lineH;
                lineH = rf->lh;
                k++;
                continue;
            }
            wsz.cx = 0;
            GetTextExtentPoint32W(hdc, t + k, cut, &wsz);
            if (wsz.cx <= remain) {               /* 整段放得下 */
                fit = cut;
            } else {                              /* 二分找能放下的最长前缀 */
                int lo = 1, hi = cut, best = 0;
                while (lo <= hi) {
                    int mid = (lo + hi) / 2;
                    SIZE s2;
                    s2.cx = 0;
                    GetTextExtentPoint32W(hdc, t + k, mid, &s2);
                    if (s2.cx > 0 && s2.cx <= remain) {
                        best = mid;
                        wsz.cx = s2.cx;
                        lo = mid + 1;
                    } else {
                        hi = mid - 1;
                    }
                }
                fit = best;
            }
            if (fit <= 0) {                       /* 一个字都放不下 */
                if (x > box->left) {              /* 换行重试 */
                    x = box->left;
                    y += lineH;
                    lineH = rf->lh;
                    continue;
                }
                fit = 1;                          /* 行宽比一个字还窄：硬放一个，避免死循环 */
                wsz.cx = 0;
                GetTextExtentPoint32W(hdc, t + k, 1, &wsz);
            }
            if (draw) TextOutW(hdc, x, y, t + k, fit);
#ifdef ZS_RT_DEBUG
            printf("  i=%d k=%d fit=%d remain=%d wx=%d x=%d y=%d lh=%d\n",
                   i, k, fit, remain, (int)wsz.cx, x, y, lineH);
#endif
            x += wsz.cx;
            k += fit;
        }
    }
    if (old) SelectObject(hdc, old);
    return (y - box->top) + (lineH > 0 ? lineH : 0);
}
