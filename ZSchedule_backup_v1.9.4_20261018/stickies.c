/* ============================================================================
 *  ZSchedule - stickies.c
 *  “贴在笔记本页面上的便笺”清单视图：
 *    · 便笺在页面上按行流式排布（自适应窗口宽度，支持大小不一的便笺）
 *    · 每张便笺有投影、左上角胶带、右下角折角，像真的贴上去
 *    · 【自由挪动】按住便笺拖到任意位置，位置被记住（拖出流式区即“脱队”）
 *    · 【固定】固定后的便笺被锁住：不能拖动、不能缩放，避免误操作
 *    · 【调整大小】右下角手柄 / 右边、下边手柄拖动即可改尺寸，尺寸被记住
 *    · 【内容自适应】字号、行距、内边距随便笺尺寸等比缩放，放不下就逐级省略
 *    · 单击选中（顶部出现图钉）、双击编辑、右键菜单、方向键移动选择
 * ==========================================================================*/
#include "zsched.h"
#include <windowsx.h>
#include <math.h>

#define STK_CLASS     L"ZScheduleStickyBoard"
#define NOTE_W        208          /* 便笺默认逻辑尺寸 */
#define NOTE_H        150
#define NOTE_MIN_W    132          /* 便笺可缩放范围（逻辑像素） */
#define NOTE_MIN_H    104
#define GAP_X         16
#define GAP_Y         18
#define PAD_X         52           /* 左侧留出装订区 */
#define PAD_TOP       14
#define PAD_RIGHT     26
#define PAD_BOTTOM    18
#define DRAG_THRESHOLD 4           /* 超过这个位移才算拖动（逻辑像素） */
#define SNAP          4            /* 拖动/缩放吸附网格（逻辑像素） */
#define GRIP          22           /* 右下角缩放手柄边长（逻辑像素） */
#define EDGE          10           /* 右/下缩放边厚度（逻辑像素） */
#define FONT_SLOTS    36

#define GD_R          1            /* 手柄位：右边 */
#define GD_B          2            /* 手柄位：下边 */

/* 调试：用 -DZS_DEBUG_STK 编译时把命中/拖动/缩放写进 zsched_dbg.txt */
#ifdef ZS_DEBUG_STK
#include <stdio.h>
static void dbg(const char *fmt, ...)
{
    wchar_t path[MAX_PATH];
    const wchar_t *p = model_path();
    const wchar_t *slash = NULL, *q;
    size_t n;
    FILE *f;
    va_list ap;
    if (!p || !p[0]) return;
    for (q = p; *q; q++) if (*q == L'\\' || *q == L'/') slash = q;
    n = slash ? (size_t)(slash - p + 1) : 0;
    if (n > MAX_PATH - 24) n = MAX_PATH - 24;
    memcpy(path, p, n * sizeof(wchar_t));
    path[n] = 0;
    wcscat(path, L"zsched_dbg.txt");
    f = _wfopen(path, L"a");
    if (!f) return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fclose(f);
}
#else
#define dbg(...) ((void)0)
#endif

typedef struct {
    int id;
    int x, y;          /* 自定义位置（逻辑像素，相对内容原点）；-1 = 未自定义 */
    int w, h;          /* 自定义尺寸（逻辑像素）；0 = 默认 */
    int pinned;        /* 1 = 固定，不能拖动/缩放 */
    int hidden;        /* 1 = 在笔记本里收起来（不占位、不画） */
    int page;          /* 贴在第几页（1 起；0/1 都算第 1 页） */
} BoardPos;

typedef struct {
    int   key;                     /* 绝对缩放百分比（已量化到 4%） */
    HFONT hTitle, hSmall, hTiny, hNum, hMid;
    int   lhTitle, lhSmall, lhTiny, lhNum, lhMid;
} StkFonts;

typedef struct {
    HWND   hwnd;
    int   *ids;
    int    count;
    int    selId;
    int    hotId;
    int    hotTracking;
    int    scrollY;
    int    contentH;
    int    clientW, clientH;
    int    cols;
    int    noteW, noteH, gapX, gapY, padX, padTop, padRight, padBottom;
    int    scale;
    HFONT  hTitle, hSmall, hTiny, hNum;
    int    lhTitle, lhSmall, lhTiny, lhNum;
    RECT  *rects;                  /* 流式布局缓存（动态数组，v1.6 起不再有张数上限） */
    int   *z;                      /* 绘制顺序（动态数组） */
    int    arrCap;                 /* rects/z 的容量 */
    int    page;                   /* 当前第几页（1 起） */
    int    pageCount;              /* 一共几页（至少 1） */
    int    pageFull;               /* 1 = 这一页装不下（可以新开一页） */
    int    footerH;                /* 页脚高度（页码 / 翻页 / 新页） */
    RECT   fPrev, fNext, fNew;     /* 页脚按钮命中矩形 */
    int    layoutOk;
    int    dbgCount;
    int    zCount;                 /* z[] \u91cc\u771f\u6b63\u6709\u6548\u7684\u9879\u6570\uff08\u753b\u7684\u65f6\u5019\u53ea\u80fd\u8d70\u8fd9\u4e48\u591a\uff09 */
    /* 拖动状态 */
    int    dragId;
    int    dragging;
    int    pressId;
    POINT  pressPt;
    RECT   pressRect;
    POINT  dragDelta;
    /* 缩放状态 */
    int    resizing;
    int    resizeId;
    int    resizeGrip;
    int    rszW0, rszH0;
    POINT  rszPt0;
    /* 照片操作状态 */
    int    imgMode;      /* 0 无 1 移动照片 2 缩放照片 */
    int    imgHotIdx;    /* 鼠标在第几张照片上 */
    int    imgSelIdx;    /* 正在拖的是第几张 */
    int    imgId;
    int    imgHdl;       /* 缩放时的手柄位 */
    int    imgHot;       /* 鼠标停在照片上（画虚线框） */
    POINT  imgPt0;
    RECT   imgBox0, imgContent0;
    RECT   imgArea0;          /* 按下时照片可用的纵向范围（四角缩放的上限） */
} Sticky;

static Sticky  *g_stk;
static BoardPos *g_pos;            /* 动态：便笺墙位置表（不再有 512 条上限） */
static int      g_posCap;
static int      g_posCount;
static int      g_viewPage = 1;    /* 上次看的是第几页（存盘，下次回来还在这一页） */
static int      g_pagesSaved = 1;  /* 存盘里记录过的总页数（空页也能留到下次） */
static int      g_boardInit = 1;   /* 第一次铺墙：历史便笺缺省在第 1 页（不是“当前页”） */
static int      g_posLoaded;
static wchar_t  g_posPath[MAX_PATH];
static StkFonts g_fonts[FONT_SLOTS];
static int      g_fontCount;

/* ======================================================================
 *  自定义位置/尺寸表（持久化到 zsched_board.txt）
 * ====================================================================*/
static void pos_path_init(void)
{
    const wchar_t *p;
    const wchar_t *slash;
    size_t n;
    if (g_posPath[0]) return;
    p = model_path();
    if (!p || !p[0]) return;
    slash = NULL;
    {
        const wchar_t *q;
        for (q = p; *q; q++)
            if (*q == L'\\' || *q == L'/') slash = q;
    }
    n = slash ? (size_t)(slash - p + 1) : 0;
    if (n >= MAX_PATH - 24) n = MAX_PATH - 24;
    memcpy(g_posPath, p, n * sizeof(wchar_t));
    g_posPath[n] = 0;
    wcscat(g_posPath, L"zsched_board.txt");
}

void stickies_path_forget(void)
{
    g_posPath[0] = 0;
    g_posLoaded = 0;
}

static int pos_ensure(int need)
{
    int nc;
    BoardPos *np;
    if (need <= g_posCap) return 1;
    nc = g_posCap ? g_posCap * 2 : 64;
    while (nc < need) nc *= 2;
    np = (BoardPos *)realloc(g_pos, (size_t)nc * sizeof(BoardPos));
    if (!np) return 0;
    memset(np + g_posCap, 0, (size_t)(nc - g_posCap) * sizeof(BoardPos));
    g_pos = np;
    g_posCap = nc;
    return 1;
}

static BoardPos *pos_find(int id, int create)
{
    int i;
    if (id <= 0) return NULL;
    for (i = 0; i < g_posCount; i++)
        if (g_pos[i].id == id) return &g_pos[i];
    if (!create) return NULL;
    for (i = 0; i < g_posCount; i++) {
        if (g_pos[i].id == 0) {
            memset(&g_pos[i], 0, sizeof(g_pos[i]));
            g_pos[i].id = id;
            g_pos[i].x = g_pos[i].y = -1;
            g_pos[i].page = g_boardInit ? 1 : (g_stk ? g_stk->page : 1);
            return &g_pos[i];
        }
    }
    if (!pos_ensure(g_posCount + 1)) return NULL;
    {
        BoardPos *bp = &g_pos[g_posCount++];
        memset(bp, 0, sizeof(*bp));
        bp->id = id;
        bp->x = bp->y = -1;
        bp->page = g_boardInit ? 1 : (g_stk ? g_stk->page : 1);
        return bp;
    }
}

static void pos_save(void)
{
    WStr out;
    HANDLE h;
    DWORD wrote = 0;
    unsigned short bom = 0xFEFF;
    int i;
    pos_path_init();
    if (!g_posPath[0]) return;
    ws_init(&out);
    if (g_viewPage > 1) {
        ws_add(&out, L"viewpage=");
        ws_int(&out, g_viewPage);
        ws_add(&out, L"\r\n");
    }
    {
        int pc = g_stk ? g_stk->pageCount : g_pagesSaved;
        if (pc > 1) {
            ws_add(&out, L"pages=");
            ws_int(&out, pc);
            ws_add(&out, L"\r\n");
        }
    }
    for (i = 0; i < g_posCount; i++) {
        if (!g_pos[i].id) continue;
        if (model_count() > 0 && !model_by_id(g_pos[i].id)) {
            memset(&g_pos[i], 0, sizeof(g_pos[i]));    /* \u65e5\u7a0b\u5df2\u5220\uff1a\u987a\u624b\u6e05\u6389\u6b8b\u7559\u8bb0\u5f55 */
            continue;
        }
        if (g_pos[i].x < 0 && g_pos[i].y < 0 && g_pos[i].w <= 0 &&
            g_pos[i].h <= 0 && !g_pos[i].pinned && !g_pos[i].hidden &&
            g_pos[i].page <= 1) continue;
        ws_add(&out, L"id=");   ws_int(&out, g_pos[i].id);
        ws_add(&out, L" x=");   ws_int(&out, g_pos[i].x);
        ws_add(&out, L" y=");   ws_int(&out, g_pos[i].y);
        ws_add(&out, L" w=");   ws_int(&out, g_pos[i].w);
        ws_add(&out, L" h=");   ws_int(&out, g_pos[i].h);
        ws_add(&out, L" pin="); ws_int(&out, g_pos[i].pinned);
        ws_add(&out, L" hid="); ws_int(&out, g_pos[i].hidden);
        if (g_pos[i].page > 1) { ws_add(&out, L" page="); ws_int(&out, g_pos[i].page); }
        ws_add(&out, L"\r\n");
    }
    h = CreateFileW(g_posPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { ws_free(&out); return; }
    WriteFile(h, &bom, 2, &wrote, NULL);
    if (out.p) WriteFile(h, out.p, (DWORD)(out.len * sizeof(wchar_t)), &wrote, NULL);
    CloseHandle(h);
    ws_free(&out);
}

static void pos_load(void)
{
    HANDLE h;
    DWORD size, rd = 0;
    wchar_t *buf, *p;
    if (g_posLoaded) return;
    g_posLoaded = 1;
    pos_path_init();
    if (!g_posPath[0]) return;
    h = CreateFileW(g_posPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(h, NULL);
    if (size < 4 || size > 256 * 1024) { CloseHandle(h); return; }
    buf = (wchar_t *)malloc(size + 4);
    if (!buf) { CloseHandle(h); return; }
    ReadFile(h, buf, size, &rd, NULL);
    CloseHandle(h);
    {
        size_t n = rd / 2;
        if (n && buf[0] == 0xFEFF) { memmove(buf, buf + 1, (n - 1) * sizeof(wchar_t)); n--; }
        buf[n] = 0;
    }
    for (p = buf; *p; ) {
        wchar_t *nl = wcschr(p, L'\n');
        int id = 0, x = -1, y = -1, w = 0, hh = 0, pin = 0, hid = 0, page = 1;
        if (nl) *nl = 0;
        {
            wchar_t *q = p;
            while (*q) {
                if (!wcsncmp(q, L"id=", 3))       id = (int)wcstol(q + 3, NULL, 10);
                else if (!wcsncmp(q, L"x=", 2))   x = (int)wcstol(q + 2, NULL, 10);
                else if (!wcsncmp(q, L"y=", 2))   y = (int)wcstol(q + 2, NULL, 10);
                else if (!wcsncmp(q, L"w=", 2))   w = (int)wcstol(q + 2, NULL, 10);
                else if (!wcsncmp(q, L"h=", 2))   hh = (int)wcstol(q + 2, NULL, 10);
                else if (!wcsncmp(q, L"pin=", 4)) pin = (int)wcstol(q + 4, NULL, 10);
                else if (!wcsncmp(q, L"hid=", 4)) hid = (int)wcstol(q + 4, NULL, 10);
                else if (!wcsncmp(q, L"page=", 5) && wcsncmp(q, L"viewpage=", 9)) page = (int)wcstol(q + 5, NULL, 10);
                else if (!wcsncmp(q, L"viewpage=", 9)) g_viewPage = (int)wcstol(q + 9, NULL, 10);
                else if (!wcsncmp(q, L"pages=", 6))   g_pagesSaved = (int)wcstol(q + 6, NULL, 10);
                while (*q && *q != L' ') q++;
                while (*q == L' ') q++;
            }
        }
        if (id > 0) {
            BoardPos *bp = pos_find(id, 1);
            if (bp) { bp->x = x; bp->y = y; bp->w = w; bp->h = hh;
                      bp->pinned = pin; bp->hidden = hid ? 1 : 0;
                      bp->page = (page > 1) ? page : 1; }
        }
        if (!nl) break;
        p = nl + 1;
    }
    free(buf);
}

/* ======================================================================
 *  字体：按“绝对缩放百分比”缓存，让内容随便笺尺寸等比变化
 * ====================================================================*/
static int st_line(HFONT f)
{
    HDC hdc = GetDC(NULL);
    TEXTMETRICW tm;
    HFONT old = (HFONT)SelectObject(hdc, f);
    tm.tmHeight = 0;
    GetTextMetricsW(hdc, &tm);
    SelectObject(hdc, old);
    ReleaseDC(NULL, hdc);
    return tm.tmHeight > 0 ? tm.tmHeight : 16;
}

static HFONT st_font(int pt, int weight, int scale)
{
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    HFONT f;
    ReleaseDC(NULL, hdc);
    f = CreateFontW(-MulDiv(pt, dpi * scale, 9600), 0, 0, 0, weight, 0, 0, 0,
                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    return f;
}

static void st_fonts_free(void)
{
    int i;
    for (i = 0; i < g_fontCount; i++) {
        DeleteObject(g_fonts[i].hTitle);
        DeleteObject(g_fonts[i].hSmall);
        DeleteObject(g_fonts[i].hTiny);
        DeleteObject(g_fonts[i].hNum);
        DeleteObject(g_fonts[i].hMid);
    }
    g_fontCount = 0;
}

static const StkFonts *st_fonts_for(int absPct)
{
    StkFonts *f;
    int key, i;
    if (absPct < 40) absPct = 40;
    if (absPct > 1600) absPct = 1600;
    key = ((absPct + 2) / 4) * 4;
    for (i = 0; i < g_fontCount; i++)
        if (g_fonts[i].key == key) return &g_fonts[i];
    if (g_fontCount >= FONT_SLOTS) st_fonts_free();
    f = &g_fonts[g_fontCount++];
    f->key = key;
    f->hTitle = st_font(11, FW_SEMIBOLD, key);
    f->hSmall = st_font(9, FW_NORMAL, key);
    f->hTiny  = st_font(8, FW_NORMAL, key);
    f->hNum   = st_font(13, FW_BOLD, key);
    f->hMid   = st_font(11, FW_BOLD, key);
    f->lhTitle = st_line(f->hTitle);
    f->lhSmall = st_line(f->hSmall);
    f->lhTiny  = st_line(f->hTiny);
    f->lhNum   = st_line(f->hNum);
    f->lhMid   = st_line(f->hMid);
    return f;
}

static void st_metrics(Sticky *s)
{
    int scale;
    {
        HDC hdc = GetDC(NULL);
        scale = MulDiv(GetDeviceCaps(hdc, LOGPIXELSY), 100, 96);
        ReleaseDC(NULL, hdc);
    }
    if (scale < 70) scale = 70;
    if (scale == s->scale && s->hTitle) return;
    s->scale = scale;
    if (s->hTitle) DeleteObject(s->hTitle);
    if (s->hSmall) DeleteObject(s->hSmall);
    if (s->hTiny) DeleteObject(s->hTiny);
    if (s->hNum) DeleteObject(s->hNum);
    s->hTitle = st_font(11, FW_SEMIBOLD, scale);
    s->hSmall = st_font(9, FW_NORMAL, scale);
    s->hTiny = st_font(8, FW_NORMAL, scale);
    s->hNum = st_font(13, FW_BOLD, scale);
    s->lhTitle = st_line(s->hTitle);
    s->lhSmall = st_line(s->hSmall);
    s->lhTiny = st_line(s->hTiny);
    s->lhNum = st_line(s->hNum);

    s->noteW = MulDiv(NOTE_W, scale, 100);
    s->noteH = MulDiv(NOTE_H, scale, 100);
    s->gapX = MulDiv(GAP_X, scale, 100);
    s->gapY = MulDiv(GAP_Y, scale, 100);
    s->padX = MulDiv(PAD_X, scale, 100);
    s->padTop = MulDiv(PAD_TOP, scale, 100);
    s->padRight = MulDiv(PAD_RIGHT, scale, 100);
    s->padBottom = MulDiv(PAD_BOTTOM, scale, 100);
    st_fonts_free();
}

static double st_angle(int id)
{
    static const double tab[7] = { -2.4, 1.8, -1.2, 2.6, -1.9, 0.9, -0.6 };
    int k = (id * 2654435761u) >> 26 & 0x3F;
    return tab[k % 7];
}

/* ======================================================================
 *  尺寸与布局
 * ====================================================================*/
static int st_note_w(const Sticky *s, const BoardPos *bp)
{
    if (bp && bp->w > 0) return MulDiv(bp->w, s->scale, 100);
    return s->noteW;
}

static int st_note_h(const Sticky *s, const BoardPos *bp)
{
    if (bp && bp->h > 0) return MulDiv(bp->h, s->scale, 100);
    return s->noteH;
}

/* 内容自适应系数：便笺越小内容越小（百分比，相对默认尺寸） */
static int st_fit(int w, int h, int baseW, int baseH)
{
    int fx = baseW > 0 ? MulDiv(w, 100, baseW) : 100;
    int fy = baseH > 0 ? MulDiv(h, 100, baseH) : 100;
    int f = fx < fy ? fx : fy;
    if (f < 50) f = 50;
    if (f > 800) f = 800;
    return f;
}

static int st_hidden(int id);      /* 后面才定义（st_rect 也要用） */

static void st_rect(Sticky *s, int i, RECT *r)
{
    BoardPos *bp;
    if (i < 0 || i >= s->count) { SetRectEmpty(r); return; }
    if (st_hidden(s->ids[i])) { SetRectEmpty(r); return; }   /* \u6536\u8d77\u7684\u4fbf\u7b3a\u6ca1\u6709\u77e9\u5f62 */
    if (!s->layoutOk) {   /* 布局缓存尚未建立：按行内位置兜底 */
        int col = s->cols > 0 ? i % s->cols : 0;
        int row = s->cols > 0 ? i / s->cols : 0;
        r->left = s->padX + col * (s->noteW + s->gapX);
        r->top = s->padTop + row * (s->noteH + s->gapY);
        r->right = r->left + s->noteW;
        r->bottom = r->top + s->noteH;
    } else {
        *r = s->rects[i];
    }
    if (!g_posLoaded) pos_load();
    bp = pos_find(s->ids[i], 0);
    if (bp && bp->x >= 0 && bp->y >= 0) {
        int w = st_note_w(s, bp), h = st_note_h(s, bp);
        int x = MulDiv(bp->x, s->scale, 100);
        int y = MulDiv(bp->y, s->scale, 100);
        int limit = s->clientW - w - MulDiv(8, s->scale, 100);
        /* 夹在页面里：窗口变窄时也不会跑到看不见的地方（不改用户设置） */
        if (limit < 0) limit = 0;
        if (x > limit) x = limit;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        r->left = x; r->top = y;
        r->right = x + w;
        r->bottom = y + h;
    }
}


/* ======================================================================
 *  翻页：一条便笺属于第几页（BoardPos.page；0/1 都算第 1 页）
 * ====================================================================*/
static int st_item_page(int id)
{
    BoardPos *bp = pos_find(id, 0);
    return (bp && bp->page > 1) ? bp->page : 1;
}

/* rects[] / z[] 的容量（v1.6 起动态增长，不再有张数上限） */
static int st_arr_ensure(Sticky *s, int need)
{
    int nc;
    if (need <= s->arrCap) return 1;
    nc = s->arrCap ? s->arrCap * 2 : 128;
    while (nc < need) nc *= 2;
    {
        RECT *nr = (RECT *)realloc(s->rects, (size_t)nc * sizeof(RECT));
        int *nz;
        if (!nr) return 0;
        s->rects = nr;
        nz = (int *)realloc(s->z, (size_t)nc * sizeof(int));
        if (!nz) return 0;
        s->z = nz;
        s->arrCap = nc;
    }
    return 1;
}

static void st_page_label(Sticky *s, wchar_t *buf, size_t cch)
{
    WBuf b;
    wb_init(&b, buf, cch);
    wb_add(&b, L"第 ");
    wb_int(&b, s->page);
    wb_add(&b, L" / ");
    wb_int(&b, s->pageCount);
    wb_add(&b, L" 页");
}

/* 新开一页按钮的文字：本页满了就直说（“当一页填满后可以选择新开一页”） */
static void st_newpage_label(Sticky *s, wchar_t *buf, size_t cch)
{
    WBuf b;
    wb_init(&b, buf, cch);
    if (s->pageFull) wb_add(&b, L"本页已满 · 新开一页");
    else             wb_add(&b, L"＋ 新一页");
}

/* 页脚三个按钮 + 页码文字的位置（相对客户区） */
static void st_footer_rects(Sticky *s, RECT *prev, RECT *next, RECT *add)
{
    RECT rc;
    int fh = MulDiv(32, s->scale, 100);
    int bw = MulDiv(26, s->scale, 100);
    int gap = MulDiv(6, s->scale, 100);
    int tw = 0, aw = 0, total, x, top, bot;
    SIZE sz;
    wchar_t txt[64];
    HDC hdc;
    GetClientRect(s->hwnd, &rc);
    s->footerH = fh;
    st_page_label(s, txt, 64);
    hdc = GetDC(s->hwnd);
    {
        HFONT old = (HFONT)SelectObject(hdc, s->hSmall);
        sz.cx = sz.cy = 0;
        GetTextExtentPoint32W(hdc, txt, (int)wcslen(txt), &sz);
        tw = sz.cx + MulDiv(12, s->scale, 100);
        st_newpage_label(s, txt, 64);
        GetTextExtentPoint32W(hdc, txt, (int)wcslen(txt), &sz);
        aw = sz.cx + MulDiv(20, s->scale, 100);
        SelectObject(hdc, old);
    }
    ReleaseDC(s->hwnd, hdc);
    total = bw * 2 + tw + aw + gap * 4;
    x = (rc.right - total) / 2;
    if (x < MulDiv(4, s->scale, 100)) x = MulDiv(4, s->scale, 100);
    top = rc.bottom - fh + MulDiv(4, s->scale, 100);
    bot = rc.bottom - MulDiv(3, s->scale, 100);
    prev->left = x; prev->top = top; prev->right = x + bw; prev->bottom = bot;
    x = prev->right + gap + tw + gap;
    next->left = x; next->top = top; next->right = x + bw; next->bottom = bot;
    add->left = next->right + gap * 2; add->top = top;
    add->right = add->left + aw; add->bottom = bot;
}

/* 页脚：‹ 页码 › ＋新一页（本页满时高亮） */
static void st_draw_footer(HDC hdc, Sticky *s)
{
    RECT rc, pv, nx, ad, tr;
    int fh;
    wchar_t txt[64];
    GetClientRect(s->hwnd, &rc);
    st_footer_rects(s, &pv, &nx, &ad);
    fh = s->footerH;
    {
        HBRUSH hb = CreateSolidBrush(TH_BG);
        RECT strip = rc;
        strip.top = rc.bottom - fh;
        FillRect(hdc, &strip, hb);
        DeleteObject(hb);
        {
            HPEN pen = CreatePen(PS_SOLID, 1, TH_LINE_SOFT);
            HPEN op = (HPEN)SelectObject(hdc, pen);
            MoveToEx(hdc, MulDiv(10, s->scale, 100), strip.top, NULL);
            LineTo(hdc, rc.right - MulDiv(10, s->scale, 100), strip.top);
            SelectObject(hdc, op);
            DeleteObject(pen);
        }
    }
    SetBkMode(hdc, TRANSPARENT);
    {
        HFONT old = (HFONT)SelectObject(hdc, s->hSmall);
        SelectObject(hdc, s->hTitle);
        SetTextColor(hdc, s->page > 1 ? TH_TEXT_2 : TH_LINE);
        DrawTextW(hdc, L"\u2039", -1, &pv, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, s->hSmall);
        tr.left = pv.right + MulDiv(6, s->scale, 100);
        tr.right = nx.left - MulDiv(6, s->scale, 100);
        tr.top = pv.top;
        tr.bottom = pv.bottom;
        SetTextColor(hdc, TH_TEXT_3);
        st_page_label(s, txt, 64);
        DrawTextW(hdc, txt, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, s->hTitle);
        SetTextColor(hdc, s->page < s->pageCount ? TH_TEXT_2 : TH_LINE);
        DrawTextW(hdc, L"\u203a", -1, &nx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, s->hSmall);
        st_newpage_label(s, txt, 64);
        if (s->pageFull) {
            HBRUSH hb = CreateSolidBrush(TH_ACCENT);
            FillRect(hdc, &ad, hb);
            DeleteObject(hb);
            SetTextColor(hdc, RGB(255, 255, 255));
        } else {
            HPEN pen = CreatePen(PS_SOLID, 1, TH_ACCENT);
            HPEN op = (HPEN)SelectObject(hdc, pen);
            HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, ad.left, ad.top, ad.right, ad.bottom);
            SelectObject(hdc, ob);
            SelectObject(hdc, op);
            DeleteObject(pen);
            SetTextColor(hdc, TH_ACCENT);
        }
        DrawTextW(hdc, txt, -1, &ad, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, old);
    }
}

/* 流式布局：从左到右摆放，放不下就换行；行高取该行最高的一张；
   已“脱队”自由摆放的便笺不占流式空间。 */
static void st_layout(Sticky *s)
{
    RECT rc;
    int avail, x, y, rowH, i, firstRow = 0;
    if (!g_posLoaded) pos_load();
    GetClientRect(s->hwnd, &rc);
    avail = rc.right - s->padX - s->padRight;
    if (avail < s->noteW) avail = s->noteW;
    x = s->padX;
    y = s->padTop;
    rowH = 0;
    st_arr_ensure(s, s->count > 0 ? s->count : 1);
    for (i = 0; i < s->count; i++) {
        BoardPos *bp = pos_find(s->ids[i], 0);
        int w = st_note_w(s, bp), h = st_note_h(s, bp);
        if (st_item_page(s->ids[i]) != s->page) {   /* 别的页：这一页不画、不占位 */
            SetRectEmpty(&s->rects[i]);
            continue;
        }
        if (bp && bp->hidden) {                 /* 收起来的不占位 */
            SetRectEmpty(&s->rects[i]);
            continue;
        }
        if (bp && bp->x >= 0 && bp->y >= 0) {   /* 已脱队：自由摆放 */
            SetRectEmpty(&s->rects[i]);
            continue;
        }
        if (x > s->padX && x + w > s->padX + avail) {
            x = s->padX;
            y += rowH + s->gapY;
            rowH = 0;
        }
        s->rects[i].left = x;
        s->rects[i].top = y;
        s->rects[i].right = x + w;
        s->rects[i].bottom = y + h;
        x += w + s->gapX;
        if (h > rowH) rowH = h;
        if (y == s->padTop) firstRow++;
    }
    s->cols = firstRow > 0 ? firstRow : 1;
    s->layoutOk = 1;
}

/* 这一页有几张便笺（含收起的） */
static int st_page_notes(Sticky *s)
{
    int i, n = 0;
    for (i = 0; i < s->count; i++)
        if (model_by_id(s->ids[i]) && st_item_page(s->ids[i]) == s->page) n++;
    return n;
}

static void st_recalc(Sticky *s)
{
    int i, maxPage = 1, notesBottom;
    RECT rc, pv, nx, ad;
    if (!g_posLoaded) pos_load();
    if (s->page < 1) s->page = (g_viewPage > 0) ? g_viewPage : 1;   /* 启动时恢复上次看的页 */
    GetClientRect(s->hwnd, &rc);
    s->clientW = rc.right;
    s->clientH = rc.bottom;
    for (i = 0; i < s->count; i++) {
        int p = st_item_page(s->ids[i]);
        if (p > maxPage) maxPage = p;
    }
    if (maxPage < s->page) maxPage = s->page;     /* 当前页永远算“存在”的 */
    if (maxPage < g_pagesSaved) maxPage = g_pagesSaved;
    s->pageCount = maxPage;
    st_footer_rects(s, &pv, &nx, &ad);            /* 顺手定下 footerH */
    st_layout(s);
    notesBottom = s->padTop + s->noteH;
    for (i = 0; i < s->count; i++) {
        RECT r;
        st_rect(s, i, &r);
        if (IsRectEmpty(&r)) continue;
        if (r.bottom > notesBottom) notesBottom = r.bottom;
    }
    s->contentH = notesBottom + s->padBottom + s->footerH;
    s->pageFull = (notesBottom + s->padBottom > s->clientH - s->footerH) ? 1 : 0;
}

static void st_update_scroll(Sticky *s)
{
    RECT rc;
    SCROLLINFO si;
    int page;
    GetClientRect(s->hwnd, &rc);
    page = rc.bottom;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = s->contentH > 0 ? s->contentH - 1 : 0;
    si.nPage = (UINT)(page > 0 ? page : 1);
    si.nPos = s->scrollY;
    SetScrollInfo(s->hwnd, SB_VERT, &si, TRUE);
}

/* ======================================================================
 *  小工具
 * ====================================================================*/
static COLORREF st_lighten(COLORREF c, int pct)
{
    int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
    r = r + (255 - r) * pct / 100;
    g = g + (255 - g) * pct / 100;
    b = b + (255 - b) * pct / 100;
    return RGB(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
}

static COLORREF st_urgency(time_t occ)
{
    time_t d;
    if (occ <= 0) return TH_TEXT_3;
    d = occ - time(NULL);
    if (d <= 3600) return TH_DANGER;
    if (d < 3600 * 6) return TH_WARN;
    return TH_ACCENT;
}

/* 说明：曾用 SetWorldTransform 给便笺加真实倾角，但在同一个内存 DC 上连续绘制
   多张时 GDI 的世界变换状态不稳定（只有最后绘制的能正常渲染），因此改为正交
   绘制，用投影偏移、胶带与折角体现“手贴上去”的感觉。st_angle() 只用于投影偏向。*/

/* ======================================================================
 *  绘制：投影（裁剪排除便笺本体，保证投影绝不覆盖纸面）
 * ====================================================================*/
static void st_shadow(HDC hdc, const RECT *note, int radius, int lift, int depth)
{
    int save = SaveDC(hdc);
    RECT inner = *note;
    RECT sh = *note;
    int i;
    inner.left += 1; inner.top += 1; inner.right -= 1; inner.bottom -= 1;
    ExcludeClipRect(hdc, inner.left, inner.top, inner.right, inner.bottom);
    sh.left += lift + 2; sh.top += lift + 3;
    sh.right += lift + 2; sh.bottom += lift + 3;
    for (i = depth; i >= 1; i--) {
        RECT r = sh;
        int t = 232 - (depth - i) * 6;
        HBRUSH hb;
        HPEN pen;
        HBRUSH ob;
        HPEN op;
        r.left += i - 1; r.top += i - 1;
        r.right += i + 1; r.bottom += i + 1;
        if (t < 202) t = 202;
        hb = CreateSolidBrush(RGB(t, t - 3, t - 9));
        pen = CreatePen(PS_SOLID, 1, RGB(t, t - 3, t - 9));
        ob = (HBRUSH)SelectObject(hdc, hb);
        op = (HPEN)SelectObject(hdc, pen);
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius + i, radius + i);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(pen);
        DeleteObject(hb);
    }
    RestoreDC(hdc, save);
}

/* 缩放手柄的视觉：右下角三条斜线 + 右/下边缘的浅色细条 */
static void st_draw_grips(HDC hdc, const Sticky *s, const RECT *r, int active)
{
    int u = s->scale;
    int pw = MulDiv(1, u, 100);
    COLORREF c = active ? TH_ACCENT : TH_TEXT_3;
    HPEN pen;
    HPEN op;
    int i;
    if (pw < 1) pw = 1;
    pen = CreatePen(PS_SOLID, pw, c);
    op = (HPEN)SelectObject(hdc, pen);
    for (i = 0; i < 3; i++) {
        int d = MulDiv(5 + i * 5, u, 100);
        MoveToEx(hdc, r->right - 3 - d, r->bottom - 3, NULL);
        LineTo(hdc, r->right - 3, r->bottom - 3 - d);
    }
    SelectObject(hdc, op);
    DeleteObject(pen);
    {
        HBRUSH hb = CreateSolidBrush(active ? TH_ACCENT : RGB(196, 186, 168));
        RECT b = *r;
        int th = MulDiv(3, u, 100);
        int len = MulDiv(26, u, 100);
        if (th < 1) th = 1;
        b.left = r->right - len - MulDiv(6, u, 100);
        b.right = r->right - MulDiv(6, u, 100);
        b.top = r->bottom - th;
        FillRect(hdc, &b, hb);
        b.left = r->right - th;
        b.right = r->right;
        b.top = r->bottom - len - MulDiv(6, u, 100);
        b.bottom = r->bottom - MulDiv(6, u, 100);
        FillRect(hdc, &b, hb);
        DeleteObject(hb);
    }
}

/* 倒计时字号档位：便笺越窄字号越小，避免“还剩 3天 12:31:05”被截断 */
enum { CF_NUM = 0, CF_MID = 1, CF_SMALL = 2 };
static int st_cf_pick(int w, int u)
{
    if (w < MulDiv(168, u, 100)) return CF_SMALL;
    if (w < MulDiv(220, u, 100)) return CF_MID;
    return CF_NUM;
}
static HFONT st_cf_font(const StkFonts *ff, int idx)
{
    return idx == CF_SMALL ? ff->hSmall : (idx == CF_MID ? ff->hMid : ff->hNum);
}
static int st_cf_line(const StkFonts *ff, int idx)
{
    return idx == CF_SMALL ? ff->lhSmall : (idx == CF_MID ? ff->lhMid : ff->lhNum);
}

/* 这张便笺是否被收起来了 */
static int st_hidden(int id)
{
    BoardPos *bp = pos_find(id, 0);
    return (bp && bp->hidden) ? 1 : 0;
}

/* 这张便笺要画几张照片 */
static int st_img_count(const Schedule *sc)
{
    int n = sched_image_count(sc);
    if (!sc) return 0;
    if (sc->imgN > 0 && sc->imgN < n) n = sc->imgN;
    if (n > MAX_IMGS) n = MAX_IMGS;
    return n;
}

/* 便笺内容布局：画的时候与鼠标响应（拖动/缩放照片）用同一套数字 */
typedef struct {
    const StkFonts *ff;
    int u;                 /* 绝对缩放百分比 */
    int pad, avail;
    int x, xr;
    int yTitle, yAfterTitle;
    int cf;                /* 倒计时字号档位 */
    RECT content;          /* 内容区（标题下方 → avail），照片在这里自由摆 */
    RECT img[MAX_IMGS];    /* 每张照片各自的框（已按滚动位移） */
    RECT imgBase[MAX_IMGS];/* 未滚动的框（存盘用） */
    int  imgCount;         /* 有几张照片可画 */
    int  imgTop, imgBottom;/* 照片区上下边 */
    int  imgAreaTop, imgAreaBottom;  /* 照片可用的纵向范围（缩放上限） */
    int  imgSized;         /* 1 = 用户手动调过照片大小（imgB 里有宽度千分比） */
    int  textUse;          /* 不滚动时可见的正文高度 */
    int  viewTop;          /* 可滚动视口上边 */
    int  imgLayer;         /* 照片层次：在文字上 / 下 */
    int gapAfterImg;
    int dateTop, dateH;    /* dateTop = 0 表示不画日期/待办行 */
    int boxTop, boxBottom; /* boxTop = 0 表示不画倒计时框 */
    int bodyTop, textH;    /* 正文视口（固定不动） */
    int bodyFont;          /* 0 = 小字，1 = 更小的字（文本自动适配） */
    int scroll, maxScroll; /* 内容（含照片）超长时在视口内滚动 */
    int contentH;          /* 正文内容总高 */
} StkLayout;

/* 每张便笺的正文滚动位置（临时状态，不写盘） */
#define NSCR_MAX 256
static struct { int id, off; } g_nscr[NSCR_MAX];

static int st_nscr_get(int id)
{
    int i;
    for (i = 0; i < NSCR_MAX; i++)
        if (g_nscr[i].id == id) return g_nscr[i].off;
    return 0;
}

static void st_nscr_set(int id, int off)
{
    int i, slot = -1;
    for (i = 0; i < NSCR_MAX; i++) {
        if (g_nscr[i].id == id) { g_nscr[i].off = off; return; }
        if (slot < 0 && g_nscr[i].id == 0) slot = i;
    }
    if (slot >= 0) { g_nscr[slot].id = id; g_nscr[slot].off = off; }
}


/* 正文开头的空行也算正文位置；预留一行可见正文时要把它们算进去 */
static int st_lead_blank_lines(const wchar_t *t)
{
    int n = 0;
    if (!t) return 0;
    while (*t) {
        if (*t == L'\r' || *t == L'\n') { n++; t++; }
        else if (*t == L' ' || *t == L'\t') t++;
        else break;
    }
    return n;
}

static void st_note_layout(Sticky *s, const Schedule *sc, const RECT *r, StkLayout *L)
{
    int w = r->right - r->left;
    int h = r->bottom - r->top;
    int fit = st_fit(w, h, s->noteW, s->noteH);
    int u, avail, y;
    int gap, dateH, boxH, textMin;

    memset(L, 0, sizeof(*L));
    L->ff = st_fonts_for(MulDiv(s->scale, fit, 100));
    u = L->u = L->ff->key;
    L->pad = MulDiv(14, u, 100);
    avail = L->avail = r->bottom - MulDiv(11, u, 100);
    L->x = r->left + L->pad;
    L->xr = r->right - L->pad;
    L->cf = st_cf_pick(w, u);
    y = r->top + MulDiv(19, u, 100);
    L->yTitle = y;

    if (y + L->ff->lhTitle <= avail) {          /* 标题（最多两行） */
        int bottom = y + L->ff->lhTitle * 2;
        if (bottom > avail) bottom = avail;
        y = bottom + MulDiv(3, u, 100);
    }
    L->yAfterTitle = y;
    L->content.left = L->x;
    L->content.right = L->xr;
    L->content.top = y;
    L->content.bottom = avail;
    { int q; for (q = 0; q < MAX_IMGS; q++) SetRectEmpty(&L->img[q]); L->imgCount = 0; L->imgLayer = IMGL_OVER; }

    gap = MulDiv(6, u, 100);
    dateH = L->ff->lhSmall + MulDiv(5, u, 100);
    boxH = (sc->kind != KIND_TODO)
             ? (st_cf_line(L->ff, L->cf) + MulDiv(8, u, 100) + MulDiv(5, u, 100)) : 0;
    textMin = L->ff->lhSmall + MulDiv(4, u, 100);
    L->gapAfterImg = gap;
    L->dateH = dateH;

    /* 1) 先按“没有照片”排：标题 → 日期 → 倒计时 → 正文起点 */
    {
        int room = avail - y;
        if (room >= dateH + textMin) {
            L->dateTop = y;
            y += dateH;
            room -= dateH;
            if (boxH && room >= boxH + textMin) {
                L->boxTop = y;
                L->boxBottom = y + boxH - MulDiv(5, u, 100);
                y += boxH;
            }
        } else if (room >= dateH) {
            L->dateTop = y;             /* 地方不够也先把日期放上 */
            y += dateH;
        }
    }
    L->content.top = (L->dateTop && L->dateTop < y) ? L->dateTop : y;
    {
        int bodyTop0 = y;                       /* 正文（不含照片）的起点 */
        int availBody = avail - bodyTop0;       /* 正文 + 照片可用的总高度 */
        int thFull = 0, thUse = 0;

        /* 量正文自然高度 */
        if (sc->note[0] && availBody > 0) {
            HDC hdc = GetDC(NULL);
            RECT m;
            m.left = 0; m.top = 0; m.right = L->xr - L->x; m.bottom = 0;
            rich_parse(sc->noteRtf, sc->note, rich_doc());
            thFull = rich_draw(hdc, rich_doc(), &m, L->bodyFont ? 16 : 18,
                               L->ff->key, RGB(96, 86, 72), 0);
            ReleaseDC(NULL, hdc);
        }
        /* 图片清单 */
        {
            int n = sc->showImg ? st_img_count(sc) : 0;
            if (n > MAX_IMGS) n = MAX_IMGS;
            L->imgCount = n;
            L->imgLayer = sc->imgLayer;
        }
        /* 没有正文就不给正文留位置（否则照片下面会空出一块“留着却没人用”的空白） */
        thUse = (thFull > 0) ? thFull : 0;
        if (L->imgCount > 0 && thUse > availBody * 60 / 100)
            thUse = availBody * 60 / 100;
        if (thUse > availBody) thUse = availBody;
        if (thUse < 0) thUse = 0;
        L->textUse = thUse;                    /* 不滚动时能看到的正文高度 */
        L->contentH = (thFull > 0) ? thFull : thUse;   /* 滚动范围按正文自然高度 */

        /* 2) 摆照片 */
        if (L->imgCount > 0) {
            int imgH;
            int i;
            int minH = MulDiv(IMG_MIN_H, u, 100);
            int minW = MulDiv(IMG_MIN_W, u, 100);
            int hasText = sc->note[0] ? 1 : 0;
            int lead = hasText ? st_lead_blank_lines(sc->note) : 0;
            int oneLine = textMin + gap;
            int keep = oneLine + lead * L->ff->lhSmall;   /* 一行可见正文 + 开头空行 */
            int reserve = 0;
            int imgTop;
            /* v1.9.1：用户调过照片大小 → 照片区用满正文区；
               v1.9.3：只要有正文就永远留出一行，照片再大也压不掉正文 */
            for (i = 0; i < L->imgCount; i++)
                if (sc->imgB[i][2] >= 60) { L->imgSized = 1; break; }
            if (hasText) {
                reserve = L->imgSized ? keep : (thUse > keep ? thUse : keep);
                if (reserve > availBody) reserve = availBody;
                if (reserve < 0) reserve = 0;
            }
            imgH = availBody - reserve - ((sc->imgMode == IMGM_END) ? gap : 0);
            imgTop = (sc->imgMode == IMGM_END) ? (bodyTop0 + reserve + gap) : bodyTop0;
            /* 照片最小高度只在“加上它仍放得下那一行正文”时生效 */
            if (imgH < minH && (!hasText || availBody - minH >= reserve)) {
                imgH = minH;
                if (sc->imgMode != IMGM_END) {
                    thUse = availBody - imgH;
                    if (thUse < 0) thUse = 0;
                    L->textUse = thUse;
                }
            }
            if (imgH < 0) imgH = 0;
            if (imgTop + imgH > avail) imgH = avail - imgTop;
            if (imgH < 0) imgH = 0;
            {
                int cw = L->xr - L->x;
                int chh = L->content.bottom - L->content.top;
                int slot = (L->imgCount > 0)
                             ? (imgH - (L->imgCount - 1) * MulDiv(4, u, 100)) / L->imgCount
                             : imgH;
                int yAuto = imgTop;
                if (slot < minH) slot = minH;
                if (slot > imgH) slot = imgH;    /* 不越过照片带（正文优先时照片带可能很小） */
                if (slot < 0) slot = 0;
                for (i = 0; i < L->imgCount; i++) {
                    RECT bx;
                    wchar_t ipath[MAX_PATH];
                    int iw = 0, ih = 0, wf = 1000, w, h;
                    const int *b = sc->imgB[i];
                    if (!sched_image_at(sc, i, ipath, MAX_PATH) ||
                        !img_size(ipath, &iw, &ih) || iw <= 0 || ih <= 0) {
                        iw = ih = 1;                  /* 读不到尺寸：退回铺满一块 */
                    }
                    if (b[2] >= 60 && b[2] <= 1000) wf = b[2];
                    w = (int)((long long)cw * wf / 1000);
                    if (w < minW) w = minW;      /* 只钳宽度；高度永远按照片比例来 */
                    if (w > cw) w = cw;
                    h = (int)((long long)w * ih / iw);
                    if (h > slot) {                   /* 放不下就整体缩小：整张照片永远看得全 */
                        h = slot;
                        w = (int)((long long)h * iw / ih);
                        if (w > cw) { w = cw; h = (int)((long long)w * ih / iw); }
                    }
                    {   /* 位置：存过位置的（浮动）按存的来；否则水平居中、自上而下排 */
                        int fx = -1, fy = -1;
                        if (sc->imgMode == IMGM_FLOAT && (b[0] > 0 || b[1] > 0)) {
                            fx = L->x + (int)((long long)cw * b[0] / 1000);
                            fy = L->content.top + (int)((long long)chh * b[1] / 1000);
                        }
                        if (fx < 0) {
                            bx.left = L->x + (cw - w) / 2;
                            bx.top = yAuto;
                            yAuto = bx.top + h + MulDiv(4, u, 100);
                        } else {
                            bx.left = fx;
                            bx.top = fy;
                        }
                    }
                    bx.right = bx.left + w;
                    bx.bottom = bx.top + h;
                    if (bx.left < L->x) bx.left = L->x;
                    if (bx.top < imgTop) bx.top = imgTop;
                    if (bx.right > L->xr) { bx.right = L->xr; bx.left = bx.right - w; }
                    if (bx.bottom > imgTop + imgH) { bx.bottom = imgTop + imgH; bx.top = bx.bottom - h; }
                    /* v1.9.2：框永远贴住照片本身，不再按最小宽度/高度撑开——
                       撑出来的那部分在 contain 绘制下就是左右/上下的无效空白 */
                    L->img[i] = bx;

                }
            }
            L->imgTop = imgTop;
            L->imgAreaTop = imgTop;
            L->imgAreaBottom = imgTop + imgH;
            {
                int low = imgTop, q;
                for (q = 0; q < L->imgCount; q++)
                    if (L->img[q].bottom > low) low = L->img[q].bottom;
                L->imgBottom = low;
            }
        } else {
            L->imgTop = L->imgBottom = 0;
        }

        /* 3) 正文起点：照片在上方时排在照片之后，文末版式则排在照片之前 */
        if (L->imgCount > 0 && sc->imgMode != IMGM_END) {
            L->bodyTop = L->imgBottom + gap;
            L->textH = avail - L->bodyTop;
            if (!L->imgSized && L->textH < thUse) L->textH = thUse;   /* 滚动可达 */
        } else {
            L->bodyTop = bodyTop0;
            L->textH = thUse;
        }
        if (L->textH < 0) L->textH = 0;
        L->bodyFont = (L->textH >= L->ff->lhSmall * 2) ? 0 : 1;
        L->scroll = 0;
        L->maxScroll = 0;
        {   /* 滚动范围：从最上面的元素到最下面（含照片） */
            int vt = availBody > 0 ? (L->dateTop ? L->dateTop : bodyTop0) : bodyTop0;
            int vb = L->bodyTop + L->contentH;      /* 内容真正的底边（可能超出可视区） */
            if (L->imgCount > 0 && L->imgBottom > vb) vb = L->imgBottom;
            {
                int total = vb - vt, vis = avail - vt;
                if (vis < 0) vis = 0;
                if (total > vis) L->maxScroll = total - vis;
            }
            L->viewTop = vt;
            L->scroll = st_nscr_get(sc->id);
            if (L->scroll > L->maxScroll) L->scroll = L->maxScroll;
            if (L->scroll < 0) L->scroll = 0;
            if (L->scroll) {
                int q;
                for (q = 0; q < L->imgCount; q++) OffsetRect(&L->img[q], 0, -L->scroll);
                if (L->dateTop) L->dateTop -= L->scroll;
                if (L->boxTop) { L->boxTop -= L->scroll; L->boxBottom -= L->scroll; }
                L->bodyTop -= L->scroll;
            }
            {
                int q;
                for (q = 0; q < L->imgCount; q++) L->imgBase[q] = L->img[q];
                for (q = 0; q < L->imgCount; q++)
                    if (L->img[q].bottom < vt) SetRectEmpty(&L->img[q]);
            }
        }
    }
}

/* 画便笺上的照片（逐张，各自有自己的框；照片本身无条件绘制） */
static void st_draw_images(HDC hdc, Sticky *s, const Schedule *sc, const StkLayout *L,
                           COLORREF paper, int u)
{
    int i;
    for (i = 0; i < L->imgCount; i++) {
        wchar_t ipath[MAX_PATH];
        RECT ib = L->img[i];
        if (ib.right <= ib.left || ib.bottom <= ib.top) continue;
        if (!sched_image_at(sc, i, ipath, MAX_PATH)) break;
        img_draw_fit(hdc, ipath, ib.left, ib.top, ib.right - ib.left, ib.bottom - ib.top);
        {
            HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(paper, -34));
            HPEN op = (HPEN)SelectObject(hdc, pen);
            HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, ib.left, ib.top, ib.right - 1, ib.bottom - 1);
            SelectObject(hdc, ob);
            SelectObject(hdc, op);
            DeleteObject(pen);
        }
    }
    /* 点选的那张：虚线框 + 四个顶角的缩放手柄
       （v1.9.2：悬停只改光标，不再自动进入“编辑照片”状态；右键/点别处退出） */
    if (s->imgSelIdx >= 0 && s->imgSelIdx < L->imgCount && s->selId == sc->id) {
        RECT ib = L->img[s->imgSelIdx];
        if (ib.right > ib.left && ib.bottom > ib.top) {
            int hs = MulDiv(IMG_HDL, u, 100);
            int hw = MulDiv(1, u, 100);
            int cx[4], cy[4], k;
            HPEN pen;
            HPEN op;
            HBRUSH hb;
            HBRUSH ob;
            if (hs < 6) hs = 6;
            if (hw < 1) hw = 1;
            pen = CreatePen(PS_DASH, hw, TH_ACCENT);
            op = (HPEN)SelectObject(hdc, pen);
            SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, ib.left - 1, ib.top - 1, ib.right, ib.bottom);
            SelectObject(hdc, op);
            DeleteObject(pen);
            cx[0] = ib.left;  cy[0] = ib.top;
            cx[1] = ib.right; cy[1] = ib.top;
            cx[2] = ib.left;  cy[2] = ib.bottom;
            cx[3] = ib.right; cy[3] = ib.bottom;
            hb = CreateSolidBrush(RGB(252, 250, 244));
            pen = CreatePen(PS_SOLID, hw, TH_ACCENT);
            ob = (HBRUSH)SelectObject(hdc, hb);
            op = (HPEN)SelectObject(hdc, pen);
            for (k = 0; k < 4; k++) {
                int l = cx[k] - hs / 2, t2 = cy[k] - hs / 2;
                Rectangle(hdc, l, t2, l + hs, t2 + hs);
            }
            SelectObject(hdc, op);
            SelectObject(hdc, ob);
            DeleteObject(pen);
            DeleteObject(hb);
        }
    }
}

static void st_draw_note(HDC hdc, Sticky *s, const Schedule *sc, const RECT *box,
                         int selected, int hot, int pinned, int dragging,
                         int resizing, int imgActive)
{
    RECT r = *box;
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    StkLayout L;
    const StkFonts *ff;
    int u, avail;
    COLORREF paper;
    COLORREF line;
    int rad, lift;
    int save;
    int x, xr, y;

    if (w < 8 || h < 8) return;
    st_note_layout(s, sc, &r, &L);
    ff = L.ff;
    u = L.u;
    paper = st_lighten(sc->color, 78);
    line = paper_shade(paper, -36);
    rad = MulDiv(7, u, 100);
    lift = dragging ? MulDiv(9, u, 100)
                    : (selected ? MulDiv(6, u, 100) : (hot ? MulDiv(5, u, 100) : MulDiv(3, u, 100)));

    {
        /* 用倾角决定投影偏向，让每张便笺看起来歪得不一样 */
        int skew = (int)(st_angle(sc->id) * MulDiv(2, u, 100));
        RECT sh = r;
        if (skew > 0) { sh.left += skew; sh.right += skew; }
        st_shadow(hdc, &sh, rad, lift, MulDiv(6, u, 100));
    }

    save = SaveDC(hdc);
    IntersectClipRect(hdc, r.left - 2, r.top - 2, r.right + 2, r.bottom + 2);

    {   /* 纸面 */
        HBRUSH hb = CreateSolidBrush(paper);
        RoundRect(hdc, r.left, r.top, r.right - 1, r.bottom - 1, rad, rad);
        DeleteObject(hb);
    }
    {
        int i;
        for (i = 0; i < 3; i++) {
            RECT b = r;
            HBRUSH hb;
            b.left += rad / 2;
            b.right -= rad / 2;
            b.bottom = r.bottom - i;
            b.top = b.bottom - 1;
            hb = CreateSolidBrush(paper_shade(paper, -(2 + i * 2)));
            FillRect(hdc, &b, hb);
            DeleteObject(hb);
        }
    }
    {
        COLORREF bc = selected ? sc->color : line;
        HPEN pen = CreatePen(PS_SOLID, selected ? 2 : 1, bc);
        HPEN op = (HPEN)SelectObject(hdc, pen);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, r.left, r.top, r.right - 1, r.bottom - 1, rad, rad);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
    if (h > MulDiv(46, u, 100)) {
        RECT t = r;                 /* 左上角的胶带 */
        t.left += w / 5;
        t.right = t.left + w * 3 / 5;
        t.top -= MulDiv(3, u, 100);
        t.bottom = t.top + MulDiv(14, u, 100);
        {
            HBRUSH hb = CreateSolidBrush(RGB(247, 245, 238));
            HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(paper, -18));
            HBRUSH ob = (HBRUSH)SelectObject(hdc, hb);
            HPEN op = (HPEN)SelectObject(hdc, pen);
            Rectangle(hdc, t.left, t.top, t.right, t.bottom);
            SelectObject(hdc, op);
            SelectObject(hdc, ob);
            DeleteObject(pen);
            DeleteObject(hb);
        }
    }
    if (h > MulDiv(52, u, 100))
        paper_fold_corner(hdc, &r, paper, MulDiv(18, u, 100));

    /* -------- 内容：y 为游标，空间不足就逐级省略 -------- */
    SetBkMode(hdc, TRANSPARENT);
    avail = L.avail;
    x = L.x;
    xr = L.xr;
    y = L.yTitle;

    if (y + MulDiv(11, u, 100) <= avail) {
        RECT d = { x, y + MulDiv(5, u, 100), 0, 0 };
        HBRUSH hb;
        HBRUSH ob;
        HPEN op;
        d.right = d.left + MulDiv(9, u, 100);
        d.bottom = d.top + MulDiv(9, u, 100);
        hb = CreateSolidBrush(sc->color);
        ob = (HBRUSH)SelectObject(hdc, hb);
        op = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        Ellipse(hdc, d.left, d.top, d.right, d.bottom);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(hb);
    }
    if (y + ff->lhTitle <= avail) {
        RECT t;
        t.left = x + MulDiv(15, u, 100);
        t.right = xr;
        t.top = y;
        t.bottom = y + ff->lhTitle * 2;
        if (t.bottom > avail) t.bottom = avail;
        SelectObject(hdc, ff->hTitle);
        SetTextColor(hdc, RGB(56, 46, 36));
        DrawTextW(hdc, sc->title, -1, &t,
                  DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
        y = t.bottom + MulDiv(3, u, 100);
    }
    /* v1.9.3：滚动内容（照片/日期/倒计时/正文）统一裁在固定视口里，
       往上滚时不会压住标题；滚动条保持固定 */
    {
    int svScroll = SaveDC(hdc);
    {
        int clipTop = L.viewTop ? L.viewTop : L.content.top;
        if (clipTop < L.content.top) clipTop = L.content.top;
        if (clipTop > avail) clipTop = avail;
        IntersectClipRect(hdc, L.x, clipTop, L.xr, avail);
    }
    if (L.imgCount > 0 && L.imgLayer == IMGL_UNDER)   /* 衬于文字下方：先画照片 */
        st_draw_images(hdc, s, sc, &L, paper, u);
    y = L.dateTop;
    if (sc->kind == KIND_TODO) {
        /* 无时间待办：勾选框 + 标记，不画日期与倒计时 */
        if (L.dateTop && y + ff->lhSmall <= avail) {
            int box = MulDiv(13, u, 100);
            int pw = MulDiv(2, u, 100);
            RECT cb, t;
            HPEN pen, op;
            HBRUSH ob;
            if (pw < 1) pw = 1;
            if (box < 8) box = 8;
            cb.left = x;
            cb.top = y + MulDiv(1, u, 100);
            cb.right = cb.left + box;
            cb.bottom = cb.top + box;
            pen = CreatePen(PS_SOLID, pw, sc->done ? paper_shade(sc->color, -20) : sc->color);
            ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            op = (HPEN)SelectObject(hdc, pen);
            RoundRect(hdc, cb.left, cb.top, cb.right, cb.bottom,
                      MulDiv(3, u, 100), MulDiv(3, u, 100));
            if (sc->done) {
                HPEN p2 = CreatePen(PS_SOLID, pw, paper_shade(sc->color, -30));
                HPEN op2 = (HPEN)SelectObject(hdc, p2);
                MoveToEx(hdc, cb.left + box / 5, cb.top + box / 2, NULL);
                LineTo(hdc, cb.left + box * 2 / 5, cb.bottom - box / 4);
                LineTo(hdc, cb.right - box / 5, cb.top + box / 5);
                SelectObject(hdc, op2);
                DeleteObject(p2);
            }
            SelectObject(hdc, op);
            SelectObject(hdc, ob);
            DeleteObject(pen);
            t = cb;
            t.left = cb.right + MulDiv(6, u, 100);
            t.right = xr;
            t.bottom = t.top + ff->lhSmall;
            SelectObject(hdc, ff->hSmall);
            SetTextColor(hdc, sc->done ? RGB(140, 132, 120) : RGB(126, 112, 92));
            DrawTextW(hdc, sc->done ? L"\u5f85\u529e \u00b7 \u5df2\u5b8c\u6210"
                                    : L"\u5f85\u529e\uff08\u65e0\u65f6\u95f4\uff09",
                      -1, &t, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            y = cb.bottom + MulDiv(5, u, 100);
        }
    } else if (L.dateTop && y + ff->lhSmall <= avail) {
        RECT t;
        wchar_t b1[64], b2[96], buf[200];
        WBuf b;
        t.left = x;
        t.right = xr;
        t.top = y;
        t.bottom = y + ff->lhSmall;
        SelectObject(hdc, ff->hSmall);
        SetTextColor(hdc, RGB(126, 112, 92));
        wb_init(&b, buf, 200);
        wb_add(&b, dt_text(&sc->start, b1, 64));
        wb_add(&b, L"  ");
        wb_add(&b, repeat_text(sc, b2, 96));
        {   /* 一行放不下重复说明时就去掉它，宁可只显示日期也不要省略号 */
            SIZE sz;
            int len = (int)wcslen(buf);
            if (GetTextExtentPoint32W(hdc, buf, len, &sz) && sz.cx > (t.right - t.left)) {
                wb_init(&b, buf, 200);
                wb_add(&b, dt_text(&sc->start, b1, 64));
            }
        }
        DrawTextW(hdc, buf, -1, &t,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        y = t.bottom + MulDiv(5, u, 100);
    }
    if (L.boxTop) {
        {
            RECT t;
            wchar_t buf[96], buf2[96], b1[64];
            WBuf b;
            HFONT hf = st_cf_font(ff, L.cf);
            t.left = x;
            t.right = xr;
            t.top = L.boxTop;
            t.bottom = L.boxBottom;
            if (sc->kind == KIND_ANNIV) {
                wb_init(&b, buf, 96);
                wb_add(&b, sched_anniv_text(sc, buf2, 96));
            } else if (sc->done) {
                wb_init(&b, buf, 96);
                wb_add(&b, L"\u5df2\u5b8c\u6210");
            } else if (!sc->enabled) {
                wb_init(&b, buf, 96);
                wb_add(&b, L"\u5df2\u6682\u505c");
            } else if (sc->timetable) {   /* \u8bfe\u8868\uff1a\u8fd9\u8282\u8bfe\u8fd8\u5269\u591a\u4e45\uff1b
                                              \u4e0a\u5b8c\u4e86 / \u4eca\u5929\u6ca1\u8fd9\u8282\u5c31\u56de\u5230\u666e\u901a\u5012\u8ba1\u65f6 */
                time_t cs = 0, ce = 0, nw = time(NULL);
                int unit = cd_unit_of(sc);
                int have = sched_class_window(sc, &cs, &ce);
                if (unit == CDU_SEC && !sc->cdUnit) unit = CDU_MIN;
                wb_init(&b, buf, 96);
                if (have && nw < cs)      { wb_add(&b, L"\u8ddd\u4e0a\u8bfe "); wb_add(&b, cd_text_u(cs, unit, b1, 64)); }
                else if (have && nw < ce) { wb_add(&b, L"\u4e0a\u8bfe\u4e2d "); wb_add(&b, cd_text_u(ce, unit, b1, 64)); }
                else if (sc->nextOcc > 0) { wb_add(&b, L"\u4e0b\u8282\u8bfe "); wb_add(&b, cd_text(sc, sc->nextOcc, b1, 64)); }
                else                      { wb_add(&b, L"\u8bfe\u8868 \u00b7 \u6682\u65e0\u540e\u7eed"); }
            } else if (sc->nextOcc > 0) {
                wb_init(&b, buf, 96);
                wb_add(&b, L"\u8fd8\u5269 ");
                wb_add(&b, cd_text(sc, sc->nextOcc, b1, 64));
            } else {
                wb_init(&b, buf, 96);
                wb_add(&b, L"\u6682\u65e0\u540e\u7eed");
            }
            {
                COLORREF box = paper_shade(paper, -14);
                HBRUSH hb = CreateSolidBrush(box);
                HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(paper, -30));
                HBRUSH ob = (HBRUSH)SelectObject(hdc, hb);
                HPEN op = (HPEN)SelectObject(hdc, pen);
                RoundRect(hdc, t.left, t.top, t.right, t.bottom,
                          MulDiv(7, u, 100), MulDiv(7, u, 100));
                SelectObject(hdc, op);
                SelectObject(hdc, ob);
                DeleteObject(pen);
                DeleteObject(hb);
            }
            {
                RECT r2 = t;
                SelectObject(hdc, hf);
                if (sc->kind == KIND_ANNIV)       SetTextColor(hdc, TH_ACCENT);
                else if (sc->done || !sc->enabled) SetTextColor(hdc, RGB(140, 132, 120));
                else if (sc->nextOcc > 0)          SetTextColor(hdc, st_urgency(sc->nextOcc));
                else                               SetTextColor(hdc, RGB(140, 132, 120));
                r2.left += MulDiv(7, u, 100);
                r2.right -= MulDiv(4, u, 100);
                DrawTextW(hdc, buf, -1, &r2,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
    }
    if (sc->note[0] && w > MulDiv(96, u, 100)) {
        RECT n;
        int need = ff->lhSmall + MulDiv(4, u, 100);   /* 至少一行就画，不轻易放弃 */
        int vBottom = r.bottom - MulDiv(11, u, 100);
        int clipTop = L.viewTop;
        if (sc->attachCount > 0) vBottom -= ff->lhTiny + MulDiv(4, u, 100);
        n.left = x;
        n.right = xr;
        n.top = L.bodyTop;              /* 布局里已经减过一次滚动位移 */
        n.bottom = n.top + (L.contentH > 0 ? L.contentH : 0);
        if (clipTop < L.content.top) clipTop = L.content.top;
        if (clipTop > vBottom) clipTop = vBottom;
        if (vBottom > L.bodyTop && vBottom - L.bodyTop >= need) {
            int sv = SaveDC(hdc);
            IntersectClipRect(hdc, x, clipTop, xr, vBottom);   /* 固定的正文视口，不跟着滚动跑 */
            rich_parse(sc->noteRtf, sc->note, rich_doc());
            rich_draw(hdc, rich_doc(), &n, L.bodyFont ? 16 : 18, u, RGB(96, 86, 72), 1);
            RestoreDC(hdc, sv);
        }
    }
    RestoreDC(hdc, svScroll);   /* 附件行、滚动条是固定元素，不参与滚动内容裁剪 */
    }
    if (sc->attachCount > 0 && L.imgCount == 0 && h > MulDiv(70, u, 100)) {
        RECT a;
        wchar_t buf[32];
        WBuf b;
        int fold = MulDiv(20, u, 100);
        a.right = r.right - MulDiv(11, u, 100);
        a.left = a.right - MulDiv(62, u, 100);
        a.bottom = r.bottom - fold;
        a.top = a.bottom - ff->lhTiny;
        SelectObject(hdc, ff->hTiny);
        SetTextColor(hdc, RGB(120, 108, 92));
        wb_init(&b, buf, 32);
        wb_add(&b, L"\u9644\u4ef6 ");
        wb_int(&b, sc->attachCount);
        DrawTextW(hdc, buf, -1, &a, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
    }
    if (L.imgCount > 0 && L.imgLayer == IMGL_OVER) {  /* 浮于文字上方：文字之后再画照片 */
        int svImg = SaveDC(hdc);
        int clipTop2 = L.viewTop ? L.viewTop : L.content.top;
        if (clipTop2 < L.content.top) clipTop2 = L.content.top;
        if (clipTop2 > avail) clipTop2 = avail;
        IntersectClipRect(hdc, L.x, clipTop2, L.xr, avail);
        st_draw_images(hdc, s, sc, &L, paper, u);
        RestoreDC(hdc, svImg);
    }

    /* ---- 右侧独立滚动条：只要有东西显示不全就出现 ---- */
    if (L.maxScroll > 0) {
        int vTop = L.viewTop;
        int vBot = r.bottom - MulDiv(6, u, 100);
        int trackH = vBot - vTop;
        if (trackH > 8) {
            int th = trackH * trackH / (trackH + L.maxScroll);
            int ty;
            RECT bar;
            HBRUSH hb;
            if (th < MulDiv(18, u, 100)) th = MulDiv(18, u, 100);
            if (th > trackH) th = trackH;
            ty = vTop + MulDiv(trackH - th, L.scroll, L.maxScroll > 0 ? L.maxScroll : 1);
            bar.left = r.right - MulDiv(11, u, 100);
            bar.right = r.right - MulDiv(2, u, 100);
            if (bar.right < bar.left + MulDiv(4, u, 100))
                bar.right = bar.left + MulDiv(4, u, 100);
            bar.top = vTop;
            bar.bottom = vBot;
            hb = CreateSolidBrush(paper_shade(paper, -6));
            FillRect(hdc, &bar, hb);
            DeleteObject(hb);
            bar.top = ty;
            bar.bottom = ty + th;
            bar.left += MulDiv(1, u, 100);
            bar.right -= MulDiv(1, u, 100);
            hb = CreateSolidBrush(paper_shade(paper, -70));
            FillRect(hdc, &bar, hb);
            DeleteObject(hb);
        }
    }
    if (resizing) {   /* 缩放时在便笺上实时显示尺寸 */
        RECT a;
        wchar_t buf[48];
        WBuf b;
        a.left = x;
        a.right = xr;
        a.bottom = r.bottom - MulDiv(10, u, 100);
        a.top = a.bottom - ff->lhTiny;
        SelectObject(hdc, ff->hTiny);
        SetTextColor(hdc, TH_ACCENT);
        wb_init(&b, buf, 48);
        wb_int(&b, w);
        wb_add(&b, L" \u00d7 ");
        wb_int(&b, h);
        DrawTextW(hdc, buf, -1, &a, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    }
    RestoreDC(hdc, save);

    /* 顶部图钉：选中或固定时出现；固定多一道横杠表示“钉住” */
    if (selected || pinned) {
        int cx = (r.left + r.right) / 2;
        int px = cx, py = r.top - MulDiv(2, u, 100);
        int rr = MulDiv(7, u, 100);
        COLORREF c = pinned ? RGB(118, 110, 98) : sc->color;
        HBRUSH hb;
        HPEN pen;
        HBRUSH ob;
        HPEN op;
        if (rr < 3) rr = 3;
        hb = CreateSolidBrush(c);
        pen = CreatePen(PS_SOLID, 1, paper_shade(c, -40));
        ob = (HBRUSH)SelectObject(hdc, hb);
        op = (HPEN)SelectObject(hdc, pen);
        Ellipse(hdc, px - rr, py - rr, px + rr, py + rr);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(pen);
        DeleteObject(hb);
        {
            HBRUSH wbr = CreateSolidBrush(RGB(250, 250, 245));
            HBRUSH ob2 = (HBRUSH)SelectObject(hdc, wbr);
            HPEN op2 = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
            int r2 = rr / 3;
            Ellipse(hdc, px - r2 - 1, py - r2 - 1, px + r2 - 1, py + r2 - 1);
            SelectObject(hdc, op2);
            SelectObject(hdc, ob2);
            DeleteObject(wbr);
        }
        if (pinned) {
            HBRUSH hb2 = CreateSolidBrush(c);
            RECT b;
            b.left = px - rr; b.right = px + rr;
            b.top = py + rr - 1; b.bottom = py + rr + MulDiv(2, u, 100);
            FillRect(hdc, &b, hb2);
            DeleteObject(hb2);
        }
    }
    /* 缩放手柄：选中或悬停时提示；正在摆照片时不画，免得与照片手柄混淆 */
    if ((selected || hot || resizing) && !pinned && !imgActive)
        st_draw_grips(hdc, s, &r, resizing || selected);
}

static void st_draw_page(HDC hdc, Sticky *s, const RECT *rc, int scrollY)
{
    RECT g = *rc;
    paper_fill(hdc, &g, TH_BG, 0);
    {
        int step = MulDiv(34, s->scale, 100);
        int y;
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(214, 214, 198));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        for (y = s->padTop - (scrollY % step); y < rc->bottom; y += step) {
            if (y < 0) continue;
            MoveToEx(hdc, MulDiv(6, s->scale, 100), y, NULL);
            LineTo(hdc, rc->right - 8, y);
        }
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
    {
        int x = s->padX - MulDiv(14, s->scale, 100);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(214, 150, 140));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, x, 0, NULL);
        LineTo(hdc, x, rc->bottom);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
}

/* 绘制顺序：普通 → 选中 → 拖动中（后者盖在前者上） */
static int st_on_page(Sticky *s, int id)
{
    return st_item_page(id) == s->page;
}

static void st_build_z(Sticky *s)
{
    int i, n = 0;
    st_arr_ensure(s, s->count > 0 ? s->count : 1);
    for (i = 0; i < s->count; i++)
        if (st_hidden(s->ids[i]) || !st_on_page(s, s->ids[i])) continue;
        else if (s->ids[i] != s->dragId && s->ids[i] != s->selId) s->z[n++] = i;
    for (i = 0; i < s->count; i++)
        if (!st_hidden(s->ids[i]) && st_on_page(s, s->ids[i]) && s->ids[i] == s->selId) s->z[n++] = i;
    for (i = 0; i < s->count; i++)
        if (!st_hidden(s->ids[i]) && st_on_page(s, s->ids[i]) && s->ids[i] == s->dragId) s->z[n++] = i;
    s->zCount = n;      /* 只算“这一页真正要画的”张数 */
}

static void st_paint(Sticky *s)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(s->hwnd, &ps);
    RECT rc;
    HDC mem;
    HBITMAP bmp, old;
    int i;

    GetClientRect(s->hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) { EndPaint(s->hwnd, &ps); return; }
    if (!s->layoutOk) st_recalc(s);
    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    old = (HBITMAP)SelectObject(mem, bmp);

    st_draw_page(mem, s, &rc, s->scrollY);
    st_build_z(s);

    for (i = 0; i < s->zCount; i++) {   /* \u53ea\u80fd\u8d70 z[] \u7684\u6709\u6548\u9879\uff1a\u4ee5\u524d\u6309 count \u8d70\uff0c
                                                          \u6536\u8d77\u7684\u4fbf\u7b3a\u4f1a\u88ab\u4e0a\u4e00\u8f6e\u7684\u6b8b\u7559\u4e0b\u6807\u91cd\u65b0\u753b\u51fa\u6765 */
        int idx = s->z[i];
        Schedule *sc;
        BoardPos *bp;
        RECT r;
        int dx = 0, dy = 0;
        if (idx < 0 || idx >= s->count) continue;
        sc = model_by_id(s->ids[idx]);
        if (!sc || st_hidden(sc->id)) continue;
        st_rect(s, idx, &r);
        if (IsRectEmpty(&r)) continue;
        r.top -= s->scrollY;
        r.bottom -= s->scrollY;
        if (s->dragId == sc->id && s->dragging) {
            int w = r.right - r.left, maxL;
            dx = s->dragDelta.x;
            dy = s->dragDelta.y;
            maxL = s->clientW - w - MulDiv(8, s->scale, 100);
            if (maxL < 0) maxL = 0;
            if (r.left + dx > maxL) dx = maxL - r.left;
            if (r.left + dx < 0)    dx = -r.left;
            if (r.top + dy < 0)     dy = -r.top;
        }
        if (r.bottom + dy < -s->noteH || r.top + dy > rc.bottom + s->noteH) continue;
        OffsetRect(&r, dx, dy);
        bp = pos_find(sc->id, 0);
        st_draw_note(mem, s, sc, &r,
                     sc->id == s->selId, sc->id == s->hotId,
                     bp ? bp->pinned : 0,
                     (s->dragId == sc->id && s->dragging),
                     (s->resizing && s->resizeId == sc->id),
                         (s->imgHot && sc->id == s->hotId) || s->imgMode == 2);
    }

    {
        int total = st_page_notes(s), hidden = 0;
        for (i = 0; i < s->count; i++)
            if (st_on_page(s, s->ids[i]) && st_hidden(s->ids[i])) hidden++;
        if (total == 0 || hidden >= total) {
            RECT t = rc;
            SelectObject(mem, s->hTitle);
            SetBkMode(mem, TRANSPARENT);
            SetTextColor(mem, TH_TEXT_3);
            t.top = rc.bottom / 3;
            t.bottom = t.top + MulDiv(30, s->scale, 100);
            DrawTextW(mem, total == 0
                         ? L"这一页还没有便笺 —— 点「＋ 新建日程」写一张，或用下面的 ‹ › 翻页"
                         : L"这一页的便笺都收起来了 —— 视图菜单「显示所有便笺」可以摆回来",
                      -1, &t, DT_CENTER | DT_SINGLELINE);
        }
    }
    st_draw_footer(mem, s);

    if (s->scrollY + rc.bottom < s->contentH - 4) {
        int i2;
        for (i2 = 0; i2 < 10; i2++) {
            RECT b = rc;
            HBRUSH hb;
            b.top = rc.bottom - 10 + i2;
            b.bottom = b.top + 1;
            hb = CreateSolidBrush(paper_shade(TH_BG, -(16 - i2 * 2)));
            FillRect(mem, &b, hb);
            DeleteObject(hb);
        }
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(s->hwnd, &ps);
}

/* ======================================================================
 *  命中：便笺（正交矩形）与缩放手柄
 * ====================================================================*/
static int st_hit_ex(Sticky *s, int x, int y, int *pIndex, int skipDrag)
{
    int i;
    if (pIndex) *pIndex = -1;
    st_build_z(s);                       /* 绘制顺序 = 命中顺序（只含当前页） */
    for (i = s->zCount - 1; i >= 0; i--) {
        int k = s->z[i];
        RECT r;
        if (skipDrag && s->ids[k] == s->dragId) continue;
        st_rect(s, k, &r);
        if (IsRectEmpty(&r)) continue;
        r.top -= s->scrollY;
        r.bottom -= s->scrollY;
        if (x < r.left - 2 || x > r.right + 2) continue;
        if (y < r.top - 2 || y > r.bottom + 2) continue;
        if (pIndex) *pIndex = k;
        return s->ids[k];
    }
    return 0;
}

static int st_hit(Sticky *s, int x, int y, int *pIndex)
{
    return st_hit_ex(s, x, y, pIndex, 1);
}

/* 手柄命中：返回 GD_R / GD_B 的组合，0 表示不是手柄 */
static int st_grip_at(const Sticky *s, const RECT *r, int x, int y)
{
    int g = MulDiv(GRIP, s->scale, 100);
    int e = MulDiv(EDGE, s->scale, 100);
    int gd = 0;
    if (g < 12) g = 12;
    if (e < 6) e = 6;
    if (x >= r->right - g && y >= r->bottom - g) return GD_R | GD_B;
    if (x >= r->right - e && x <= r->right + 2 && y >= r->bottom - g * 2) gd |= GD_R;
    if (y >= r->bottom - e && y <= r->bottom + 2 && x >= r->right - g * 2) gd |= GD_B;
    return gd;
}

static void st_scroll_by(Sticky *s, int dy)
{
    RECT rc;
    int maxY;
    GetClientRect(s->hwnd, &rc);
    maxY = s->contentH - rc.bottom;
    if (maxY < 0) maxY = 0;
    s->scrollY += dy;
    if (s->scrollY > maxY) s->scrollY = maxY;
    if (s->scrollY < 0) s->scrollY = 0;
    SetScrollPos(s->hwnd, SB_VERT, s->scrollY, TRUE);
    InvalidateRect(s->hwnd, NULL, TRUE);
}

static void st_ensure_visible(Sticky *s, int id)
{
    int i;
    RECT rc, r;
    for (i = 0; i < s->count; i++) {
        if (s->ids[i] != id) continue;
        GetClientRect(s->hwnd, &rc);
        st_rect(s, i, &r);
        if (IsRectEmpty(&r)) return;
        if (r.top - s->scrollY < 0) {
            s->scrollY = r.top - s->padTop;
            if (s->scrollY < 0) s->scrollY = 0;
        } else if (r.bottom - s->scrollY > rc.bottom) {
            s->scrollY = r.bottom - rc.bottom + s->padTop;
        }
        st_update_scroll(s);
        InvalidateRect(s->hwnd, NULL, TRUE);
        return;
    }
}

/* 便笺尺寸的说明文字（逻辑像素） */
static void st_size_text(int w, int h, wchar_t *buf, size_t cch)
{
    WBuf b;
    if (w <= 0) w = NOTE_W;
    if (h <= 0) h = NOTE_H;
    wb_init(&b, buf, cch);
    wb_add(&b, L"\u4fbf\u7b3a\u5c3a\u5bf8 ");
    wb_int(&b, w);
    wb_add(&b, L" \u00d7 ");
    wb_int(&b, h);
    wb_add(&b, L"\uff08\u5185\u5bb9\u81ea\u9002\u5e94\uff09");
}

static int st_layout_of(Sticky *s, int idx, StkLayout *L, RECT *pRect);

/* 滚轮：光标在某张便笺上、且该便笺正文超长时，滚动它的正文 */
static int st_note_wheel(Sticky *s, int x, int y, int dir)
{
    int idx = -1;
    int id = st_hit(s, x, y, &idx);
    StkLayout L;
    Schedule *sc;
    int off, step;
    if (!id || idx < 0) return 0;
    sc = model_by_id(id);
    if (!sc) return 0;
    if (!st_layout_of(s, idx, &L, NULL)) return 0;
    if (L.maxScroll <= 0) return 0;
    step = L.ff->lhSmall * 2;
    if (step < 8) step = 8;
    off = L.scroll + (dir > 0 ? -step : step);
    if (off > L.maxScroll) off = L.maxScroll;
    if (off < 0) off = 0;
    if (off != L.scroll) {
        st_nscr_set(id, off);
        InvalidateRect(s->hwnd, NULL, TRUE);
        {
            wchar_t buf[80];
            WBuf b;
            wb_init(&b, buf, 80);
            wb_add(&b, L"\u4fbf\u7b3a\u5185\u5bb9\u8d85\u957f\uff0c\u6eda\u8f6e\u7ee7\u7eed\u770b ");
            wb_int(&b, off);
            wb_add(&b, L" / ");
            wb_int(&b, L.maxScroll);
            main_set_status(buf);
        }
    }
    return 1;
}

/* ---- 便笺上照片的命中：移动、八向缩放 ---- */
static int st_index_of(Sticky *s, int id)
{
    int i;
    for (i = 0; i < s->count; i++)
        if (s->ids[i] == id) return i;
    return -1;
}

/* 取某张便笺的布局（已换算成子窗口坐标） */
static int st_layout_of(Sticky *s, int idx, StkLayout *L, RECT *pRect)
{
    RECT r;
    Schedule *sc;
    if (idx < 0 || idx >= s->count) return 0;
    sc = model_by_id(s->ids[idx]);
    if (!sc) return 0;
    st_rect(s, idx, &r);
    if (IsRectEmpty(&r)) return 0;
    r.top -= s->scrollY;
    r.bottom -= s->scrollY;
    st_note_layout(s, sc, &r, L);
    if (pRect) *pRect = r;
    return 1;
}

/* 把图片框夹回合法位置：只挪位置、不改尺寸（尺寸由照片比例决定） */
static void st_img_clamp(const StkLayout *L, int minTextH, RECT *box)
{
    int w = box->right - box->left;
    int h = box->bottom - box->top;
    int bottomLimit = L->content.bottom - minTextH;
    if (w > L->content.right - L->content.left) w = L->content.right - L->content.left;
    if (h > bottomLimit - L->content.top) h = bottomLimit - L->content.top;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (box->left < L->content.left) box->left = L->content.left;
    if (box->left + w > L->content.right) box->left = L->content.right - w;
    if (box->top < L->content.top) box->top = L->content.top;
    if (box->top + h > bottomLimit) box->top = bottomLimit - h;
    box->right = box->left + w;
    box->bottom = box->top + h;
}

/* 图片框下方必须保留的正文高度（与布局一致）；只要这张日程有正文就一直保留 */
static int st_img_min_text(const StkLayout *L, const Schedule *sc)
{
    if (!sc || !sc->note[0]) return 0;
    return L->ff->lhSmall + MulDiv(4, L->u, 100) +
           st_lead_blank_lines(sc->note) * L->ff->lhSmall;
}

/* 照片四角热区：命中返回 IMG_HDL_L|T 之类的组合（只认四个顶角），否则 0 */
static int st_img_corner(const RECT *b, int x, int y, int hs)
{
    int hdl = 0, cx, cy;
    if (b->right - b->left < 6 || b->bottom - b->top < 6) return 0;
    if (x >= b->left - hs && x <= b->left + hs)        hdl |= IMG_HDL_L;
    else if (x >= b->right - hs && x <= b->right + hs) hdl |= IMG_HDL_R;
    if (y >= b->top - hs && y <= b->top + hs)          hdl |= IMG_HDL_T;
    else if (y >= b->bottom - hs && y <= b->bottom + hs) hdl |= IMG_HDL_B;
    if ((hdl & (IMG_HDL_L | IMG_HDL_R)) == 0) return 0;
    if ((hdl & (IMG_HDL_T | IMG_HDL_B)) == 0) return 0;
    cx = (hdl & IMG_HDL_L) ? b->left : b->right;
    cy = (hdl & IMG_HDL_T) ? b->top : b->bottom;
    if (abs(x - cx) > hs || abs(y - cy) > hs) return 0;
    return hdl;
}

/* 命中：返回 1 = 在照片上，*phdl 给出手柄位（0 = 图片本体，只能移动） */
static int st_img_hit(Sticky *s, int x, int y, int *pIdx, int *phdl, RECT *pContent,
                      int *pImgIdx)
{
    int idx = -1;
    int id = st_hit(s, x, y, &idx);
    StkLayout L;
    int i, hs;
    if (pImgIdx) *pImgIdx = -1;
    if (!id || idx < 0) return 0;
    if (!st_layout_of(s, idx, &L, NULL)) return 0;
    if (L.imgCount <= 0) return 0;
    hs = MulDiv(IMG_HDL + 4, L.u, 100);
    if (hs < 10) hs = 10;
    for (i = L.imgCount - 1; i >= 0; i--) {    /* 后画的在上面，优先命中 */
        RECT ib = L.img[i];
        int hdl;
        if (ib.right <= ib.left || ib.bottom <= ib.top) continue;
        hdl = st_img_corner(&ib, x, y, hs);
        if (hdl || (x >= ib.left && x <= ib.right && y >= ib.top && y <= ib.bottom)) {
            if (pIdx) *pIdx = idx;
            if (phdl) *phdl = hdl;
            if (pContent) *pContent = L.content;
            if (pImgIdx) *pImgIdx = i;
            s->imgHotIdx = i;
            return 1;
        }
    }
    return 0;
}

/* v1.9.1：滚轮不再缩放照片（照片用四个角拖），这里只负责正文滚动 */

/* ======================================================================
 *  过程
 * ====================================================================*/
static LRESULT CALLBACK stk_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Sticky *s = g_stk;

    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)g_stk);
        return 0;

    case WM_SIZE:
        if (s) { s->layoutOk = 0; st_recalc(s); st_update_scroll(s); InvalidateRect(hwnd, NULL, TRUE); }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        if (s) st_paint(s);
        else { PAINTSTRUCT ps; BeginPaint(hwnd, &ps); EndPaint(hwnd, &ps); }
        return 0;

    case WM_VSCROLL:
        if (s) {
            SCROLLINFO si;
            int delta = 0;
            memset(&si, 0, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            switch (LOWORD(wp)) {
            case SB_LINEUP:   delta = -s->gapY; break;
            case SB_LINEDOWN: delta = s->gapY; break;
            case SB_PAGEUP:   delta = -(int)si.nPage; break;
            case SB_PAGEDOWN: delta = (int)si.nPage; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: delta = si.nTrackPos - si.nPos; break;
            default: break;
            }
            st_scroll_by(s, delta);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (s) {
            POINT p;
            int dir = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            ScreenToClient(hwnd, &p);
            if (!st_note_wheel(s, p.x, p.y, dir))
                st_scroll_by(s, dir > 0 ? -s->noteH / 2 : s->noteH / 2);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (s) {
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            if (s->imgMode) {              /* 拖照片：1 = 移动位置，2 = 四角两维同比例缩放 */
                StkLayout L;
                Schedule *sc = model_by_id(s->imgId);
                RECT box;
                int idx = st_index_of(s, s->imgId);
                if (!sc || idx < 0 || !st_layout_of(s, idx, &L, NULL)) { s->imgMode = 0; return 0; }
                box = s->imgBox0;
                if (s->imgMode == 1) {
                    int dx = x - s->imgPt0.x;
                    int dy = y - s->imgPt0.y;
                    OffsetRect(&box, dx, dy);
                    st_img_clamp(&L, st_img_min_text(&L, sc), &box);
                } else {
                    /* 四个顶角：固定的对角不动，新宽高按原比例同时变（照片不变形、整张可见） */
                    int bw = box.right - box.left, bh = box.bottom - box.top;
                    int fx = (s->imgHdl & IMG_HDL_L) ? box.right : box.left;
                    int fy = (s->imgHdl & IMG_HDL_T) ? box.bottom : box.top;
                    int dx = abs(x - fx), dy = abs(y - fy);
                    int nw, nh, minW, minH, maxW, maxH;
                    if (bw < 1) bw = 1;
                    if (bh < 1) bh = 1;
                    if ((long long)dx * bh >= (long long)dy * bw) {
                        nw = dx;
                        nh = (int)((long long)nw * bh / bw);
                    } else {
                        nh = dy;
                        nw = (int)((long long)nh * bw / bh);
                    }
                    /* 最小尺寸只防“缩成一条线”，不能用卡片字体缩放那一套（会把宽照片硬撑大） */
                    minW = MulDiv(48, s->scale, 100);
                    minH = MulDiv(36, s->scale, 100);
                    if (minW < 24) minW = 24;
                    if (minH < 18) minH = 18;
                    if (nw < minW) { nw = minW; nh = nw * bh / bw; }
                    if (nh < minH) { nh = minH; nw = nh * bw / bh; }
                    maxW = (s->imgHdl & IMG_HDL_L) ? (fx - s->imgContent0.left)
                                                   : (s->imgContent0.right - fx);
                    maxH = (s->imgHdl & IMG_HDL_T) ? (fy - s->imgArea0.top)
                                                   : (s->imgArea0.bottom - fy);
                    if (maxW < minW) maxW = minW;
                    if (maxH < minH) maxH = minH;
                    if (nw > maxW) { nw = maxW; nh = nw * bh / bw; }
                    if (nh > maxH) { nh = maxH; nw = nh * bw / bh; }
                    if (nw < minW) nw = minW;
                    if (nh < minH) nh = minH;
                    box.left = (s->imgHdl & IMG_HDL_L) ? fx - nw : fx;
                    box.right = box.left + nw;
                    box.top = (s->imgHdl & IMG_HDL_T) ? fy - nh : fy;
                    box.bottom = box.top + nh;
                }
                {
                    img_box_store(&s->imgContent0, &box, sc);
                    {
                        int kk = (s->imgSelIdx >= 0 && s->imgSelIdx < MAX_IMGS) ? s->imgSelIdx : 0;
                        sc->imgB[kk][0] = sc->imgX; sc->imgB[kk][1] = sc->imgY;
                        sc->imgB[kk][2] = sc->imgW; sc->imgB[kk][3] = sc->imgH;
                    }
                }
                InvalidateRect(hwnd, NULL, TRUE);
                {
                    wchar_t buf[96];
                    WBuf b;
                    wb_init(&b, buf, 96);
                    wb_add(&b, s->imgMode == 1 ? L"\u7167\u7247\u4f4d\u7f6e "
                                               : L"\u7167\u7247\u5c3a\u5bf8 ");
                    wb_int(&b, box.right - box.left);
                    wb_add(&b, L" \u00d7 ");
                    wb_int(&b, box.bottom - box.top);
                    wb_add(&b, L"  \u4f4d\u7f6e ");
                    wb_int(&b, box.left - s->imgContent0.left);
                    wb_add(&b, L",");
                    wb_int(&b, box.top - s->imgContent0.top);
                    main_set_status(buf);
                }
                return 0;
            }
#ifdef ZS_DEBUG_STK
            if (s->dbgCount < 40) {
                int hi = st_hit(s, x, y, NULL);
                s->dbgCount++;
                dbg("MOVE %d pt=%d,%d hit=%d scroll=%d", s->dbgCount, x, y, hi, s->scrollY);
            }
#endif
            if (s->resizing) {
                int dx = x - s->rszPt0.x, dy = y - s->rszPt0.y;
                int minW = MulDiv(NOTE_MIN_W, s->scale, 100);
                int minH = MulDiv(NOTE_MIN_H, s->scale, 100);
                int snap = MulDiv(SNAP, s->scale, 100);
                int w = s->rszW0, h = s->rszH0;
                BoardPos *bp = pos_find(s->resizeId, 1);
                if (!bp) { s->resizing = 0; return 0; }
                if (s->resizeGrip & GD_R) w = s->rszW0 + dx;
                if (s->resizeGrip & GD_B) h = s->rszH0 + dy;
                if (snap < 1) snap = 1;
                w = ((w + snap / 2) / snap) * snap;
                h = ((h + snap / 2) / snap) * snap;
                if (w < minW) w = minW;
                if (h < minH) h = minH;
                bp->w = MulDiv(w, 100, s->scale);
                bp->h = MulDiv(h, 100, s->scale);
                s->layoutOk = 0;
                st_recalc(s);
                st_update_scroll(s);
                InvalidateRect(hwnd, NULL, TRUE);
                dbg("RESIZE id=%d grip=%d w=%d h=%d", s->resizeId, s->resizeGrip,
                    (int)bp->w, (int)bp->h);
                return 0;
            }
            if (s->pressId && !s->dragging) {
                int th = MulDiv(DRAG_THRESHOLD, s->scale, 100);
                if (abs(x - s->pressPt.x) > th || abs(y - s->pressPt.y) > th) {
                    BoardPos *bp = pos_find(s->pressId, 1);
                    if (bp && !bp->pinned) {
                        s->dragging = 1;
                        s->dragId = s->pressId;
                        SetCapture(hwnd);
                        SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                        s->dragDelta.x = x - s->pressPt.x;
                        s->dragDelta.y = y - s->pressPt.y;
                        s->layoutOk = 0;
                        dbg("DRAGSTART id=%d", s->dragId);
                    } else {
                        s->pressId = 0;   /* 已固定：不允许拖动 */
                        main_set_status(L"\u8fd9\u5f20\u4fbf\u7b3a\u5df2\u56fa\u5b9a\uff08\u53f3\u952e\u53ef\u53d6\u6d88\uff09");
                    }
                }
            }
            if (s->dragging) {
                RECT rc;
                int margin, step = 0;
                GetClientRect(hwnd, &rc);
                margin = MulDiv(26, s->scale, 100);
                if (y < margin) step = -MulDiv(22, s->scale, 100);
                else if (y > rc.bottom - margin) step = MulDiv(22, s->scale, 100);
                if (step) {
                    int before = s->scrollY;
                    st_scroll_by(s, step);
                    s->pressPt.y += before - s->scrollY;
                }
                s->dragDelta.x = x - s->pressPt.x;
                s->dragDelta.y = y - s->pressPt.y;
                InvalidateRect(hwnd, NULL, FALSE);
                SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
            } else if (!s->pressId) {
                int idx = -1, id = st_hit(s, x, y, &idx);
                int grip = 0;
                if (id) {
                    BoardPos *bp = pos_find(id, 0);
                    RECT r;
                    st_rect(s, idx, &r);
                    r.top -= s->scrollY;
                    r.bottom -= s->scrollY;
                    if (!bp || !bp->pinned) grip = st_grip_at(s, &r, x, y);
                }
                {
                    int iidx = -1, ihdl = 0;
                    int pin = 0;
                    if (id) {
                        BoardPos *bp2 = pos_find(id, 0);
                        pin = bp2 && bp2->pinned;
                    }
                    {
                        int onImg = (!pin && st_img_hit(s, x, y, &iidx, &ihdl, NULL, NULL)) ? 1 : 0;
                        if (onImg != s->imgHot) {
                            s->imgHot = onImg;
                            InvalidateRect(hwnd, NULL, FALSE);
                        }
                        if (onImg && iidx == idx && !grip) {
                            if (ihdl == (IMG_HDL_L | IMG_HDL_T) || ihdl == (IMG_HDL_R | IMG_HDL_B))
                                SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
                            else if (ihdl == (IMG_HDL_R | IMG_HDL_T) || ihdl == (IMG_HDL_L | IMG_HDL_B))
                                SetCursor(LoadCursorW(NULL, IDC_SIZENESW));
                            else if (ihdl & (IMG_HDL_L | IMG_HDL_R))
                                SetCursor(LoadCursorW(NULL, IDC_SIZEWE));
                            else if (ihdl & (IMG_HDL_T | IMG_HDL_B))
                                SetCursor(LoadCursorW(NULL, IDC_SIZENS));
                            else
                                SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                            if (id != s->hotId) { s->hotId = id; InvalidateRect(hwnd, NULL, FALSE); }
                            if (!s->hotTracking) {
                                TRACKMOUSEEVENT tme;
                                memset(&tme, 0, sizeof(tme));
                                tme.cbSize = sizeof(tme);
                                tme.dwFlags = TME_LEAVE;
                                tme.hwndTrack = hwnd;
                                TrackMouseEvent(&tme);
                                s->hotTracking = 1;
                            }
                            return 0;
                        }
                    }
                }
                if (id != s->hotId || grip) {
                    s->hotId = id;
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                if (grip == (GD_R | GD_B)) SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
                else if (grip & GD_R)      SetCursor(LoadCursorW(NULL, IDC_SIZEWE));
                else if (grip & GD_B)      SetCursor(LoadCursorW(NULL, IDC_SIZENS));
                else if (id)               SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                else                       SetCursor(LoadCursorW(NULL, IDC_ARROW));
                if (!s->hotTracking) {
                    TRACKMOUSEEVENT tme;
                    memset(&tme, 0, sizeof(tme));
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                    s->hotTracking = 1;
                }
            }
        }
        return 0;

    case WM_MOUSELEAVE:
        if (s) {
            s->hotTracking = 0;
            if (s->hotId) { s->hotId = 0; InvalidateRect(hwnd, NULL, FALSE); }
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (s) {
            int idx = -1;
            int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            int id;
            /* 页脚按钮：先于便笺判定 */
            {
                RECT rc, pv, nx, ad;
                POINT pt;
                GetClientRect(hwnd, &rc);
                pt.x = x; pt.y = y;
                if (y >= rc.bottom - s->footerH) {
                    st_footer_rects(s, &pv, &nx, &ad);
                    if (PtInRect(&pv, pt)) {
                        stickies_page_set(hwnd, s->page - 1);
                        SetFocus(hwnd);
                        return 0;
                    }
                    if (PtInRect(&nx, pt)) {
                        stickies_page_set(hwnd, s->page + 1);
                        SetFocus(hwnd);
                        return 0;
                    }
                    if (PtInRect(&ad, pt)) {
                        stickies_page_new(hwnd);
                        SetFocus(hwnd);
                        return 0;
                    }
                }
            }
            id = st_hit_ex(s, x, y, &idx, 1);
            BoardPos *bp = id ? pos_find(id, 0) : NULL;
            int grip = 0;
            dbg("DOWN pt=%d,%d hit=%d idx=%d scroll=%d clientW=%d",
                x, y, id, idx, s->scrollY, s->clientW);
            SetFocus(hwnd);
            s->selId = id;
            s->pressId = 0;
            s->dragging = 0;
            s->dragDelta.x = s->dragDelta.y = 0;
            {
                int iidx = -1, ihdl = 0, iimg = -1;
                RECT icontent;
                int pin = (bp && bp->pinned) ? 1 : 0;
                if (!pin && st_img_hit(s, x, y, &iidx, &ihdl, &icontent, &iimg)) {
                    StkLayout iL;
                    if (st_layout_of(s, iidx, &iL, NULL)) {
                        Schedule *sc0 = model_by_id(s->ids[iidx]);
                        if (sc0) sc0->imgMode = IMGM_FLOAT;      /* 一拖就转为浮动，自定义框生效 */
                        s->imgMode = (ihdl == 0) ? 1 : 2;
                        s->imgId = s->ids[iidx];
                        s->imgSelIdx = (iimg >= 0 && iimg < MAX_IMGS) ? iimg : 0;
                        s->imgHdl = ihdl;
                        s->imgPt0.x = x;
                        s->imgPt0.y = y;
                        s->imgBox0 = (iimg >= 0 && iL.imgBase[iimg].right > iL.imgBase[iimg].left)
                                     ? iL.imgBase[iimg] : iL.img[iimg];
                        s->imgContent0 = icontent;
                        s->imgArea0 = icontent;
                        s->imgArea0.top = iL.imgAreaTop;
                        s->imgArea0.bottom = iL.imgAreaBottom;
                        s->imgHot = 1;
                        SetCapture(hwnd);
                        InvalidateRect(hwnd, NULL, FALSE);
                        PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_LISTBOX, 9), 0);
                        return 0;
                    }
                }
            }
            if (s->imgSelIdx >= 0) {          /* 点在照片以外：退出照片编辑模式 */
                s->imgSelIdx = -1;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            if (idx >= 0 && id) {
                RECT r;
                st_rect(s, idx, &r);
                r.top -= s->scrollY;
                r.bottom -= s->scrollY;
                s->pressRect = r;
                grip = st_grip_at(s, &r, x, y);
            }
            if (idx >= 0 && id && grip) {
                if (bp && bp->pinned) {
                    main_set_status(L"\u4fbf\u7b3a\u5df2\u56fa\u5b9a\uff0c\u53f3\u952e\u53d6\u6d88\u540e\u624d\u80fd\u8c03\u6574\u5927\u5c0f");
                } else {
                    bp = pos_find(id, 1);
                    if (bp) {
                        s->resizing = 1;
                        s->resizeId = id;
                        s->resizeGrip = grip;
                        s->rszPt0.x = x;
                        s->rszPt0.y = y;
                        s->rszW0 = st_note_w(s, bp);
                        s->rszH0 = st_note_h(s, bp);
                        SetCapture(hwnd);
                        dbg("RESIZESTART id=%d grip=%d w0=%d h0=%d rect=%d,%d,%d,%d",
                            id, grip, s->rszW0, s->rszH0, s->pressRect.left,
                            s->pressRect.top, s->pressRect.right, s->pressRect.bottom);
                    }
                }
            } else if (id) {
                s->pressId = id;
                s->pressPt.x = x;
                s->pressPt.y = y;
                dbg("PRESSPICK id=%d rect=%d,%d,%d,%d pin=%d", id,
                    s->pressRect.left, s->pressRect.top,
                    s->pressRect.right, s->pressRect.bottom, bp ? bp->pinned : -1);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_LISTBOX, 9), 0);
        }
        return 0;

    case WM_LBUTTONUP:
        if (s) {
            if (s->imgMode) {
                Schedule *sc = model_by_id(s->imgId);
                int mode = s->imgMode;
                s->imgMode = 0;
                s->imgId = 0;
                s->imgHdl = 0;
                ReleaseCapture();
                if (sc) {
                    model_save(model_path());
                    main_set_status(mode == 1 ? L"\u7167\u7247\u5df2\u79fb\u5230\u65b0\u4f4d\u7f6e\uff08\u53f3\u952e\u53ef\u590d\u4f4d\uff09"
                                             : L"\u7167\u7247\u5c3a\u5bf8\u5df2\u8c03\u6574\uff08\u53f3\u952e\u53ef\u590d\u4f4d\uff09");
                }
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            if (s->resizing) {
                wchar_t buf[96];
                BoardPos *bp = pos_find(s->resizeId, 0);
                dbg("RESIZEEND id=%d w=%d h=%d", s->resizeId,
                    bp ? bp->w : -1, bp ? bp->h : -1);
                pos_save();
                if (bp) st_size_text(bp->w, bp->h, buf, 96);
                else    wcscpy(buf, L"\u4fbf\u7b3a\u5c3a\u5bf8\u5df2\u8c03\u6574");
                s->resizing = 0;
                s->resizeId = 0;
                ReleaseCapture();
                main_set_status(buf);
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            if (s->dragging && s->dragId) {
                BoardPos *bp = pos_find(s->dragId, 1);
                if (bp) {
                    int snap = MulDiv(SNAP, s->scale, 100);
                    int nx, ny;
                    if (snap < 1) snap = 1;
                    nx = s->pressRect.left + s->dragDelta.x;
                    ny = s->pressRect.top + s->dragDelta.y;
                    nx = ((nx + snap / 2) / snap) * snap;
                    ny = ((ny + snap / 2) / snap) * snap;
                    if (nx < 0) nx = 0;
                    if (ny < 0) ny = 0;
                    bp->x = MulDiv(nx, 100, s->scale);
                    bp->y = MulDiv(ny, 100, s->scale);
                    dbg("DRAGEND id=%d delta=%d,%d pressRect=%d,%d saved=%d,%d scroll=%d",
                        s->dragId, s->dragDelta.x, s->dragDelta.y,
                        s->pressRect.left, s->pressRect.top, bp->x, bp->y, s->scrollY);
                    pos_save();
                    s->layoutOk = 0;
                    st_recalc(s);
                    st_update_scroll(s);
                    main_set_status(L"\u4fbf\u7b3a\u5df2\u79fb\u5230\u65b0\u4f4d\u7f6e\uff08\u53f3\u952e\u53ef\u56fa\u5b9a\u6216\u6062\u590d\u81ea\u52a8\u6392\u5217\uff09");
                }
            }
            if (s->dragging) ReleaseCapture();
            s->dragging = 0;
            s->dragId = 0;
            s->pressId = 0;
            s->dragDelta.x = s->dragDelta.y = 0;
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_CAPTURECHANGED:
        if (s) {
            s->dragging = 0; s->resizing = 0; s->dragId = 0; s->resizeId = 0;
            s->pressId = 0; s->imgMode = 0; s->imgId = 0; s->imgHdl = 0;
        }
        return 0;

    case WM_LBUTTONDBLCLK:
        if (s) {
            int idx = -1, x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp);
            int id = st_hit(s, x, y, &idx);
            RECT r;
            s->pressId = 0;
            s->dragging = 0;
            s->resizing = 0;
            if (id && idx >= 0) {
                st_rect(s, idx, &r);
                r.top -= s->scrollY;
                r.bottom -= s->scrollY;
                if (st_grip_at(s, &r, x, y)) {   /* 双击手柄：恢复默认大小 */
                    BoardPos *bp = pos_find(id, 1);
                    if (bp && !bp->pinned) {
                        bp->w = bp->h = 0;
                        pos_save();
                        s->layoutOk = 0;
                        st_recalc(s);
                        st_update_scroll(s);
                        InvalidateRect(hwnd, NULL, TRUE);
                        main_set_status(L"\u5df2\u6062\u590d\u9ed8\u8ba4\u5c3a\u5bf8");
                    }
                    return 0;
                }
            }
            if (id) PostMessageW(GetParent(hwnd), WM_COMMAND,
                                 MAKEWPARAM(IDM_EDIT_EDIT, 1), (LPARAM)id);
        }
        return 0;

    case WM_RBUTTONUP:
        if (s) {
            int id = st_hit(s, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), NULL);
            POINT p;
            if (s->imgSelIdx >= 0) {          /* 右键：退出照片编辑模式 */
                s->imgSelIdx = -1;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            if (id) s->selId = id;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            ClientToScreen(hwnd, &p);
            PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_WEEKVIEW, 6),
                         (LPARAM)MAKELONG(p.x, p.y));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_KEYDOWN:
        if (s) {
            int i, idx = -1;
            for (i = 0; i < s->count; i++)
                if (s->ids[i] == s->selId) { idx = i; break; }
            switch (wp) {
            case VK_LEFT:
                if (idx < 0) idx = 0; else if (idx > 0) idx--;
                break;
            case VK_RIGHT:
                if (idx < 0) idx = 0; else if (idx < s->count - 1) idx++;
                break;
            case VK_UP:
                if (idx < 0) idx = 0;
                else if (idx - s->cols >= 0) idx -= s->cols;
                break;
            case VK_DOWN:
                if (idx < 0) idx = 0;
                else if (idx + s->cols < s->count) idx += s->cols;
                break;
            case VK_HOME: idx = 0; break;
            case VK_END:  idx = s->count - 1; break;
            case 'H':
                if (s->selId) {
                    BoardPos *bp = pos_find(s->selId, 1);
                    if (bp) {
                        bp->hidden = bp->hidden ? 0 : 1;
                        pos_save();
                        s->layoutOk = 0;
                        st_recalc(s);
                        st_update_scroll(s);
                        InvalidateRect(hwnd, NULL, TRUE);
                        main_set_status(bp->hidden ? L"\u5df2\u6536\u8d77\u8fd9\u5f20\u4fbf\u7b3a\uff08\u89c6\u56fe\u83dc\u5355\u53ef\u6446\u56de\u6765\uff09"
                                                   : L"\u5df2\u6446\u56de\u8fd9\u5f20\u4fbf\u7b3a");
                    }
                }
                return 0;
            case 'P':
                if (s->selId) {
                    BoardPos *bp = pos_find(s->selId, 1);
                    if (bp) {
                        bp->pinned = !bp->pinned;
                        pos_save();
                        InvalidateRect(hwnd, NULL, TRUE);
                        main_set_status(bp->pinned ? L"\u4fbf\u7b3a\u5df2\u56fa\u5b9a"
                                                   : L"\u4fbf\u7b3a\u5df2\u53d6\u6d88\u56fa\u5b9a");
                    }
                }
                return 0;
            case VK_RETURN:
                if (s->selId) PostMessageW(GetParent(hwnd), WM_COMMAND,
                                           MAKEWPARAM(IDM_EDIT_EDIT, 1),
                                           (LPARAM)(INT_PTR)s->selId);
                return 0;
            case VK_DELETE:
                PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_WEEKVIEW, 4), 0);
                return 0;
            default: return 0;
            }
            if (idx >= 0 && idx < s->count) {
                s->selId = s->ids[idx];
                st_ensure_visible(s, s->selId);
                InvalidateRect(hwnd, NULL, FALSE);
                PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_LISTBOX, 9), 0);
            }
        }
        return 0;

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;

    case WM_DESTROY:
        if (s) {
            if (s->hTitle) DeleteObject(s->hTitle);
            if (s->hSmall) DeleteObject(s->hSmall);
            if (s->hTiny) DeleteObject(s->hTiny);
            if (s->hNum) DeleteObject(s->hNum);
            s->hTitle = s->hSmall = s->hTiny = s->hNum = NULL;
        }
        st_fonts_free();
        g_stk = NULL;
        return 0;

    case WM_NCDESTROY:
        {
            Sticky *old = (Sticky *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (old) free(old);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void stickies_register_class(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = stk_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = STK_CLASS;
    RegisterClassExW(&wc);
}

HWND stickies_create(HWND parent, HINSTANCE hInst, int id)
{
    Sticky *s = (Sticky *)calloc(1, sizeof(Sticky));
    HWND hwnd;
    if (!s) return NULL;
    g_stk = s;
    hwnd = CreateWindowExW(0, STK_CLASS, L"",
                           WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
                           0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, hInst, NULL);
    if (!hwnd) { free(s); g_stk = NULL; return NULL; }
    s->hwnd = hwnd;
    s->imgHotIdx = -1;          /* v1.9.2：悬停只影响光标 */
    s->imgSelIdx = -1;          /* v1.9.2：点选后才进入“编辑照片”状态 */
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
    st_metrics(s);
    return hwnd;
}

void stickies_set_items(HWND h, const int *ids, int count)
{
    Sticky *s = g_stk;
    int i;
    (void)h;
    if (!s) return;
    if (!g_posLoaded) pos_load();
    s->ids = (int *)ids;
    s->count = count;
    if (s->count < 0) s->count = 0;          /* v1.6 起不再有张数上限 */
    if (s->page < 1) s->page = (g_viewPage > 0) ? g_viewPage : 1;
    st_arr_ensure(s, s->count > 0 ? s->count : 1);
    /* 新出现的日程：记下“当前正在看第几页”，以后就贴在这一页 */
    for (i = 0; i < s->count; i++)
        if (model_by_id(s->ids[i])) pos_find(s->ids[i], 1);
    if (s->count > 0) g_boardInit = 0;
    st_metrics(s);
    s->layoutOk = 0;
    st_recalc(s);
    st_update_scroll(s);
    dbg("set_items n=%d page=%d/%d clientW=%d cols=%d", s->count, s->page,
        s->pageCount, s->clientW, s->cols);
    InvalidateRect(s->hwnd, NULL, TRUE);
}

/* ---------------- 翻页 ---------------- */
int stickies_page_get(HWND h)
{
    (void)h;
    return g_stk ? g_stk->page : 1;
}

int stickies_page_count(HWND h)
{
    (void)h;
    return g_stk ? (g_stk->pageCount > 0 ? g_stk->pageCount : 1) : 1;
}

int stickies_page_full(HWND h)
{
    (void)h;
    return g_stk ? g_stk->pageFull : 0;
}

int stickies_page_of(int id)
{
    return st_item_page(id);
}

static void st_page_refresh(Sticky *s)
{
    s->layoutOk = 0;
    s->scrollY = 0;
    st_recalc(s);
    st_update_scroll(s);
    InvalidateRect(s->hwnd, NULL, TRUE);
}

void stickies_page_set(HWND h, int page)
{
    Sticky *s = g_stk;
    (void)h;
    if (!s) return;
    if (!g_posLoaded) pos_load();
    if (page < 1) page = 1;
    if (page > s->pageCount) page = s->pageCount;
    if (page == s->page) return;
    s->page = page;
    s->selId = 0;         /* 旧页的选中不要带到新页 */
    g_viewPage = page;
    pos_save();
    st_page_refresh(s);
    {
        wchar_t buf[64];
        WBuf b;
        wb_init(&b, buf, 64);
        wb_add(&b, L"便笺第 ");
        wb_int(&b, page);
        wb_add(&b, L" 页");
        if (s->pageFull) wb_add(&b, L"（本页已满，可以新开一页）");
        main_set_status(buf);
    }
}

void stickies_page_new(HWND h)
{
    Sticky *s = g_stk;
    (void)h;
    if (!s) return;
    s->pageCount += 1;
    s->page = s->pageCount;
    s->selId = 0;         /* 新页上还没有选中的便笺 */
    g_viewPage = s->page;
    g_pagesSaved = s->pageCount;
    pos_save();
    st_page_refresh(s);
    {
        wchar_t buf[80];
        WBuf b;
        wb_init(&b, buf, 80);
        wb_add(&b, L"已新开第 ");
        wb_int(&b, s->page);
        wb_add(&b, L" 页 —— 在这页新建的便笺都会记在这一页");
        main_set_status(buf);
    }
}

void stickies_move_page(int id, int delta)
{
    int p;
    BoardPos *bp;
    if (id <= 0 || delta == 0) return;
    if (!g_posLoaded) pos_load();
    bp = pos_find(id, 1);
    if (!bp) return;
    p = (bp->page > 1 ? bp->page : 1) + delta;
    if (p < 1) p = 1;
    bp->page = p;
    pos_save();
    if (g_stk) st_page_refresh(g_stk);
    {
        wchar_t buf[64];
        WBuf b;
        wb_init(&b, buf, 64);
        wb_add(&b, L"这张便笺已移到第 ");
        wb_int(&b, p);
        wb_add(&b, L" 页");
        main_set_status(buf);
    }
}

void stickies_save_pos(void)
{
    if (!g_posLoaded) pos_load();
    pos_save();
}

int stickies_sel(HWND h)
{
    (void)h;
    return g_stk ? g_stk->selId : 0;
}

void stickies_set_sel(HWND h, int id)
{
    if (g_stk && id > 0) {
        int p = st_item_page(id);
        if (p != g_stk->page) stickies_page_set(h, p);   /* 跳到它所在的页 */
    }
    (void)h;
    if (!g_stk) return;
    if (g_stk->selId != id) {
        g_stk->selId = id;
        if (id) st_ensure_visible(g_stk, id);
        InvalidateRect(g_stk->hwnd, NULL, FALSE);
    }
}

void stickies_refresh(HWND h)
{
    (void)h;
    if (!g_stk) return;
    {   /* 滚动位置随尺寸变化重算（不再超出范围） */
        int i;
        for (i = 0; i < NSCR_MAX; i++) {
            if (!g_nscr[i].id) continue;
            if (!model_by_id(g_nscr[i].id)) g_nscr[i].id = g_nscr[i].off = 0;
        }
    }
    st_metrics(g_stk);
    st_recalc(g_stk);
    st_update_scroll(g_stk);
    InvalidateRect(g_stk->hwnd, NULL, TRUE);
}

int stickies_is_hidden(int id)
{
    return st_hidden(id);
}

void stickies_set_hidden(int id, int on)
{
    BoardPos *bp = pos_find(id, 1);
    if (!bp) return;
    bp->hidden = on ? 1 : 0;
    pos_save();
    if (g_stk) {
        g_stk->layoutOk = 0;
        st_recalc(g_stk);
        st_update_scroll(g_stk);
        InvalidateRect(g_stk->hwnd, NULL, TRUE);
    }
}

/* \u88ab\u6536\u8d77\u7684\u5f20\u6570\uff1a\u53ea\u6570\u201c\u5f53\u524d\u8fd9\u4e00\u9875\u91cc\u3001\u65e5\u7a0b\u8fd8\u5728\u201d\u7684\u4fbf\u7b3a\u3002
   \uff08\u4ee5\u524d\u628a\u5df2\u5220\u9664\u65e5\u7a0b\u7559\u4e0b\u7684\u65e7\u8bb0\u5f55\u4e5f\u7b97\u8fdb\u6765\uff0c\u4f1a\u51fa\u73b0\u201c\u8fd8\u6709\u4fbf\u7b3a\u663e\u793a\u5374\u63d0\u793a\u5168\u90fd\u6536\u8d77\u6765\u4e86\u201d\uff09 */
int stickies_hidden_count(void)
{
    int i, n = 0;
    if (g_stk && g_stk->count > 0) {          /* 只数“当前这一页”收起了几张 */
        for (i = 0; i < g_stk->count; i++)
            if (st_on_page(g_stk, g_stk->ids[i]) && st_hidden(g_stk->ids[i])) n++;
        return n;
    }
    for (i = 0; i < g_posCount; i++)
        if (g_pos[i].id && g_pos[i].hidden && model_by_id(g_pos[i].id)) n++;
    return n;
}

void stickies_show_all(void)
{
    int i;
    for (i = 0; i < g_posCount; i++) g_pos[i].hidden = 0;
    pos_save();
    if (g_stk) {
        g_stk->layoutOk = 0;
        st_recalc(g_stk);
        st_update_scroll(g_stk);
        InvalidateRect(g_stk->hwnd, NULL, TRUE);
    }
}

void stickies_hide_all(int on)
{
    int i;
    for (i = 0; i < g_posCount; i++) g_pos[i].hidden = on ? 1 : 0;
    pos_save();
    if (g_stk) {
        g_stk->layoutOk = 0;
        st_recalc(g_stk);
        st_update_scroll(g_stk);
        InvalidateRect(g_stk->hwnd, NULL, TRUE);
    }
}

int stickies_is_pinned(int id)
{
    BoardPos *bp = pos_find(id, 0);
    return bp ? bp->pinned : 0;
}

void stickies_set_pinned(int id, int on)
{
    BoardPos *bp = pos_find(id, 1);
    if (!bp) return;
    bp->pinned = on ? 1 : 0;
    pos_save();
    if (g_stk) InvalidateRect(g_stk->hwnd, NULL, TRUE);
}

int stickies_has_size(int id)
{
    BoardPos *bp = pos_find(id, 0);
    return (bp && bp->w > 0 && bp->h > 0) ? 1 : 0;
}

void stickies_reset_size(int id)
{
    BoardPos *bp = pos_find(id, 0);
    if (!bp) return;
    bp->w = bp->h = 0;
    pos_save();
    if (g_stk) {
        g_stk->layoutOk = 0;
        st_recalc(g_stk);
        st_update_scroll(g_stk);
        InvalidateRect(g_stk->hwnd, NULL, TRUE);
    }
}

void stickies_reset_size_all(void)
{
    int i;
    for (i = 0; i < g_posCount; i++) g_pos[i].w = g_pos[i].h = 0;
    pos_save();
    if (g_stk) {
        g_stk->layoutOk = 0;
        st_recalc(g_stk);
        st_update_scroll(g_stk);
        InvalidateRect(g_stk->hwnd, NULL, TRUE);
    }
}

void stickies_reset_pos(int id)
{
    BoardPos *bp = pos_find(id, 0);
    if (!bp) return;
    bp->x = bp->y = -1;
    pos_save();
    if (g_stk) {
        g_stk->layoutOk = 0;
        st_recalc(g_stk);
        st_update_scroll(g_stk);
        InvalidateRect(g_stk->hwnd, NULL, TRUE);
    }
}

void stickies_reset_all(void)
{
    int i;
    for (i = 0; i < g_posCount; i++) {
        g_pos[i].x = g_pos[i].y = -1;
        g_pos[i].pinned = 0;
    }
    pos_save();
    if (g_stk) {
        g_stk->layoutOk = 0;
        st_recalc(g_stk);
        st_update_scroll(g_stk);
        InvalidateRect(g_stk->hwnd, NULL, TRUE);
    }
}
