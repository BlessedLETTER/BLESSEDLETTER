/* ============================================================================
 *  ZSchedule - noteview.c
 *  便笺式独立窗口：
 *    · 一条日程一个窗口，也支持批量打开多条
 *    · 两种外观：跟随系统窗口 / 便笺纸贴在桌面（分层窗口，带真实柔和投影、
 *      纸纤维纹理、下沿厚度、右下折角）
 *    · 纸色可自选（参考微软便笺的配色）
 *    · 可拖拽移动、右下角缩放、双击进编辑、右键菜单、置顶
 *    · 窗口位置/尺寸/颜色/外观会持久化，下次启动自动恢复
 * ==========================================================================*/
#include "zsched.h"
#include <windowsx.h>

#define NOTE_RESTORE_MAX 0          /* 0 = 不限制：启动时恢复全部便笺（v1.6 起） */
#define NOTE_PAD      18          /* 纸面内边距（逻辑值） */
#define NOTE_MIN_W    210
#define NOTE_MIN_H    150
#define NOTE_DEF_W    300
#define NOTE_DEF_H    258
#define NOTE_SHADOW   12          /* 投影向外扩散的像素（逻辑值） */
#define NOTE_RADIUS   10

const COLORREF g_noteColors[NOTE_COLOR_COUNT] = {
    RGB(255, 241, 158),   /* 鹅黄 —— 微软便笺默认色 */
    RGB(216, 236, 199),   /* 苔绿 */
    RGB(247, 205, 214),   /* 樱粉 */
    RGB(226, 213, 240),   /* 丁香紫 */
    RGB(203, 225, 244),   /* 天青 */
    RGB(250, 220, 186),   /* 杏橙 */
    RGB(228, 228, 228),   /* 素灰 */
    RGB(206, 218, 232)    /* 黛蓝 */
};
const wchar_t *g_noteColorNames[NOTE_COLOR_COUNT] = {
    L"鹅黄", L"苔绿", L"樱粉", L"丁香紫", L"天青", L"杏橙", L"素灰", L"黛蓝"
};

typedef struct {
    int    id;                /* 日程 id，0 表示空闲槽 */
    HWND   hwnd;
    RECT   rc;                /* 纸面矩形（屏幕坐标，不含投影） */
    int    style;             /* NOTE_STYLE_* */
    int    color;             /* 0..7 */
    int    topmost;
    int    valid;             /* 对应日程是否存在 */
    int    hot;               /* \u9f20\u6807\u5728\u7a97\u53e3\u5185 */
    int    ttHot;             /* \u9f20\u6807\u505c\u5728\u201c\u4e0b\u4e00\u8282\u8bfe\u201d\u540d\u79f0\u884c\u4e0a\uff08\u53ef\u70b9\uff09 */
    int    ttPressId;         /* \u6309\u4e0b\u65f6\u90a3\u884c\u5bf9\u5e94\u7684\u8bfe\u7a0b id */
    int    hotBtn;            /* 悬停的按钮：0 色 1 式 2 钉 3 关，-1 无 */
    int    mode;              /* 0 静止 1 拖动 2 缩放 3 移动照片 4 缩放照片 */
    POINT  dragOrg;
    RECT   dragRc;
    /* 照片操作 */
    int    imgHot;
    int    imgHotIdx;         /* 鼠标在第几张照片上 */
    int    imgSelIdx;         /* 正在拖的是第几张 */
    int    imgHdl;
    int    imgMoved;          /* v1.9：这次按住的照片有没有真的被拖走（单击只是选中） */
    int    nscr;              /* 正文超长时的滚动位置 */
    POINT  imgPt0;
    RECT   imgBox0, imgContent0;
    RECT   imgArea0;          /* 按下时照片可用的纵向范围（四角缩放的上限） */
    int    edge;              /* \u4e66\u7b7e\u5438\u5728\u54ea\u6761\u8fb9 NOTE_EDGE_* */
    RECT   restoreRc;         /* \u4e66\u7b7e\u5f62\u6001\u4e0b\u201c\u5355\u51fb\u8fd8\u539f\u201d\u56de\u53bb\u7684\u4f4d\u7f6e */
    int    restoreStyle;      /* \u8fd8\u539f\u65f6\u7528\u7684\u5916\u89c2 */
    int    dragged;           /* \u672c\u6b21\u5de6\u952e\u662f\u5426\u62d6\u52a8\u8fc7\uff08\u533a\u5206\u70b9\u51fb\u4e0e\u62d6\u62fd\uff09 */
    HFONT  hBm;               /* \u4e66\u7b7e\u6807\u9898\u5b57\u4f53\uff08\u7ad6\u653e\u65f6\u5e26 lfEscapement\uff09 */
    int    lhBm;
    int    bmEsc;
    int    scale;             /* \u76f8\u5bf9 96 DPI \u7684\u767e\u5206\u6bd4 */
    int    fitPct;            /* 字号随便笺尺寸的自适应系数 */
    int    fontScale;         /* DPI × fit（给富文本展开用） */
    HFONT  hTitle, hText, hSmall, hNum, hHint;
    int    lhTitle, lhText, lhSmall, lhNum, lhHint;
    HDC    memDC;
    HBITMAP dib, oldDib;
    BYTE  *bits;
    int    dibW, dibH;
} NoteWnd;

/* 便笺窗口表：v1.6 起改成动态数组（不再有“最多 64 个”的上限） */
static NoteWnd *g_notes;
static int      g_noteCap;

/* 容量不够就翻倍；新槽位清零 */
static int note_ensure(int need)
{
    int nc;
    NoteWnd *np;
    if (need <= g_noteCap) return 1;
    nc = g_noteCap ? g_noteCap : 16;
    while (nc < need) nc *= 2;
    np = (NoteWnd *)realloc(g_notes, (size_t)nc * sizeof(NoteWnd));
    if (!np) return 0;
    memset(np + g_noteCap, 0, (size_t)(nc - g_noteCap) * sizeof(NoteWnd));
    g_notes = np;
    g_noteCap = nc;
    return 1;
}
static HWND      g_noteMain;      /* 主窗口：便笺上的命令都回到它这里 */

/* 便笺窗口没有父窗口（不能抢任务栏），所以消息要显式发给主窗口 */
void note_set_main(HWND h)
{
    g_noteMain = h;
}

static HWND note_top(HWND hwnd)
{
    HWND p = GetParent(hwnd);
    return p ? p : g_noteMain;
}
static wchar_t  g_notePath[MAX_PATH];
static int      g_noteLoaded;

/* \u6bcf\u6761\u65e5\u7a0b\u4e0a\u6b21\u7528\u8fc7\u7684\u4fbf\u7b3a\u5916\u89c2\uff1a\u8bfe\u8868\u63a5\u529b\u65f6\u65b0\u7a97\u53e3\u6cbf\u7528\u540c\u4e00\u5f20\u7eb8 */
typedef struct { int id, style, color; } NoteLook;
#define NOTE_LOOK_CAP 4096          /* 外观记忆只是缓存，到上限就不再记新的 */
static NoteLook *g_lookMem;
static int       g_lookCap;
static int       g_lookCount;

static void look_remember(int id, int style, int color)
{
    int i;
    if (id <= 0) return;
    for (i = 0; i < g_lookCount; i++) {
        if (g_lookMem[i].id == id) { g_lookMem[i].style = style; g_lookMem[i].color = color; return; }
    }
    if (g_lookCount >= NOTE_LOOK_CAP) return;
    if (g_lookCount >= g_lookCap) {
        int nc = g_lookCap ? g_lookCap * 2 : 64;
        NoteLook *np;
        if (nc > NOTE_LOOK_CAP) nc = NOTE_LOOK_CAP;
        np = (NoteLook *)realloc(g_lookMem, (size_t)nc * sizeof(NoteLook));
        if (!np) return;
        g_lookMem = np;
        g_lookCap = nc;
    }
    g_lookMem[g_lookCount].id = id;
    g_lookMem[g_lookCount].style = style;
    g_lookMem[g_lookCount].color = color;
    g_lookCount++;
}

int note_look_remembered(int id, int *style, int *colorIndex)
{
    int i;
    for (i = 0; i < g_lookCount; i++)
        if (g_lookMem[i].id == id) {
            if (style) *style = g_lookMem[i].style;
            if (colorIndex) *colorIndex = g_lookMem[i].color;
            return 1;
        }
    return 0;
}

static int g_ttAuto = 1;      /* 课表便笺自动跟随（存在 zsched_notes.txt 首行） */

int  note_tt_auto_get(void) { return g_ttAuto; }
void note_tt_auto_set(int on) { g_ttAuto = on ? 1 : 0; note_save_state(); }

/* \u6362\u4e86\u6570\u636e\u76ee\u5f55\uff1a\u5fd8\u6389\u7f13\u5b58\u7684\u72b6\u6001\u6587\u4ef6\u8def\u5f84\uff0c\u4e0b\u6b21 load/save \u4f1a\u6309\u65b0\u76ee\u5f55\u91cd\u7b97 */
void note_path_forget(void)
{
    g_notePath[0] = 0;
    g_noteLoaded = 0;
}

/* ======================================================================
 *  小工具
 * ====================================================================*/
static int nfont_line(HFONT f)
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

static HFONT nfont(int pt, int weight, int italic, int scale, const wchar_t *face)
{
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    int h;
    HFONT f;
    ReleaseDC(NULL, hdc);
    h = -MulDiv(pt, dpi * scale, 9600);           /* pt -> 像素，含缩放 */
    f = CreateFontW(h, 0, 0, 0, weight, italic, 0, 0, DEFAULT_CHARSET,
                    OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH, face);
    return f;
}

/* \u4e66\u7b7e\u6807\u9898\u5b57\u4f53\uff1a\u7ad6\u8d34\u5c4f\u5e55\u8fb9\u7684\u4e66\u7b7e\u9700\u8981\u628a\u5b57\u7ad6\u8d77\u6765\uff08lfEscapement\uff09 */
static HFONT nfont_esc(int pt, int weight, int scale, int esc, const wchar_t *face)
{
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    HFONT f;
    ReleaseDC(NULL, hdc);
    f = CreateFontW(-MulDiv(pt, dpi * scale, 9600), 0, esc, esc, weight, 0, 0, 0,
                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    return f;
}

/* \u5706\u89d2\u77e9\u5f62\u5916\u7684\u201c\u8ddd\u79bb\u201d\uff080 \u8868\u793a\u5728\u5185\u90e8\uff09\uff0c\u7528\u4e8e\u7b97\u6295\u5f71 alpha */
static int rr_alpha_at(const RECT *r, int rad, int depth, int x, int y)
{
    int dx = 0, dy = 0;
    int cornerX = 0, cornerY = 0;

    if (x < r->left)             { dx = r->left - x;        cornerX = 1; }
    else if (x >= r->right)      { dx = x - r->right + 1;    cornerX = 1; }
    if (y < r->top)              { dy = r->top - y;          cornerY = 1; }
    else if (y >= r->bottom)     { dy = y - r->bottom + 1;   cornerY = 1; }

    if (cornerX && cornerY) {
        int cx = (x < r->left) ? r->left + rad : r->right - rad - 1;
        int cy = (y < r->top) ? r->top + rad : r->bottom - rad - 1;
        int ex = x - cx, ey = y - cy;
        if (ex * ex + ey * ey <= rad * rad) return 255;
    } else if (!dx && !dy) {
        return 255;
    }
    {
        int d = dx > dy ? dx : dy;
        if (d <= 0) return 255;
        if (d > depth) return 0;
        {
            int a = 25 - d * 2;
            return a > 0 ? a : 0;
        }
    }
}

/* ======================================================================
 *  内容与几何
 * ====================================================================*/
static void note_metrics(NoteWnd *n)
{
    int scale, fit = 100;
    {
        HDC hdc = GetDC(NULL);
        scale = MulDiv(GetDeviceCaps(hdc, LOGPIXELSY), 100, 96);
        ReleaseDC(NULL, hdc);
    }
    if (scale < 70) scale = 70;
    /* 便笺拉大后字号跟着变：以 240×180 逻辑尺寸为基准 */
    {
        int w = n->rc.right - n->rc.left;
        int h = n->rc.bottom - n->rc.top;
        int bw = MulDiv(240, scale, 100);
        int bh = MulDiv(180, scale, 100);
        int fx = bw > 0 ? MulDiv(w, 100, bw) : 100;
        int fy = bh > 0 ? MulDiv(h, 100, bh) : 100;
        fit = fx < fy ? fx : fy;
        if (fit < 50) fit = 50;
        if (fit > 700) fit = 700;
        fit = (fit / 4) * 4;                 /* 量化到 4%，避免反复建字体 */
        if (fit < 50) fit = 50;
    }
    if (scale != n->scale || fit != n->fitPct || !n->hTitle ||
        (n->style == NOTE_STYLE_BOOKMARK &&
         n->bmEsc != ((n->edge == NOTE_EDGE_LEFT) ? 900 :
                      (n->edge == NOTE_EDGE_RIGHT ? 2700 : 0)))) {
        int fs;
        n->scale = scale;
        n->fitPct = fit;
        fs = MulDiv(scale, fit, 100);        /* 字号用的总缩放 */
        n->fontScale = fs;
        if (n->hTitle) DeleteObject(n->hTitle);
        if (n->hText) DeleteObject(n->hText);
        if (n->hSmall) DeleteObject(n->hSmall);
        if (n->hNum) DeleteObject(n->hNum);
        if (n->hHint) DeleteObject(n->hHint);
        n->hTitle = nfont(13, FW_SEMIBOLD, 0, fs, L"Microsoft YaHei UI");
        n->hText  = nfont(10, FW_NORMAL, 0, fs, L"Microsoft YaHei UI");
        n->hSmall = nfont(9,  FW_NORMAL, 0, fs, L"Microsoft YaHei UI");
        n->hNum   = nfont(16, FW_BOLD, 0, fs, L"Microsoft YaHei UI");
        n->hHint  = nfont(8,  FW_NORMAL, 0, fs, L"Microsoft YaHei UI");
        {   /* \u4e66\u7b7e\u6807\u9898\uff1a\u5de6/\u53f3\u8fb9\u7ad6\u6392\uff0c\u4e0a/\u4e0b\u8fb9\u6a2a\u6392 */
            int esc = (n->edge == NOTE_EDGE_LEFT) ? 900
                    : (n->edge == NOTE_EDGE_RIGHT ? 2700 : 0);
            if (n->hBm) { DeleteObject(n->hBm); n->hBm = NULL; }
            n->bmEsc = esc;
            n->hBm = nfont_esc(11, FW_SEMIBOLD, fs, esc, L"Microsoft YaHei UI");
            n->lhBm = nfont_line(n->hBm);
        }
        n->lhTitle = nfont_line(n->hTitle);
        n->lhText  = nfont_line(n->hText);
        n->lhSmall = nfont_line(n->hSmall);
        n->lhNum   = nfont_line(n->hNum);
        n->lhHint  = nfont_line(n->hHint);
    }
}

static int NSC(NoteWnd *n, int v) { return MulDiv(v, n->scale, 100); }

/* 悬停按钮：返回 4 个 16x16 的小方块 */
static void note_btn_rects(NoteWnd *n, RECT *out)
{
    int sz = NSC(n, 18);
    int gap = NSC(n, 5);
    int i;
    int x = (n->rc.right - n->rc.left) - NSC(n, NOTE_PAD) - (sz * 4 + gap * 3);
    int y = NSC(n, NOTE_PAD) - NSC(n, 4);
    for (i = 0; i < 4; i++) {
        out[i].left = x + i * (sz + gap);
        out[i].right = out[i].left + sz;
        out[i].top = y;
        out[i].bottom = y + sz;
    }
}

/* 纸面内的内容矩形 */
static void note_content_rect(NoteWnd *n, RECT *out)
{
    out->left = NSC(n, NOTE_PAD);
    out->top = NSC(n, NOTE_PAD);
    out->right = (n->rc.right - n->rc.left) - NSC(n, NOTE_PAD);
    out->bottom = (n->rc.bottom - n->rc.top) - NSC(n, NOTE_PAD) - NSC(n, 8);
}

/* ======================================================================
 *  绘制
 * ====================================================================*/
/* 日期 + 重复规则行（待办与照片两种情况共用） */
static void note_draw_date_line(NoteWnd *n, HDC hdc, RECT r, const Schedule *s)
{
    wchar_t b1[64], b2[128], buf[220];
    WBuf b;
    SelectObject(hdc, n->hSmall);
    SetTextColor(hdc, RGB(122, 108, 88));
    wb_init(&b, buf, 220);
    wb_add(&b, dt_text(&s->start, b1, 64));
    wb_add(&b, L"\u3000");
    wb_add(&b, repeat_text(s, b2, 128));
    DrawTextW(hdc, buf, -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
}

/* 便笺上的“待办”状态行：勾选框 + 文字（无时间待办不画日期与倒计时） */
static void note_draw_todo_line(NoteWnd *n, HDC hdc, RECT r, const Schedule *s, COLORREF col)
{
    int box = NSC(n, 13);
    int pw = NSC(n, 2);
    RECT cb = r;
    HPEN pen, op;
    HBRUSH ob;
    if (pw < 1) pw = 1;
    cb.right = cb.left + box;
    cb.bottom = cb.top + box;
    pen = CreatePen(PS_SOLID, pw, s->done ? paper_shade(col, -30) : TH_ACCENT);
    ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    op = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, cb.left, cb.top, cb.right, cb.bottom, NSC(n, 3), NSC(n, 3));
    if (s->done) {
        HPEN p2 = CreatePen(PS_SOLID, pw, paper_shade(col, -40));
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
    {
        RECT t = r;
        t.left = cb.right + NSC(n, 6);
        SelectObject(hdc, n->hSmall);
        SetTextColor(hdc, s->done ? RGB(146, 138, 124) : RGB(122, 108, 88));
        DrawTextW(hdc, s->done ? L"\u5f85\u529e \u00b7 \u5df2\u5b8c\u6210" : L"\u5f85\u529e\uff08\u65e0\u65f6\u95f4\uff09",
                  -1, &t, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

/* 便笺上图片区的矩形（与绘制用同一套公式）；返回 0 表示没有图片区 */
/* 便笺内容布局（桌面便笺）：画与命中共用 */
typedef struct {
    RECT content;            /* 标题下方 → 内容底 */
    RECT img[MAX_IMGS];      /* 每张照片各自的框 */
    RECT imgBase[MAX_IMGS];  /* 未滚动的框（存盘用） */
    int  imgCount;
    int  imgLayer;
    int  imgTop, imgBottom;
    int  imgAreaTop, imgAreaBottom;   /* 照片可用的纵向范围（标题/日期/倒计时以下） */
    int  viewTop;
    int  textUse;
    int  dateTop;            /* 0 = 不画 */
    int  boxTop, boxBottom;  /* boxTop = 0 = 不画倒计时框 */
    int  boxNum;             /* 0 = 倒计时用大字号，1 = 换小一号（空间不够自动降档） */
    int  ttNextId;           /* 0 = 不画“下一节课”名称行；否则为那节课的 id */
    time_t ttNextStart;      /* 下一节课的开始时间 */
    RECT ttName;             /* “下一节课”名称行的矩形（点击跳到编辑窗） */
    int  textTop, textH;
    int  textFont;           /* 0 = hText，1 = hSmall（文本自动适配） */
    int  gapAfterImg;
    int  scroll, maxScroll;  /* 正文超长时在视口内滚动 */
    int  contentH;
} NoteLayout;


/* 正文开头连续的空行也占正文的位置；预留“一行可见正文”时要把它们算进去，
   否则留下的那一行正好是空行，真正的文字还在下面（用户 CMC 便笺就是这种） */
static int note_lead_blank_lines(const wchar_t *t)
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

static void note_layout(NoteWnd *n, const Schedule *s, NoteLayout *NL)
{
    RECT cr;
    int y, gap, dateH, boxH;
    int imgSized = 0;          /* 1 = 用户手动调过照片大小（imgB 里有宽度千分比） */

    memset(NL, 0, sizeof(*NL));
    note_content_rect(n, &cr);
    y = cr.top + n->lhTitle + NSC(n, 2);
    NL->content.left = cr.left;
    NL->content.right = cr.right;
    NL->content.top = y;
    NL->content.bottom = cr.bottom;
    { int q2; for (q2 = 0; q2 < MAX_IMGS; q2++) SetRectEmpty(&NL->img[q2]); NL->imgCount = 0; NL->imgLayer = IMGL_OVER; }

    gap = NSC(n, 6);
    dateH = n->lhSmall + NSC(n, 5);
    boxH = (s->kind != KIND_TODO) ? (n->lhNum + NSC(n, 18) + NSC(n, 10)) : 0;
    {   /* 课表便笺：倒计时框里再挤一行“下一节课是哪个班” */
        if (boxH > 0 && s->timetable && s->ttNext) {
            time_t ns = 0, ne = 0;
            Schedule *nx = sched_class_next_any(time(NULL), &ns, &ne);
            if (nx && nx->id != s->id) {
                NL->ttNextId = nx->id;
                NL->ttNextStart = ns;
                boxH += n->lhSmall + NSC(n, 2);
            }
        }
    }
    NL->gapAfterImg = gap;

    /* 1) 先按“没有照片”排：日期 → 倒计时 → 正文起点
       课表便笺的倒计时框（含“下一节课”名称行）不允许被省略：空间不够时
       先让掉日期行、再换小一号数字字号，宁可紧凑也不把这两行吞掉 */
    {
        int needText = n->lhText + NSC(n, 4);
        int room = cr.bottom - y;
        int ttBox = (boxH > 0 && s->timetable && s->kind == KIND_TASK) ? 1 : 0;
        if (ttBox && room < boxH + needText) {
            NL->boxNum = 1;
            boxH = n->lhSmall + NSC(n, 12) + NSC(n, 8);
            if (NL->ttNextId) boxH += n->lhSmall + NSC(n, 2);
        }
        if (ttBox) {
            if (room >= dateH + boxH) {         /* 日期行有地方才留 */
                NL->dateTop = y;
                y += dateH;
            }
            NL->boxTop = y;
            NL->boxBottom = y + boxH - NSC(n, 10);
            if (NL->boxBottom > cr.bottom) NL->boxBottom = cr.bottom;
            y += boxH;
        } else if (room >= dateH + needText) {
            NL->dateTop = y;
            y += dateH;
            room -= dateH;
            if (boxH && room >= boxH + needText) {
                NL->boxTop = y;
                NL->boxBottom = y + boxH - NSC(n, 10);
                y += boxH;
            }
        } else if (room >= dateH) {
            NL->dateTop = y;
            y += dateH;
        }
        if (NL->boxTop && NL->ttNextId) {       /* 名称行 = 框内最下面那一行 */
            NL->ttName.left = cr.left + NSC(n, 12);
            NL->ttName.right = cr.right - NSC(n, 8);
            NL->ttName.bottom = NL->boxBottom;
            NL->ttName.top = NL->ttName.bottom - n->lhSmall;
            if (NL->ttName.top < NL->boxTop) NL->ttName.top = NL->boxTop;
        }
    }
    {
        int bodyTop0 = y;
        int availBody = cr.bottom - bodyTop0;
        int thFull = 0, thUse = 0;
        int n2 = s->showImg ? sched_image_count(s) : 0;
        int i;
        if (s->imgN > 0 && s->imgN < n2) n2 = s->imgN;
        if (n2 > MAX_IMGS) n2 = MAX_IMGS;
        NL->imgCount = n2;
        NL->imgLayer = s->imgLayer;
        for (i = 0; i < MAX_IMGS; i++) SetRectEmpty(&NL->img[i]);

        if (s->note[0] && availBody > 0) {
            HDC hdc = GetDC(NULL);
            RECT m;
            m.left = 0; m.top = 0; m.right = cr.right - cr.left - NSC(n, 12); m.bottom = 0;
            rich_parse(s->noteRtf, s->note, rich_doc());
            thFull = rich_draw(hdc, rich_doc(), &m, NL->textFont ? 18 : 20,
                               n->fontScale, RGB(74, 64, 52), 0);
            ReleaseDC(NULL, hdc);
        }
        /* 没有正文就不给正文留位置：否则照片下面会空出一块“留给文字却没人用”的空白 */
        thUse = (thFull > 0) ? thFull : 0;
        if (NL->imgCount > 0 && thUse > availBody * 60 / 100)
            thUse = availBody * 60 / 100;
        if (thUse > availBody) thUse = availBody;
        if (thUse < 0) thUse = 0;
        NL->textUse = thUse;
        NL->contentH = (thFull > 0) ? thFull : thUse;

        if (NL->imgCount > 0) {
            int imgH;
            int hasText = (s->note[0] || s->link[0]) ? 1 : 0;
            int lead = hasText ? note_lead_blank_lines(s->note[0] ? s->note : s->link) : 0;
            int oneLine = n->lhText + gap;   /* 照片与正文之间还隔着 gap */
            int keep = oneLine + lead * n->lhText;   /* 一行可见正文 + 开头的空行 */
            int reserve = 0;
            /* v1.9：照片被手动调过大小（拖四角 / 菜单选了比例）时，照片区直接用满正文区；
               v1.9.3：只要有正文就永远留出一行——照片再大也压不掉正文 */
            {
                int q2;
                for (q2 = 0; q2 < NL->imgCount; q2++)
                    if (s->imgB[q2][2] >= 60) { imgSized = 1; break; }
            }
            if (hasText) {
                reserve = imgSized ? keep : (thUse > keep ? thUse : keep);
                if (reserve > availBody) reserve = availBody;
                if (reserve < 0) reserve = 0;
            }
            imgH = availBody - reserve - ((s->imgMode == IMGM_END) ? gap : 0);
            int minH = NSC(n, IMG_MIN_H), minW = NSC(n, IMG_MIN_W);
            int imgTop = (s->imgMode == IMGM_END) ? (bodyTop0 + reserve + gap) : bodyTop0;
            /* 照片最小高度只在“加上它仍放得下那一行正文”时生效 */
            if (imgH < minH && (!hasText || availBody - minH >= reserve)) {
                imgH = minH;
                if (s->imgMode != IMGM_END) {
                    thUse = availBody - imgH;
                    if (thUse < 0) thUse = 0;
                    NL->textUse = thUse;
                }
            }
            if (imgH < 0) imgH = 0;
            if (imgTop + imgH > cr.bottom) imgH = cr.bottom - imgTop;
            if (imgH < 0) imgH = 0;
            NL->imgAreaTop = imgTop;
            NL->imgAreaBottom = imgTop + imgH;
            /* v1.9：照片按自己的比例“完整显示”（不再被便笺形状裁掉），
               框的大小跟着照片比例走（宽度 = 千分比宽度），所以框里不会有无效空白 */
            {
                int cw = cr.right - cr.left;
                int slot = (imgH - (NL->imgCount - 1) * NSC(n, 4)) / NL->imgCount;
                int yAuto = imgTop;
                if (slot < minH) slot = minH;
                if (slot > imgH) slot = imgH;    /* 不越过照片带（正文优先时照片带可能很小） */
                if (slot < 0) slot = 0;
                for (i = 0; i < NL->imgCount; i++) {
                    wchar_t ipath[MAX_PATH];
                    int iw = 0, ih = 0;
                    const int *b = s->imgB[i];
                    int wf = 1000, w, h;
                    RECT bx;
                    if (!sched_image_at(s, i, ipath, MAX_PATH) ||
                        !img_size(ipath, &iw, &ih) || iw <= 0 || ih <= 0) {
                        iw = ih = 1;                /* 读不到尺寸：退回铺满一块 */
                    }
                    if (b[2] >= 60 && b[2] <= 1000) wf = b[2];   /* 宽度千分比（40/55/75/100%） */
                    w = (int)((long long)cw * wf / 1000);
                    if (w < minW) w = minW;      /* 只钳宽度；高度永远按照片比例来 */
                    if (w > cw) w = cw;
                    h = (int)((long long)w * ih / iw);
                    if (h > slot) {                 /* 放不下就整体缩小：整张照片永远看得全 */
                        h = slot;
                        w = (int)((long long)h * iw / ih);
                        if (w > cw) { w = cw; h = (int)((long long)w * ih / iw); }
                    }
                    {   /* 位置：存过位置的（浮动）按存的来；否则水平居中、自上而下排 */
                        int fx = -1, fy = -1;
                        if (s->imgMode == IMGM_FLOAT && (b[0] > 0 || b[1] > 0)) {
                            int cch = NL->content.bottom - NL->content.top;
                            fx = NL->content.left + (int)((long long)cw * b[0] / 1000);
                            fy = NL->content.top + (int)((long long)cch * b[1] / 1000);
                        }
                        if (fx < 0) {
                            bx.left = cr.left + (cw - w) / 2;
                            bx.top = yAuto;
                            yAuto = bx.top + h + NSC(n, 4);
                        } else {
                            bx.left = fx;
                            bx.top = fy;
                        }
                    }
                    bx.right = bx.left + w;
                    bx.bottom = bx.top + h;
                    if (bx.left < cr.left) bx.left = cr.left;
                    if (bx.top < imgTop) bx.top = imgTop;
                    if (bx.right > cr.left + cw) {
                        bx.right = cr.left + cw;
                        bx.left = bx.right - w;
                    }
                    if (bx.bottom > imgTop + imgH) {
                        bx.bottom = imgTop + imgH;
                        bx.top = bx.bottom - h;
                    }
                    /* v1.9.2：框永远贴住照片本身，不再按最小宽度/高度撑开——
                       撑出来的那部分在 contain 绘制下就是左右/上下的无效空白 */
                    NL->img[i] = bx;
                }
            }
        } else {
            NL->imgTop = NL->imgBottom = 0;
        }

        if (NL->imgCount > 0 && s->imgMode != IMGM_END) {
            int low = NL->imgTop, q;
            for (q = 0; q < NL->imgCount; q++)
                if (NL->img[q].bottom > low) low = NL->img[q].bottom;
            NL->imgBottom = low;                    /* 用实际最低边，避免留下无效空白 */
            NL->textTop = NL->imgBottom + gap;
            NL->textH = cr.bottom - NL->textTop;
            if (!imgSized && NL->textH < thUse) NL->textH = thUse;
        } else {
            NL->textTop = bodyTop0;
            NL->textH = thUse;
        }
        if (NL->textH < 0) NL->textH = 0;
        NL->textFont = (NL->textH >= n->lhText * 2) ? 0 : 1;
        NL->scroll = 0;
        NL->maxScroll = 0;
        {   /* 滚动范围：最上元素 → 最下元素（含照片） */
            int vt = NL->dateTop ? NL->dateTop : bodyTop0;
            int vb = NL->textTop + NL->contentH;
            if (NL->imgCount > 0 && NL->imgBottom > vb) vb = NL->imgBottom;
            {
                int total = vb - vt, vis = cr.bottom - vt;
                if (vis < 0) vis = 0;
                if (total > vis) NL->maxScroll = total - vis;
            }
            NL->viewTop = vt;
            NL->scroll = n->nscr;
            if (NL->scroll > NL->maxScroll) NL->scroll = NL->maxScroll;
            if (NL->scroll < 0) NL->scroll = 0;
            if (NL->scroll) {
                int q;
                for (q = 0; q < NL->imgCount; q++) OffsetRect(&NL->img[q], 0, -NL->scroll);
                if (NL->dateTop) NL->dateTop -= NL->scroll;
                if (NL->boxTop) {
                    NL->boxTop -= NL->scroll;
                    NL->boxBottom -= NL->scroll;
                    if (NL->ttNextId) OffsetRect(&NL->ttName, 0, -NL->scroll);
                }
                NL->textTop -= NL->scroll;
            }
            {
                int q;
                for (q = 0; q < MAX_IMGS; q++) NL->imgBase[q] = NL->img[q];
                for (q = 0; q < NL->imgCount; q++)
                    if (NL->img[q].bottom < vt) SetRectEmpty(&NL->img[q]);
            }
        }
    }
}

/* 照片四角缩放手柄：命中返回 IMG_HDL_L|T 之类的组合（只认四个顶角），否则 0。
   手柄是“两个维度同时缩放”，所以只要落在角上的方形热区里就算 ——
   和“按住照片本体拖动位置”彻底分开。 */
static int note_img_corner(NoteWnd *n, POINT pt, int *pIdx, RECT *pBox, RECT *pContent,
                           RECT *pArea)
{
    Schedule *s = model_by_id(n->id);
    NoteLayout NL;
    int i, hs;
    if (!s || !s->showImg) return 0;
    note_layout(n, s, &NL);
    if (NL.imgCount <= 0) return 0;
    hs = NSC(n, IMG_HDL + 4);                 /* 热区比画出来的方块大一圈 */
    if (hs < 12) hs = 12;
    for (i = NL.imgCount - 1; i >= 0; i--) {
        RECT b = NL.img[i];
        int cx, cy, hdl = 0;
        if (b.right - b.left < 6 || b.bottom - b.top < 6) continue;
        if (pt.x >= b.left - hs && pt.x <= b.left + hs)       hdl |= IMG_HDL_L;
        else if (pt.x >= b.right - hs && pt.x <= b.right + hs) hdl |= IMG_HDL_R;
        if (pt.y >= b.top - hs && pt.y <= b.top + hs)          hdl |= IMG_HDL_T;
        else if (pt.y >= b.bottom - hs && pt.y <= b.bottom + hs) hdl |= IMG_HDL_B;
        /* 只认顶角：必须同时靠近左右之一 + 上下之一 */
        if ((hdl & (IMG_HDL_L | IMG_HDL_R)) == 0) continue;
        if ((hdl & (IMG_HDL_T | IMG_HDL_B)) == 0) continue;
        cx = (hdl & IMG_HDL_L) ? b.left : b.right;
        cy = (hdl & IMG_HDL_T) ? b.top : b.bottom;
        if (abs(pt.x - cx) > hs || abs(pt.y - cy) > hs) continue;
        if (pIdx) *pIdx = i;
        if (pBox) *pBox = b;
        if (pContent) *pContent = NL.content;
        if (pArea) {                     /* 照片可用的纵向范围（缩放的上限） */
            *pArea = NL.content;
            pArea->top = NL.imgAreaTop;
            pArea->bottom = NL.imgAreaBottom;
        }
        n->imgHotIdx = i;
        return hdl;
    }
    return 0;
}

/* 命中照片本体：*phdl 恒为 0（照片本体只负责“选中 / 拖动位置”） */
static int note_img_hit(NoteWnd *n, POINT pt, int *phdl, RECT *pContent, int *pImgIdx)
{
    Schedule *s = model_by_id(n->id);
    NoteLayout NL;
    int i, tol;
    if (pImgIdx) *pImgIdx = -1;
    if (!s || !s->showImg) return 0;
    note_layout(n, s, &NL);
    if (NL.imgCount <= 0) return 0;
    (void)tol;
    for (i = NL.imgCount - 1; i >= 0; i--) {
        RECT ib = NL.img[i];
        if (ib.right <= ib.left || ib.bottom <= ib.top) continue;
        if (pt.x >= ib.left && pt.x <= ib.right &&
            pt.y >= ib.top && pt.y <= ib.bottom) {
            if (phdl) *phdl = 0;           /* v1.9：照片没有缩放手柄，只有“移动” */
            if (pContent) *pContent = NL.content;
            if (pImgIdx) *pImgIdx = i;
            n->imgHotIdx = i;
            return 1;
        }
    }
    return 0;
}

/* 照片框夹回合法位置：只挪位置、不改尺寸（尺寸由照片比例决定） */
static void note_img_clamp(NoteWnd *n, const RECT *content, RECT *box)
{
    int w = box->right - box->left;
    int h = box->bottom - box->top;
    int minText = 0;
    int bottomLimit;
    Schedule *s = model_by_id(n->id);
    /* v1.9.3：只要这条日程有正文，照片就不能盖住最后一行（和布局的 reserve 一致） */
    if (s && (s->note[0] || s->link[0]))
        minText = (n->lhText + NSC(n, 6)) +
                  note_lead_blank_lines(s->note[0] ? s->note : s->link) * n->lhText;
    bottomLimit = content->bottom - minText;
    if (w > content->right - content->left) w = content->right - content->left;
    if (h > bottomLimit - content->top) h = bottomLimit - content->top;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (box->left < content->left) box->left = content->left;
    if (box->left + w > content->right) box->left = content->right - w;
    if (box->top < content->top) box->top = content->top;
    if (box->top + h > bottomLimit) box->top = bottomLimit - h;
    box->right = box->left + w;
    box->bottom = box->top + h;
}

/* 逐张画便笺上的照片：等比“完整显示”（不裁剪）。
   注意：照片本身**无条件绘制**（以前“浮于文字上方”那层只在悬停时画，
   结果鼠标一离开照片就整张消失）。虚线框 + 四角手柄由 note_draw_img_sel() 单独画，
   而且只在“点选”之后才画——鼠标只是路过不会进入编辑状态。 */
static void note_draw_images(HDC hdc, const Schedule *s, const NoteLayout *NL, COLORREF col)
{
    int i;
    for (i = 0; i < NL->imgCount; i++) {
        wchar_t ipath[MAX_PATH];
        RECT ib = NL->img[i];
        if (ib.right <= ib.left || ib.bottom <= ib.top) continue;
        if (!sched_image_at(s, i, ipath, MAX_PATH)) break;
        img_draw_fit(hdc, ipath, ib.left, ib.top, ib.right - ib.left, ib.bottom - ib.top);
        {
            HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(col, -30));
            HPEN op = (HPEN)SelectObject(hdc, pen);
            HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, ib.left, ib.top, ib.right - 1, ib.bottom - 1);
            SelectObject(hdc, ob);
            SelectObject(hdc, op);
            DeleteObject(pen);
        }
    }
}

/* 选中的照片：淡虚线框 + 四个顶角的缩放手柄（只有点选后才画） */
static void note_draw_img_sel(NoteWnd *n, HDC hdc, const NoteLayout *NL)
{
    RECT ib;
    int hs, cx[4], cy[4], k;
    if (n->imgSelIdx < 0 || n->imgSelIdx >= NL->imgCount) return;
    ib = NL->img[n->imgSelIdx];
    if (ib.right <= ib.left || ib.bottom <= ib.top) return;
    {
        HPEN pen = CreatePen(PS_DASH, 1, TH_ACCENT);
        HPEN op = (HPEN)SelectObject(hdc, pen);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, ib.left - 1, ib.top - 1, ib.right, ib.bottom);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
    hs = NSC(n, IMG_HDL);
    if (hs < 8) hs = 8;
    cx[0] = ib.left;  cy[0] = ib.top;
    cx[1] = ib.right; cy[1] = ib.top;
    cx[2] = ib.left;  cy[2] = ib.bottom;
    cx[3] = ib.right; cy[3] = ib.bottom;
    for (k = 0; k < 4; k++) {
        RECT hr;
        hr.left = cx[k] - hs / 2;
        hr.top = cy[k] - hs / 2;
        hr.right = hr.left + hs;
        hr.bottom = hr.top + hs;
        {
            HBRUSH hb = CreateSolidBrush(TH_SURFACE);
            HPEN hp = CreatePen(PS_SOLID, 1, TH_ACCENT);
            HBRUSH ob2 = (HBRUSH)SelectObject(hdc, hb);
            HPEN op2 = (HPEN)SelectObject(hdc, hp);
            Rectangle(hdc, hr.left, hr.top, hr.right, hr.bottom);
            SelectObject(hdc, ob2);
            SelectObject(hdc, op2);
            DeleteObject(hb);
            DeleteObject(hp);
        }
    }
}

/* \u6240\u5728\u663e\u793a\u5668\u7684\u8fb9\u754c\uff08\u591a\u5c4f\u5e55\u65f6\u8d34\u81ea\u5df1\u90a3\u5757\u5c4f\uff09 */
static void bm_monitor(const RECT *rc, RECT *mon)
{
    MONITORINFO mi;
    HMONITOR hm;
    mon->left = 0;
    mon->top = 0;
    mon->right = GetSystemMetrics(SM_CXSCREEN);
    mon->bottom = GetSystemMetrics(SM_CYSCREEN);
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    hm = MonitorFromRect(rc, MONITOR_DEFAULTTONEAREST);
    if (hm && GetMonitorInfoW(hm, &mi)) *mon = mi.rcMonitor;
}

/* \u79bb\u54ea\u6761\u8fb9\u6700\u8fd1 */
static int bm_nearest_edge(const RECT *rc)
{
    RECT mon;
    int dl, dr, dt, db;
    bm_monitor(rc, &mon);
    dl = rc->left - mon.left;
    dr = mon.right - rc->right;
    dt = rc->top - mon.top;
    db = mon.bottom - rc->bottom;
    if (dl <= dr && dl <= dt && dl <= db) return NOTE_EDGE_LEFT;
    if (dr <= dt && dr <= db)             return NOTE_EDGE_RIGHT;
    if (dt <= db)                         return NOTE_EDGE_TOP;
    return NOTE_EDGE_BOTTOM;
}

/* \u6309\u8fb9\u53d6\u4e66\u7b7e\u7684\u7eb8\u9762\u77e9\u5f62\uff08\u957f\u8fb9\u6cbf\u8fb9\u653e\uff0c\u4f4d\u7f6e\u8ddf\u7740\u5f53\u524d\u70b9\uff09 */
static void bm_rect_for(int edge, const RECT *from, int scale, RECT *out)
{
    RECT mon;
    int th = MulDiv(NOTE_BM_THICK, scale, 100);
    int ln = MulDiv(NOTE_BM_LONG, scale, 100);
    int x = from->left, y = from->top;
    bm_monitor(from, &mon);
    if (y + ln > mon.bottom) y = mon.bottom - ln;
    if (y < mon.top) y = mon.top;
    if (x + ln > mon.right) x = mon.right - ln;
    if (x < mon.left) x = mon.left;
    switch (edge) {
    case NOTE_EDGE_LEFT:
        out->left = mon.left; out->top = y;
        out->right = mon.left + th; out->bottom = y + ln;
        break;
    case NOTE_EDGE_RIGHT:
        out->left = mon.right - th; out->top = y;
        out->right = mon.right; out->bottom = y + ln;
        break;
    case NOTE_EDGE_TOP:
        out->left = x; out->top = mon.top;
        out->right = x + ln; out->bottom = mon.top + th;
        break;
    default:
        out->left = x; out->top = mon.bottom - th;
        out->right = x + ln; out->bottom = mon.bottom;
        break;
    }
}

/* \u4e66\u7b7e\u5916\u89c2\uff1a\u7eb8\u8272\u5e26\u5b50 + \u6302\u7ef3\u5b54 + \u7ad6/\u6a2a\u6807\u9898 */
static void note_draw_bookmark(NoteWnd *n, HDC hdc, const RECT *rc)
{
    Schedule *sc = model_by_id(n->id);
    COLORREF col = g_noteColors[(n->color >= 0 && n->color < NOTE_COLOR_COUNT) ? n->color : 0];
    int vertical = (n->edge == NOTE_EDGE_LEFT || n->edge == NOTE_EDGE_RIGHT);
    int u = n->scale;
    RECT r = *rc;
    HBRUSH hb, ob;
    HPEN hp, op;

    paper_fill(hdc, &r, col, 1);
    /* \u8d34\u5c4f\u5e55\u90a3\u4e00\u4fa7\u52a0\u4e00\u6761\u6df1\u8272\u53f0\u9636\uff0c\u91cc\u4fa7\u7559\u4e00\u9053\u4eae\u8fb9 */
    hb = CreateSolidBrush(paper_shade(col, -20));
    hp = CreatePen(PS_SOLID, 1, paper_shade(col, -20));
    ob = (HBRUSH)SelectObject(hdc, hb);
    op = (HPEN)SelectObject(hdc, hp);
    {
        RECT band = r;
        int t = MulDiv(4, u, 100);
        if (n->edge == NOTE_EDGE_LEFT)        { band.left = r.right - t; }
        else if (n->edge == NOTE_EDGE_RIGHT)  { band.right = r.left + t; }
        else if (n->edge == NOTE_EDGE_TOP)    { band.top = r.bottom - t; }
        else                                  { band.bottom = r.top + t; }
        Rectangle(hdc, band.left, band.top, band.right, band.bottom);
    }
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(hp);
    DeleteObject(hb);
    /* \u5916\u6846 */
    {
        HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(col, -38));
        HPEN old = (HPEN)SelectObject(hdc, pen);
        HBRUSH nb = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        int rad = MulDiv(5, u, 100);
        RoundRect(hdc, r.left, r.top, r.right - 1, r.bottom - 1, rad, rad);
        SelectObject(hdc, nb);
        SelectObject(hdc, old);
        DeleteObject(pen);
    }
    /* \u6302\u7ef3\u5b54 */
    {
        int d = MulDiv(6, u, 100);
        int cx, cy;
        HBRUSH hb2 = CreateSolidBrush(paper_shade(col, -50));
        HBRUSH ob2 = (HBRUSH)SelectObject(hdc, hb2);
        HPEN op2 = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
        if (vertical) {
            cx = (r.left + r.right) / 2;
            cy = (n->edge == NOTE_EDGE_LEFT) ? (r.top + MulDiv(15, u, 100))
                                            : (r.bottom - MulDiv(15, u, 100));
        } else {
            cy = (r.top + r.bottom) / 2;
            cx = (n->edge == NOTE_EDGE_TOP) ? (r.left + MulDiv(15, u, 100))
                                           : (r.right - MulDiv(15, u, 100));
        }
        Ellipse(hdc, cx - d, cy - d, cx + d, cy + d);
        SelectObject(hdc, op2);
        SelectObject(hdc, ob2);
        DeleteObject(hb2);
    }
    /* \u6807\u9898 */
    if (n->hBm && sc && sc->title[0]) {
        wchar_t buf[MAX_TITLE + 8];
        SIZE sz;
        TEXTMETRICW tm;
        int len, maxw, x, y;
        wcsncpy(buf, sc->title, MAX_TITLE);
        buf[MAX_TITLE] = 0;
        len = (int)wcslen(buf);
        maxw = (vertical ? (r.bottom - r.top) : (r.right - r.left)) - MulDiv(44, u, 100);
        SelectObject(hdc, n->hBm);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, paper_shade(col, -100));
        tm.tmHeight = 0;
        GetTextMetricsW(hdc, &tm);
        GetTextExtentPoint32W(hdc, buf, len, &sz);
        while (len > 1 && sz.cx > maxw) {
            len--;
            GetTextExtentPoint32W(hdc, buf, len, &sz);
        }
        if (vertical) {
            x = (r.left + r.right) / 2 + (n->edge == NOTE_EDGE_LEFT ? tm.tmHeight / 2 : -tm.tmHeight / 2);
            y = (n->edge == NOTE_EDGE_LEFT) ? (r.bottom - MulDiv(22, u, 100))
                                            : (r.top + MulDiv(22, u, 100));
        } else {
            y = (r.top + r.bottom) / 2 - tm.tmHeight / 2;
            x = (n->edge == NOTE_EDGE_TOP) ? (r.left + MulDiv(22, u, 100))
                                           : (r.right - MulDiv(22, u, 100) - (int)sz.cx);
        }
        TextOutW(hdc, x, y, buf, len);
    }
}

/* \u8bfe\u8868\u4fbf\u7b3a\u5012\u8ba1\u65f6\u6846\u4e0b\u9762\u90a3\u4e00\u884c\uff1a\u201c\u4e0b\u4e00\u8282 <\u8bfe\u540d> <\u4ec0\u4e48\u65f6\u5019>\u201d\uff0c
   \u53ef\u70b9\u51fb\uff08\u8df3\u5230\u90a3\u8282\u8bfe\u7684\u7f16\u8f91\u7a97\uff09\u3002
   \u5b57\u53f7\u4f9d\u6b21\u5c1d\u8bd5 \u5c0f\u53f7 \u2192 \u6b63\u6587 \u2192 \u6700\u5c0f\uff0c\u4fdd\u8bc1\u5728\u5f53\u524d\u4fbf\u7b3a\u5c3a\u5bf8\u4e0b\u628a\u5185\u5bb9\u663e\u793a\u5b8c\u6574 */
static void note_draw_tt_next(NoteWnd *n, HDC hdc, const RECT *r, int nextId,
                              time_t start, int hot)
{
    Schedule *nx = model_by_id(nextId);
    wchar_t buf[320], tb[64];
    WBuf b;
    HFONT fs[3];
    int i, len, avail, shrunk = 0;
    RECT rr = *r;
    if (!nx || r->right <= r->left || r->bottom <= r->top) return;
    /* 先按“只有课名”选字号：课名必须完整显示；
       地方够时再把“什么时候”也跟上 */
    wb_init(&b, buf, 320);
    wb_add(&b, L"\u4e0b\u4e00\u8282 ");
    wb_add(&b, nx->title);
    len = (int)wcslen(buf);
    avail = rr.right - rr.left;
    fs[0] = n->hSmall;
    fs[1] = n->hText;
    fs[2] = n->hHint;
    SetBkMode(hdc, TRANSPARENT);
    for (i = 0; i < 3; i++) {
        SIZE sz;
        SelectObject(hdc, fs[i]);
        sz.cx = 0;
        GetTextExtentPoint32W(hdc, buf, len, &sz);
        if (sz.cx <= avail) break;
    }
    if (i >= 3) { shrunk = 1; i = 2; SelectObject(hdc, fs[2]); }
    else {   /* 课名放得下：能塞下时间就一起显示 */
        wchar_t buf2[384];
        WBuf b2;
        int len2;
        wb_init(&b2, buf2, 384);
        wb_add(&b2, buf);
        wb_ch(&b2, L' ');
        wb_add(&b2, tt_when_text(start, tb, 64));
        len2 = (int)wcslen(buf2);
        {
            SIZE sz2;
            sz2.cx = 0;
            GetTextExtentPoint32W(hdc, buf2, len2, &sz2);
            if (sz2.cx <= avail) {
                memcpy(buf, buf2, ((size_t)len2 + 1) * sizeof(wchar_t));
                len = len2;
            }
        }
    }
    SetTextColor(hdc, hot ? RGB(38, 84, 168) : RGB(96, 84, 70));
    DrawTextW(hdc, buf, -1, &rr,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX |
              (shrunk ? DT_END_ELLIPSIS : 0));
    if (hot) {   /* \u60ac\u505c\u65f6\u52a0\u4e00\u9053\u4e0b\u5212\u7ebf\uff0c\u6697\u793a\u53ef\u70b9 */
        SIZE sz;
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(38, 84, 168));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        sz.cx = 0;
        GetTextExtentPoint32W(hdc, buf, len, &sz);
        if (sz.cx > avail) sz.cx = avail;
        MoveToEx(hdc, rr.left, rr.bottom - NSC(n, 1), NULL);
        LineTo(hdc, rr.left + sz.cx, rr.bottom - NSC(n, 1));
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
}

static void note_draw_content(NoteWnd *n, HDC hdc, const RECT *paper, int withShadow)
{
    Schedule *s = model_by_id(n->id);
    RECT cr;
    RECT pr = *paper;
    RECT paperInside = pr;
    int radius = NSC(n, NOTE_RADIUS);
    COLORREF col = g_noteColors[n->color & 7];
    int drewBig = 0;

    if (!s) return;
    if (n->style == NOTE_STYLE_BOOKMARK) {   /* \u4e66\u7b7e\u5f62\u6001\uff1a\u53ea\u753b\u4e00\u6761\u4e66\u7b7e */
        note_draw_bookmark(n, hdc, paper);
        return;
    }

    /* ---- \u7eb8\u9762 ---- */
    paper_fill(hdc, &paperInside, col, 1);
    /* 下沿厚度 */
    {
        int i;
        for (i = 0; i < 4; i++) {
            RECT band = paperInside;
            band.left += radius / 2;
            band.right -= radius / 2;
            band.bottom = paperInside.bottom - i;
            band.top = band.bottom - 1;
            if (band.top <= paperInside.top) break;
            {
                HBRUSH b = CreateSolidBrush(paper_shade(col, -(2 + i * 2)));
                FillRect(hdc, &band, b);
                DeleteObject(b);
            }
        }
    }
    /* 顶部一条略亮的“胶带压痕” */
    {
        RECT band = paperInside;
        band.bottom = band.top + NSC(n, 3);
        {
            HBRUSH b = CreateSolidBrush(paper_shade(col, 16));
            FillRect(hdc, &band, b);
            DeleteObject(b);
        }
    }
    paper_fold_corner(hdc, &paperInside, col, NSC(n, 22));
    /* 细边框 */
    {
        HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(col, -34));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        RoundRect(hdc, paperInside.left, paperInside.top,
                  paperInside.right - 1, paperInside.bottom - 1, radius, radius);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
    (void)withShadow;

    /**/
    note_content_rect(n, &cr);

    SetBkMode(hdc, TRANSPARENT);

    /* ---- 标题 ---- */
    {
        RECT r = cr;
        r.bottom = r.top + n->lhTitle;
        r.right = cr.right - NSC(n, 84);       /* 给悬停按钮留位 */
        SelectObject(hdc, n->hTitle);
        SetTextColor(hdc, RGB(48, 40, 30));
        DrawTextW(hdc, s->title, -1, &r,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }

    /* ---- 日期 / 倒计时 / 照片 / 正文：位置统一由布局给出 ---- */
    {
        NoteLayout NL;
        int svContent;
        note_layout(n, s, &NL);
        /* v1.9.3：滚动内容统一裁在固定视口里（标题不滚；照片/日期/正文滚到上面也不会压住标题） */
        svContent = SaveDC(hdc);
        {
            int clipTop = NL.viewTop ? NL.viewTop : cr.top;
            if (clipTop < cr.top) clipTop = cr.top;
            IntersectClipRect(hdc, cr.left, clipTop, cr.right, cr.bottom);
        }

        if (s->kind == KIND_TODO) {
            RECT r = cr;
            r.top = NL.dateTop;
            r.bottom = r.top + n->lhSmall;
            note_draw_todo_line(n, hdc, r, s, col);
        } else if (NL.dateTop) {
            RECT r = cr;
            r.top = NL.dateTop;
            r.bottom = r.top + n->lhSmall;
            note_draw_date_line(n, hdc, r, s);
        }

        if (NL.boxTop) {                       /* 倒计时 / 已过去 */
            RECT box = cr;
            wchar_t b2[128], buf[220], buf2[160];
            box.top = NL.boxTop;
            box.bottom = NL.boxBottom;
            box.right = cr.right - NSC(n, 6);
            {
                COLORREF fill = paper_shade(col, -12);
                HBRUSH hb = CreateSolidBrush(fill);
                HPEN p2 = CreatePen(PS_SOLID, 1, paper_shade(col, -30));
                HBRUSH ob = (HBRUSH)SelectObject(hdc, hb);
                HPEN op = (HPEN)SelectObject(hdc, p2);
                RoundRect(hdc, box.left, box.top, box.right, box.bottom,
                          NSC(n, 8), NSC(n, 8));
                SelectObject(hdc, op);
                SelectObject(hdc, ob);
                DeleteObject(p2);
                DeleteObject(hb);
            }
            {
                RECT r2 = box;
                r2.left += NSC(n, 12);
                r2.right -= NSC(n, 4);
                /* 课表便笺：名称行占框内最下面一行，倒计时挤在上面那部分；
                   便笺太小时（boxNum）倒计时自动换小一号数字字号 */
                if (NL.ttNextId) {
                    r2.bottom = NL.ttName.top - NSC(n, 1);
                    if (r2.bottom < r2.top + n->lhSmall) r2.bottom = r2.top + n->lhSmall;
                }
                SelectObject(hdc, (s->kind == KIND_ANNIV || !NL.boxNum) ? n->hNum : n->hSmall);
                SetTextColor(hdc, RGB(54, 44, 32));
                if (s->kind == KIND_ANNIV) {
                    WBuf b3;
                    int yy, mm, dd;
                    wb_init(&b3, buf, 220);
                    wb_add(&b3, sched_anniv_text(s, buf2, 160));
                    if (sched_anniv_days(s, NULL) > 0) {
                        /* 将来的倒数日不再附年月 */
                    } else {
                        sched_anniv_elapsed(s, &yy, &mm, &dd);
                        if (yy > 0) {
                            wb_add(&b3, L"\uff08");
                            wb_int(&b3, yy);
                            wb_add(&b3, L" \u5e74 ");
                            wb_int(&b3, mm);
                            wb_add(&b3, L" \u4e2a\u6708\uff09");
                        }
                    }
                    DrawTextW(hdc, buf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                } else if (s->timetable) {
                    WBuf b3;
                    time_t cs = 0, ce = 0, nw = time(NULL);
                    int unit = cd_unit_of(s);
                    int have = sched_class_window(s, &cs, &ce);
                    if (unit == CDU_SEC && !s->cdUnit) unit = CDU_MIN;   /* \u8bfe\u8868\u9ed8\u8ba4\u770b\u5230\u5206 */
                    wb_init(&b3, buf, 220);
                    if (have && nw < cs) {          /* \u8fd8\u6ca1\u4e0a\u8bfe */
                        wb_add(&b3, L"\u8ddd\u4e0a\u8bfe "); wb_add(&b3, cd_text_u(cs, unit, b2, 128));
                    } else if (have && nw < ce) {   /* \u6b63\u5728\u4e0a\u8bfe */
                        wb_add(&b3, L"\u8ddd\u4e0b\u8bfe "); wb_add(&b3, cd_text_u(ce, unit, b2, 128));
                    } else if (s->nextOcc > 0) {    /* \u4e0a\u5b8c\u4e86 / \u4eca\u5929\u6ca1\u8fd9\u8282\uff1a\u7167\u65e7\u5012\u8ba1\u65f6 */
                        wb_add(&b3, have ? L"\u4e0b\u8282\u8bfe " : L"\u8fd8\u6709 ");
                        wb_add(&b3, cd_text(s, s->nextOcc, b2, 128));
                    } else {
                        wb_add(&b3, L"\u8bfe\u8868 \u00b7 \u6682\u65e0\u540e\u7eed");
                    }
                    DrawTextW(hdc, buf, -1, &r2,
                              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                } else if (s->nextOcc > 0) {
                    WBuf b3;
                    wb_init(&b3, buf, 220);
                    wb_add(&b3, L"\u8fd8\u5269 ");
                    wb_add(&b3, cd_text(s, s->nextOcc, b2, 128));
                    DrawTextW(hdc, buf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                } else {
                    SelectObject(hdc, n->hText);
                    DrawTextW(hdc, s->done ? L"\u5df2\u5b8c\u6210" : L"\u6682\u65e0\u540e\u7eed",
                              -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
            }
            if (NL.ttNextId)
                note_draw_tt_next(n, hdc, &NL.ttName, NL.ttNextId, NL.ttNextStart, n->ttHot);
        }


        if (NL.imgCount > 0) {
            if (NL.imgLayer == IMGL_UNDER)
                note_draw_images(hdc, s, &NL, col);
            drewBig = 1;      /* 已经画了大图，不再画底部小缩略图 */
        }

        if (s->note[0] || s->link[0]) {
            RECT r3 = cr;
            int vTop = NL.textTop;          /* 布局里已经减过一次滚动位移 */
            int vBottom = cr.bottom;
            int clipTop = NL.viewTop ? NL.viewTop : cr.top;   /* 固定的正文视口顶 */
            r3.top = vTop;                  /* v1.9.3：不再重复减 scroll（否则滚到最底字全被裁掉） */
            r3.bottom = r3.top + (NL.contentH > 0 ? NL.contentH : 0);
            if (clipTop < cr.top) clipTop = cr.top;
            if (clipTop > vBottom) clipTop = vBottom;
            if (vBottom - vTop >= n->lhSmall) {     /* 正文起点已经进入视口，至少一行就画 */
                int sv = SaveDC(hdc);
                IntersectClipRect(hdc, cr.left, clipTop, cr.right, vBottom);
                if (s->note[0]) {
                    rich_parse(s->noteRtf, s->note, rich_doc());
                    rich_draw(hdc, rich_doc(), &r3,
                              NL.textFont ? 18 : 20, n->fontScale, RGB(74, 64, 52), 1);
                } else {
                    SelectObject(hdc, n->hSmall);
                    SetTextColor(hdc, RGB(96, 112, 150));
                    DrawTextW(hdc, s->link, -1, &r3,
                              DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
                }
                RestoreDC(hdc, sv);
            }
        }
        RestoreDC(hdc, svContent);      /* 滚动条、悬停按钮等固定元素不参与内容裁剪 */

        /* ---- 右侧独立滚动条：内容（含照片/正文）显示不全就一定出现 ---- */
        if (NL.maxScroll > 0) {
            int vTop = NL.viewTop, vBot = cr.bottom;
            int trackH = vBot - vTop, th, ty;
            RECT bar;
            HBRUSH hb;
            if (trackH > 8) {
                th = trackH * trackH / (trackH + NL.maxScroll);
                if (th < NSC(n, 18)) th = NSC(n, 18);
                if (th > trackH) th = trackH;
                ty = vTop + (trackH - th) * NL.scroll / (NL.maxScroll > 0 ? NL.maxScroll : 1);
                bar.left = cr.right + NSC(n, 2);
                bar.right = cr.right + NSC(n, 11);
                if (bar.right < bar.left + NSC(n, 4)) bar.right = bar.left + NSC(n, 4);
                bar.top = vTop;
                bar.bottom = vBot;
                hb = CreateSolidBrush(paper_shade(col, -6));
                FillRect(hdc, &bar, hb);
                DeleteObject(hb);
                {
                    RECT b2 = bar;
                    b2.left += NSC(n, 1);
                    b2.right -= NSC(n, 1);
                    b2.top += NSC(n, 1);
                    b2.bottom -= NSC(n, 1);
                    bar = b2;
                }
                bar.top = ty;
                bar.bottom = ty + th;
                hb = CreateSolidBrush(paper_shade(col, -70));
                FillRect(hdc, &bar, hb);
                DeleteObject(hb);
            }
        }
    }

    if (drewBig) {
        NoteLayout NL2;
        int sv2;
        note_layout(n, s, &NL2);
        /* 浮于文字上方这一层同样裁在滚动视口里（照片往上滚时不压标题/不进滚动条） */
        sv2 = SaveDC(hdc);
        {
            int clipTop = NL2.viewTop ? NL2.viewTop : cr.top;
            if (clipTop < cr.top) clipTop = cr.top;
            IntersectClipRect(hdc, cr.left, clipTop, cr.right, cr.bottom);
        }
        /* “浮于文字上方”这一层：**无条件**画在正文之上
           （以前这里要求 imgHotIdx >= 0，鼠标一离开照片就不画了 → 照片消失） */
        if (NL2.imgCount > 0 && NL2.imgLayer == IMGL_OVER)
            note_draw_images(hdc, s, &NL2, col);
        /* 选中的那张：虚线框 + 四角手柄，压在所有内容之上 */
        note_draw_img_sel(n, hdc, &NL2);
        RestoreDC(hdc, sv2);
    }

    /* ---- 附件缩略图（贴底横排；已大图时不再画） ---- */
    if (s->attachCount > 0 && !drewBig) {
        int ts = NSC(n, 38);
        int x = cr.left;
        int y = cr.bottom - ts;
        int i, maxN = (cr.right - cr.left) / (ts + NSC(n, 6));
        if (maxN < 1) maxN = 1;
        for (i = 0; i < s->attachCount && i < maxN; i++) {
            RECT tr;
            tr.left = x + i * (ts + NSC(n, 6));
            tr.top = y;
            tr.right = tr.left + ts;
            tr.bottom = y + ts;
            {
                HBRUSH b = CreateSolidBrush(paper_shade(col, 22));
                HPEN p = CreatePen(PS_SOLID, 1, paper_shade(col, -26));
                HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
                HPEN op = (HPEN)SelectObject(hdc, p);
                Rectangle(hdc, tr.left, tr.top, tr.right, tr.bottom);
                SelectObject(hdc, op);
                SelectObject(hdc, ob);
                DeleteObject(p);
                DeleteObject(b);
            }
            if (img_is_image(s->attach[i]))
                img_draw(hdc, s->attach[i], tr.left + 1, tr.top + 1, ts - 2, ts - 2);
            else
                img_draw_file_icon(hdc, s->attach[i], tr.left + 1, tr.top + 1, ts - 2, ts - 2);
        }
    }

    /* ---- 悬停按钮 ---- */
    if (n->hot) {
        RECT b[4];
        int i;
        note_btn_rects(n, b);
        for (i = 0; i < 4; i++) {
            RECT r = b[i];
            int hovered = (n->hotBtn == i);
            if (hovered) {
                HBRUSH hb = CreateSolidBrush(paper_shade(col, -18));
                HBRUSH ob = (HBRUSH)SelectObject(hdc, hb);
                HPEN op = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
                RoundRect(hdc, r.left, r.top, r.right, r.bottom, NSC(n, 6), NSC(n, 6));
                SelectObject(hdc, op);
                SelectObject(hdc, ob);
                DeleteObject(hb);
            }
            {
                int cx = (r.left + r.right) / 2, cy = (r.top + r.bottom) / 2;
                int sz = (r.right - r.left) / 2 - NSC(n, 3);
                HPEN pen = CreatePen(PS_SOLID, NSC(n, 2) > 1 ? NSC(n, 2) : 1,
                                     RGB(92, 80, 64));
                HPEN op = (HPEN)SelectObject(hdc, pen);
                if (i == 0) {                       /* 换色：圆点 */
                    HBRUSH cb = CreateSolidBrush(g_noteColors[(n->color + 1) & 7]);
                    HBRUSH ob = (HBRUSH)SelectObject(hdc, cb);
                    Ellipse(hdc, cx - sz, cy - sz, cx + sz, cy + sz);
                    SelectObject(hdc, ob);
                    DeleteObject(cb);
                } else if (i == 1) {                /* 换肤：方框带斜线 */
                    HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                    Rectangle(hdc, cx - sz, cy - sz, cx + sz + 1, cy + sz + 1);
                    SelectObject(hdc, ob);
                    MoveToEx(hdc, cx - sz, cy + sz, NULL);
                    LineTo(hdc, cx + sz, cy - sz);
                } else if (i == 2) {                /* 置顶：别针 */
                    MoveToEx(hdc, cx, cy - sz, NULL);
                    LineTo(hdc, cx, cy + sz);
                    MoveToEx(hdc, cx - sz + 1, cy - sz, NULL);
                    LineTo(hdc, cx + sz - 1, cy - sz);
                    if (!n->topmost) {
                        MoveToEx(hdc, cx - sz / 2, cy + sz, NULL);
                        LineTo(hdc, cx + sz / 2, cy + sz);
                    }
                } else {                            /* 关闭：叉 */
                    MoveToEx(hdc, cx - sz, cy - sz, NULL);
                    LineTo(hdc, cx + sz + 1, cy + sz + 1);
                    MoveToEx(hdc, cx + sz, cy - sz, NULL);
                    LineTo(hdc, cx - sz - 1, cy + sz + 1);
                }
                SelectObject(hdc, op);
                DeleteObject(pen);
            }
        }
    }

    /* ---- 右下角缩放提示（鼠标在照片上时不画，避免两类手柄混淆） ---- */
    if (n->hot && !n->imgHot) {
        HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(col, -50));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        int gx = paperInside.right - NSC(n, 16);
        int gy = paperInside.bottom - NSC(n, 16);
        int i;
        for (i = 0; i < 3; i++) {
            MoveToEx(hdc, gx + i * 4, paperInside.bottom - NSC(n, 5), NULL);
            LineTo(hdc, paperInside.right - NSC(n, 5), gy + i * 4);
        }
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
}

/* 分层窗口：渲染到 32bpp DIB 后 UpdateLayeredWindow */
static int note_render_layered(NoteWnd *n)
{
    int W, H, x, y, bm;
    RECT paper;
    BLENDFUNCTION bf;
    POINT src = { 0, 0 }, dst;
    SIZE sz;
    HDC screen;

    note_metrics(n);
    bm = (n->style == NOTE_STYLE_BOOKMARK);
    W = (n->rc.right - n->rc.left) + NSC(n, bm ? 5 : NOTE_SHADOW);
    H = (n->rc.bottom - n->rc.top) + NSC(n, bm ? 5 : NOTE_SHADOW);
    paper.left = 0;
    paper.top = 0;
    paper.right = n->rc.right - n->rc.left;
    paper.bottom = n->rc.bottom - n->rc.top;

    if (!n->memDC || n->dibW != W || n->dibH != H) {
        BITMAPINFO bi;
        if (n->oldDib) { SelectObject(n->memDC, n->oldDib); n->oldDib = NULL; }
        if (n->dib) { DeleteObject(n->dib); n->dib = NULL; }
        if (n->memDC) { DeleteDC(n->memDC); n->memDC = NULL; }
        memset(&bi, 0, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = W;
        bi.bmiHeader.biHeight = -H;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        n->memDC = CreateCompatibleDC(NULL);
        n->dib = CreateDIBSection(n->memDC, &bi, DIB_RGB_COLORS, (void **)&n->bits, NULL, 0);
        if (!n->dib || !n->bits) {
            DeleteObject(n->dib);
            DeleteDC(n->memDC);
            n->dib = NULL;
            n->memDC = NULL;
            n->bits = NULL;
            return 0;
        }
        n->oldDib = (HBITMAP)SelectObject(n->memDC, n->dib);
        n->dibW = W;
        n->dibH = H;
    }

    /* 1) 整幅先铺投影色（alpha 之后按几何重算） */
    {
        RECT all = { 0, 0, W, H };
        HBRUSH b = CreateSolidBrush(RGB(96, 86, 72));
        FillRect(n->memDC, &all, b);
        DeleteObject(b);
    }
    /* 2) 纸面与内容 */
    note_draw_content(n, n->memDC, &paper, 1);

    /* 3) 逐像素 alpha：纸面不透明，外圈按距离渐隐 */
    {
        int rad = NSC(n, bm ? 5 : NOTE_RADIUS);
        int depth = NSC(n, bm ? 5 : NOTE_SHADOW);
        for (y = 0; y < H; y++) {
            BYTE *row = n->bits + (size_t)y * W * 4;
            for (x = 0; x < W; x++) {
                row[x * 4 + 3] = (BYTE)rr_alpha_at(&paper, rad, depth, x, y);
            }
        }
    }

    screen = GetDC(NULL);
    dst.x = n->rc.left - NSC(n, bm ? 0 : 2);   /* \u4e66\u7b7e\u8d34\u7740\u5c4f\u5e55\u8fb9\uff0c\u4e0d\u7559\u504f\u79fb */
    dst.y = n->rc.top - NSC(n, bm ? 0 : 2);
    sz.cx = W;
    sz.cy = H;
    memset(&bf, 0, sizeof(bf));
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(n->hwnd, screen, &dst, &sz, n->memDC, &src, 0, &bf, ULW_ALPHA);
    ReleaseDC(NULL, screen);
    return 1;
}

/* 系统默认外观：普通窗口，WM_PAINT 直接画 */
static void note_paint_system(NoteWnd *n)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(n->hwnd, &ps);
    RECT rc, paper;
    int radius = NSC(n, NOTE_RADIUS);
    COLORREF col = g_noteColors[n->color & 7];
    GetClientRect(n->hwnd, &rc);
    paper = rc;
    paper_fill(hdc, &paper, col, 0);
    {
        HBRUSH b = CreateSolidBrush(paper_shade(col, 16));
        RECT band = paper;
        band.bottom = band.top + NSC(n, 3);
        FillRect(hdc, &band, b);
        DeleteObject(b);
    }
    paper_fold_corner(hdc, &paper, col, NSC(n, 22));
    {
        HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(col, -34));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right - 1, rc.bottom - 1);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
    (void)radius;
    note_draw_content(n, hdc, &paper, 0);
    EndPaint(n->hwnd, &ps);
}

static void note_repaint(NoteWnd *n)
{
    if (!n->hwnd || !IsWindow(n->hwnd)) return;
    {   /* 窗口标题跟日程标题（Alt+Tab / 任务栏能一眼看出换了哪一节） */
        Schedule *sm = model_by_id(n->id);
        wchar_t cur[128];
        GetWindowTextW(n->hwnd, cur, 128);
        if (sm && wcscmp(cur, sm->title) != 0) SetWindowTextW(n->hwnd, sm->title);
    }
    if (n->style != NOTE_STYLE_SYSTEM) {
        if (!note_render_layered(n)) {
            /* 分层失败就退回普通绘制 */
            InvalidateRect(n->hwnd, NULL, TRUE);
        }
    } else {
        InvalidateRect(n->hwnd, NULL, TRUE);
    }
}

/* ======================================================================
 *  命中与交互
 * ====================================================================*/
static int note_hit_btn(NoteWnd *n, POINT ptWindow)
{
    RECT b[4];
    int i;
    if (n->style == NOTE_STYLE_BOOKMARK) return -1;   /* \u4e66\u7b7e\u4e0a\u53ea\u7559\u62d6\u52a8\u4e0e\u5355\u51fb\u8fd8\u539f */
    note_btn_rects(n, b);
    for (i = 0; i < 4; i++)
        if (ptWindow.x >= b[i].left && ptWindow.x < b[i].right &&
            ptWindow.y >= b[i].top && ptWindow.y < b[i].bottom) return i;
    return -1;
}

/* \u70b9\u5728\u201c\u4e0b\u4e00\u8282\u8bfe\u201d\u540d\u79f0\u884c\u4e0a\u4e86\u5417\uff1f\uff08\u70b9\u51fb\u8df3\u5230\u90a3\u8282\u8bfe\u7684\u7f16\u8f91\u7a97\uff09 */
static int note_tt_hit(NoteWnd *n, POINT pt, int *pNextId)
{
    Schedule *s = model_by_id(n->id);
    NoteLayout NL;
    if (!s || !s->timetable || !s->ttNext) return 0;
    note_layout(n, s, &NL);
    if (!NL.ttNextId || IsRectEmpty(&NL.ttName)) return 0;
    if (pt.x < NL.ttName.left || pt.x >= NL.ttName.right) return 0;
    if (pt.y < NL.ttName.top || pt.y >= NL.ttName.bottom) return 0;
    if (pNextId) *pNextId = NL.ttNextId;
    return 1;
}

static int note_hit_grip(NoteWnd *n, POINT ptWindow)
{
    int m = NSC(n, 20);
    if (n->style == NOTE_STYLE_BOOKMARK) return 0;
    return (ptWindow.x >= (n->rc.right - n->rc.left) - m &&
            ptWindow.y >= (n->rc.bottom - n->rc.top) - m);
}

static NoteWnd *note_find(int id)
{
    int i;
    for (i = 0; i < g_noteCap; i++)
        if (g_notes[i].id == id && g_notes[i].hwnd) return &g_notes[i];
    return NULL;
}

static NoteWnd *note_alloc(void)
{
    int i;
    for (i = 0; i < g_noteCap; i++)
        if (!g_notes[i].hwnd) {
            g_notes[i].id = 0;
            return &g_notes[i];
        }
    if (!note_ensure(g_noteCap + 1)) return NULL;     /* 满员就扩容：不再有上限 */
    g_notes[g_noteCap - 1].id = 0;
    return &g_notes[g_noteCap - 1];
}

static void note_cycle_color(NoteWnd *n)
{
    n->color = (n->color + 1) % NOTE_COLOR_COUNT;
    note_repaint(n);
    note_save_state();
}

static void note_toggle_style(NoteWnd *n)
{
    RECT rcOld = n->rc;
    if (n->style == NOTE_STYLE_BOOKMARK) n->style = NOTE_STYLE_PAPER;   /* \u4e66\u7b7e\u5148\u53d8\u56de\u4fbf\u7b3a\u7eb8 */
    else n->style = (n->style == NOTE_STYLE_PAPER) ? NOTE_STYLE_SYSTEM : NOTE_STYLE_PAPER;
    /* 重建窗口：两种外观的窗口样式/扩展样式不同 */
    {
        int id = n->id, style = n->style, color = n->color, top = n->topmost;
        RECT keep = rcOld;
        DestroyWindow(n->hwnd);
        n->hwnd = NULL;
        n->id = 0;
        note_open(id, style, color, 0);
        {
            NoteWnd *m = note_find(id);
            if (m) {
                m->rc = keep;
                m->topmost = top;
                SetWindowPos(m->hwnd, m->topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                             m->rc.left - MulDiv(NOTE_SHADOW, m->scale, 100),
                             m->rc.top - MulDiv(NOTE_SHADOW, m->scale, 100),
                             (m->rc.right - m->rc.left) + MulDiv(NOTE_SHADOW, m->scale, 100),
                             (m->rc.bottom - m->rc.top) + MulDiv(NOTE_SHADOW, m->scale, 100),
                             SWP_NOACTIVATE);
                note_repaint(m);
            }
        }
    }
    note_save_state();
}

static void note_toggle_topmost(NoteWnd *n)
{
    n->topmost = !n->topmost;
    SetWindowPos(n->hwnd, n->topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    note_repaint(n);
    note_save_state();
}

static void note_do_menu(NoteWnd *n)
{
    HMENU m = CreatePopupMenu();
    HMENU mc = CreatePopupMenu();
    POINT p;
    int i, cmd;
    for (i = 0; i < NOTE_COLOR_COUNT; i++) {
        AppendMenuW(mc, MF_STRING | (i == n->color ? MF_CHECKED : 0), 3000 + i,
                    g_noteColorNames[i]);
    }
    AppendMenuW(m, MF_STRING, IDM_EDIT_EDIT, L"编辑这条日程(&E)…");
    AppendMenuW(m, MF_STRING, IDM_VIEW_PIN, L"设为倒计时目标(&P)");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)mc, L"便笺颜色(&C)");
    {
        Schedule *sm = model_by_id(n->id);
        wchar_t ip[MAX_PATH];
        int hasImg = sm && sched_first_image(sm, ip, MAX_PATH) != NULL;
        HMENU mi = CreatePopupMenu();
        static const int pcts[4] = { 40, 55, 75, 100 };
        int k;
        AppendMenuW(m, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                    (sm && sm->showImg ? MF_CHECKED : 0), IDM_NOTE_SHOWIMG,
                    L"\u5728\u4fbf\u7b3a\u4e0a\u663e\u793a\u56fe\u7247(&I)");
        for (k = 0; k < 4; k++) {
            wchar_t lb[48];
            WBuf b;
            wb_init(&b, lb, 48);
            wb_int(&b, pcts[k]);
            wb_add(&b, L"%  ");
            wb_add(&b, k == 0 ? L"\u5c0f" : (k == 1 ? L"\u4e2d"
                                    : (k == 2 ? L"\u5927" : L"\u6574\u5f20")));
            AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                        (sm && sm->imgW == pcts[k] * 10 ? MF_CHECKED : 0),
                        3500 + k, lb);
        }
        AppendMenuW(mi, MF_SEPARATOR, 0, NULL);
        {
            static const wchar_t *pm2[3] = { L"\u6d6e\u52a8\uff08\u53ef\u81ea\u7531\u6446\u653e\uff09(&F)",
                                             L"\u968f\u6587\uff08\u6587\u4e2d\uff09(&I)",
                                             L"\u6587\u672b\uff08\u6b63\u6587\u4e4b\u540e\uff09(&E)" };
            int q;
            AppendMenuW(mi, MF_SEPARATOR, 0, NULL);
            for (q = 0; q < 3; q++)
                AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                            (sm && sm->imgMode == q ? MF_CHECKED : 0), 3600 + q, pm2[q]);
            AppendMenuW(mi, MF_SEPARATOR, 0, NULL);
            AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                        (sm && sm->imgLayer == IMGL_OVER ? MF_CHECKED : 0), 3610,
                        L"\u6d6e\u4e8e\u6587\u5b57\u4e0a\u65b9(&O)");
            AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                        (sm && sm->imgLayer == IMGL_UNDER ? MF_CHECKED : 0), 3611,
                        L"\u886c\u4e8e\u6587\u5b57\u4e0b\u65b9(&U)");
            AppendMenuW(mi, MF_SEPARATOR, 0, NULL);
        }
        AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED), 3504,
                    L"\u4f4d\u7f6e\u4e0e\u5927\u5c0f\u590d\u4f4d(&R)");
        AppendMenuW(m, MF_POPUP | (hasImg ? 0 : MF_GRAYED), (UINT_PTR)mi,
                    L"照片（拖动调整位置；双击可编辑）(&Z)");
    }
    {   /* 课表模式 + 倒计时显示精度 */
        Schedule *sm2 = model_by_id(n->id);
        HMENU md = CreatePopupMenu();
        static const wchar_t *dun[4] = { L"天", L"时", L"分", L"秒" };
        int dk;
        AppendMenuW(m, MF_STRING | (sm2 && sm2->timetable ? MF_CHECKED : 0), IDM_BOARD_TT,
                    L"课表模式（到点自动开关便笺）(&L)");
        AppendMenuW(m, MF_STRING | (note_tt_auto_get() ? MF_CHECKED : 0), IDM_BOARD_TTAUTO,
                    L"课表便笺自动跟随(&A)");
        for (dk = 0; dk < 4; dk++) {
            wchar_t lb[64];
            WBuf b;
            wb_init(&b, lb, 64);
            wb_add(&b, L"显示到 ");
            wb_add(&b, dun[dk]);
            AppendMenuW(md, MF_STRING | (sm2 && cd_unit_of(sm2) == dk + 1 ? MF_CHECKED : 0),
                        IDM_CDU_DAY + dk, lb);
        }
        AppendMenuW(m, MF_POPUP, (UINT_PTR)md, L"倒计时显示精度(&U)");
    }
    {
        Schedule *sms = model_by_id(n->id);
        AppendMenuW(m, MF_STRING | (sms && sms->timetable ? 0 : MF_GRAYED) |
                    (sms && sms->ttNext ? MF_CHECKED : 0), IDM_NOTE_TTNEXT,
                    L"\u5012\u8ba1\u65f6\u4e0b\u65b9\u663e\u793a\u4e0b\u4e00\u8282\u8bfe\u540d\u79f0(&N)");
    }
    AppendMenuW(m, MF_STRING | (n->style == NOTE_STYLE_BOOKMARK ? MF_CHECKED : 0),
                IDM_NOTE_BOOKMARK,
                L"\u4e66\u7b7e\u5f62\u6001\uff08\u5438\u9644\u5c4f\u5e55\u8fb9\u6cbf\uff09(&B)");
    AppendMenuW(m, MF_STRING, IDM_NOTE_STYLE,
                n->style == NOTE_STYLE_PAPER ? L"切换为系统窗口外观(&S)"
                                             : L"切换为便笺纸外观(&S)");
    AppendMenuW(m, MF_STRING | (n->topmost ? MF_CHECKED : 0), IDM_NOTE_TOPMOST,
                L"窗口置顶(&T)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_NOTE_BATCH, L"为选中的日程都开便笺(&B)");
    AppendMenuW(m, MF_STRING, IDM_NOTE_ALL, L"为所有待办开便笺(&A)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_NOTE_CLOSE, L"关闭全部便笺(&X)");

    GetCursorPos(&p);
    SetForegroundWindow(n->hwnd);
    cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, n->hwnd, NULL);
    DestroyMenu(m);
    if (cmd == 0) return;
    if (cmd >= 3000 && cmd < 3000 + NOTE_COLOR_COUNT) {
        n->color = cmd - 3000;
        note_repaint(n);
        note_save_state();
        return;
    }
    if (cmd >= 3500 && cmd < 3504) {
        static const int pcts[4] = { 40, 55, 75, 100 };
        Schedule *sm = model_by_id(n->id);
        if (sm) {
            {
                int nimg = sched_image_count(sm), q;
                if (nimg > MAX_IMGS) nimg = MAX_IMGS;
                if (nimg < 1) nimg = 1;
                for (q = 0; q < nimg; q++) {
                    sm->imgB[q][0] = 0;
                    sm->imgB[q][1] = q * (1000 / nimg);
                    sm->imgB[q][2] = pcts[cmd - 3500] * 10;
                    sm->imgB[q][3] = (pcts[cmd - 3500] * 10) / nimg;
                    if (sm->imgB[q][3] < 100) sm->imgB[q][3] = 100;
                }
                sm->imgMode = IMGM_FLOAT;
            }
            sm->imgPct = 0;
            sm->showImg = 1;
            model_save(model_path());
            note_repaint(n);
            note_refresh_all();
        }
        return;
    }
    if (cmd >= 3600 && cmd < 3603) {
        Schedule *sm = model_by_id(n->id);
        if (sm) {
            int q;
            sm->imgMode = cmd - 3600;
            for (q = 0; q < MAX_IMGS; q++)
                sm->imgB[q][0] = sm->imgB[q][1] = sm->imgB[q][2] = sm->imgB[q][3] = 0;
            model_save(model_path());
            note_repaint(n);
            note_refresh_all();
        }
        return;
    }
    if (cmd == 3610 || cmd == 3611) {
        Schedule *sm = model_by_id(n->id);
        if (sm) {
            sm->imgLayer = (cmd == 3611) ? IMGL_UNDER : IMGL_OVER;
            model_save(model_path());
            note_repaint(n);
            note_refresh_all();
        }
        return;
    }
    if (cmd == 3504) {
        Schedule *sm = model_by_id(n->id);
        if (sm) {
            int q;
            sm->imgX = sm->imgY = sm->imgW = sm->imgH = 0;
            sm->imgPct = 0;
            for (q = 0; q < MAX_IMGS; q++)
                sm->imgB[q][0] = sm->imgB[q][1] = sm->imgB[q][2] = sm->imgB[q][3] = 0;
            model_save(model_path());
            note_repaint(n);
            note_refresh_all();
        }
        return;
    }
    switch (cmd) {
    case IDM_EDIT_EDIT:     PostMessageW(note_top(n->hwnd), WM_COMMAND,
                                        MAKEWPARAM(IDM_EDIT_EDIT, 1), (LPARAM)n->id); break;
    case IDM_VIEW_PIN:      PostMessageW(note_top(n->hwnd), WM_COMMAND,
                                        MAKEWPARAM(IDM_VIEW_PIN, 1), (LPARAM)n->id); break;
    case IDM_NOTE_SHOWIMG:
        {
            Schedule *sm = model_by_id(n->id);
            if (sm) {
                sm->showImg = sm->showImg ? 0 : 1;
                model_save(model_path());
                note_repaint(n);
                note_refresh_all();
            }
        }
        break;
    case IDM_BOARD_TT:
    case IDM_BOARD_TTAUTO:
    case IDM_CDU_DAY: case IDM_CDU_HOUR: case IDM_CDU_MIN: case IDM_CDU_SEC:
        PostMessageW(note_top(n->hwnd), WM_COMMAND, MAKEWPARAM(cmd, 1), (LPARAM)n->id);
        break;
    case IDM_NOTE_BOOKMARK:
        note_set_bookmark(n->id, n->style != NOTE_STYLE_BOOKMARK);
        break;
    case IDM_NOTE_TTNEXT:
        {
            Schedule *sm = model_by_id(n->id);
            if (sm) {
                sm->ttNext = sm->ttNext ? 0 : 1;
                model_save(model_path());
                n->scale = 0;                 /* \u5e03\u5c40\u53d8\u4e86\uff08\u591a\u4e00\u884c/\u5c11\u4e00\u884c\uff09 */
                note_repaint(n);
                note_refresh_all();
                main_set_status(sm->ttNext ? L"\u5df2\u663e\u793a\u4e0b\u4e00\u8282\u8bfe\u540d\u79f0"
                                          : L"\u5df2\u9690\u85cf\u4e0b\u4e00\u8282\u8bfe\u540d\u79f0");
            }
        }
        break;
    case IDM_NOTE_STYLE:    note_toggle_style(n); break;
    case IDM_NOTE_TOPMOST:  note_toggle_topmost(n); break;
    case IDM_NOTE_BATCH:
    case IDM_NOTE_ALL:
    case IDM_NOTE_CLOSE:
        PostMessageW(note_top(n->hwnd), WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
        break;
    }
}

/* ======================================================================
 *  窗口过程
 * ====================================================================*/
static LRESULT CALLBACK note_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    NoteWnd *n = (NoteWnd *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_ERASEBKGND:
        if (n && n->style == NOTE_STYLE_PAPER) return 1;
        break;

    case WM_PAINT:
        if (n) {
            if (n->style == NOTE_STYLE_PAPER) {
                /* 分层窗口不需要常规绘制 */
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
            } else {
                note_paint_system(n);
            }
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (n) {
            /* v1.9：滚轮不再缩放照片（照片按自己的比例自动定大小），在照片上也照样滚正文 */
            Schedule *is = model_by_id(n->id);
            NoteLayout NL;
            int dir = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1;
            if (is) {
                note_layout(n, is, &NL);
                if (NL.maxScroll > 0) {
                    int step = n->lhText * 2;
                    int off;
                    if (step < 8) step = 8;
                    off = NL.scroll + (dir > 0 ? -step : step);
                    if (off > NL.maxScroll) off = NL.maxScroll;
                    if (off < 0) off = 0;
                    if (off != NL.scroll) {
                        n->nscr = off;
                        note_repaint(n);
                    }
                }
            }
        }
        return 0;

    case WM_MOUSEMOVE:
        if (n) {
            POINT pt;
            pt.x = GET_X_LPARAM(lp);
            pt.y = GET_Y_LPARAM(lp);
            if (n->mode == 1) {
                POINT cur;
                int w = n->dragRc.right - n->dragRc.left;
                int h = n->dragRc.bottom - n->dragRc.top;
                GetCursorPos(&cur);
                n->rc.left = n->dragRc.left + (cur.x - n->dragOrg.x);
                n->rc.top = n->dragRc.top + (cur.y - n->dragOrg.y);
                n->rc.right = n->rc.left + w;      /* \u4fdd\u6301\u5c3a\u5bf8\uff1a\u5426\u5219\u62d6\u52a8\u4f1a\u628a\u4fbf\u7b3a\u538b\u6241 */
                n->rc.bottom = n->rc.top + h;
                if (abs(cur.x - n->dragOrg.x) + abs(cur.y - n->dragOrg.y) > MulDiv(4, n->scale, 100))
                    n->dragged = 1;                /* \u52a8\u8fc7\u5c31\u4e0d\u7b97\u201c\u5355\u51fb\u201d\u4e86 */
                note_repaint(n);
            } else if (n->mode == 3) {
                Schedule *is = model_by_id(n->id);
                RECT box;
                int dx, dy;
                if (!is) { n->mode = 0; return 0; }
                box = n->imgBox0;
                dx = pt.x - n->imgPt0.x;
                dy = pt.y - n->imgPt0.y;
                if (dx || dy) n->imgMoved = 1;
                OffsetRect(&box, dx, dy);
                note_img_clamp(n, &n->imgContent0, &box);
                {   /* 只记住位置；大小始终按照片自身比例（v1.9） */
                    int cw = n->imgContent0.right - n->imgContent0.left;
                    int ch = n->imgContent0.bottom - n->imgContent0.top;
                    int kk = (n->imgSelIdx >= 0 && n->imgSelIdx < MAX_IMGS) ? n->imgSelIdx : 0;
                    is->imgMode = IMGM_FLOAT;
                    if (cw > 0) {
                        int fx = (box.left - n->imgContent0.left) * 1000 / cw;
                        if (fx < 0) fx = 0;
                        if (fx > 1000) fx = 1000;
                        is->imgB[kk][0] = fx;
                    }
                    if (ch > 0) {
                        int fy = (box.top - n->imgContent0.top) * 1000 / ch;
                        if (fy < 0) fy = 0;
                        if (fy > 1000) fy = 1000;
                        is->imgB[kk][1] = fy;
                    }
                }
                note_repaint(n);
                return 0;
            } else if (n->mode == 4) {
                /* 四角缩放：两个维度一起变，始终按原比例（整张照片永远完整、不变形） */
                Schedule *is = model_by_id(n->id);
                if (!is) { n->mode = 0; ReleaseCapture(); return 0; }
                {
                    RECT box = n->imgBox0;
                    int bw = box.right - box.left, bh = box.bottom - box.top;
                    int fx = (n->imgHdl & IMG_HDL_L) ? box.right : box.left;
                    int fy = (n->imgHdl & IMG_HDL_T) ? box.bottom : box.top;
                    int dx = abs(pt.x - fx), dy = abs(pt.y - fy);
                    int nw, nh, maxW, maxH, minW, minH;
                    int kk = (n->imgSelIdx >= 0 && n->imgSelIdx < MAX_IMGS) ? n->imgSelIdx : 0;
                    int cw = n->imgContent0.right - n->imgContent0.left;
                    int ch = n->imgContent0.bottom - n->imgContent0.top;
                    if (bw < 1) bw = 1;
                    if (bh < 1) bh = 1;
                    n->imgMoved = 1;
                    /* 以“相对变化更大”的那一维为准，另一维按比例跟过去 */
                    if ((long long)dx * bh >= (long long)dy * bw) {
                        nw = dx;
                        nh = (int)((long long)nw * bh / bw);
                    } else {
                        nh = dy;
                        nw = (int)((long long)nh * bw / bh);
                    }
                    /* 最小尺寸只防缩成一条线（大尺寸由照片比例 / 可用区域决定） */
                    minW = NSC(n, 32);
                    minH = NSC(n, 24);
                    if (nw < minW) { nw = minW; nh = nw * bh / bw; }
                    if (nh < minH) { nh = minH; nw = nh * bw / bh; }
                    maxW = (n->imgHdl & IMG_HDL_L) ? (fx - n->imgContent0.left)
                                                   : (n->imgContent0.right - fx);
                    maxH = (n->imgHdl & IMG_HDL_T) ? (fy - n->imgArea0.top)
                                                   : (n->imgArea0.bottom - fy);
                    if (maxW < minW) maxW = minW;
                    if (maxH < minH) maxH = minH;
                    if (nw > maxW) { nw = maxW; nh = nw * bh / bw; }
                    if (nh > maxH) { nh = maxH; nw = nh * bw / bh; }
                    if (nw < minW) nw = minW;
                    if (nh < minH) nh = minH;
                    box.left = (n->imgHdl & IMG_HDL_L) ? fx - nw : fx;
                    box.right = box.left + nw;
                    box.top = (n->imgHdl & IMG_HDL_T) ? fy - nh : fy;
                    box.bottom = box.top + nh;
                    /* 存：宽度千分比 + 左上角位置（高度按图片比例在布局里推） */
                    is->imgMode = IMGM_FLOAT;
                    if (cw > 0) {
                        int v = nw * 1000 / cw;
                        if (v < 60) v = 60;
                        if (v > 1000) v = 1000;
                        is->imgB[kk][2] = v;
                        v = (box.left - n->imgContent0.left) * 1000 / cw;
                        if (v < 0) v = 0;
                        if (v > 1000) v = 1000;
                        is->imgB[kk][0] = v;
                    }
                    if (ch > 0) {
                        int v = (box.top - n->imgContent0.top) * 1000 / ch;
                        if (v < 0) v = 0;
                        if (v > 1000) v = 1000;
                        is->imgB[kk][1] = v;
                        v = nh * 1000 / ch;
                        if (v < 60) v = 60;
                        if (v > 1000) v = 1000;
                        is->imgB[kk][3] = v;
                    }
                    note_repaint(n);
                }
                return 0;
            } else if (n->mode == 2) {
                POINT cur;
                GetCursorPos(&cur);
                int w = n->dragRc.right - n->dragRc.left + (cur.x - n->dragOrg.x);
                int h = n->dragRc.bottom - n->dragRc.top + (cur.y - n->dragOrg.y);
                if (w < MulDiv(NOTE_MIN_W, n->scale, 100)) w = MulDiv(NOTE_MIN_W, n->scale, 100);
                if (h < MulDiv(NOTE_MIN_H, n->scale, 100)) h = MulDiv(NOTE_MIN_H, n->scale, 100);
                n->rc.right = n->rc.left + w;
                n->rc.bottom = n->rc.top + h;
                note_repaint(n);
            } else {
                int tt = note_tt_hit(n, pt, NULL);   /* \u201c\u4e0b\u4e00\u8282\u8bfe\u201d\u540d\u79f0\u884c\u7684\u60ac\u505c\u72b6\u6001 */
                int hb = note_hit_btn(n, pt);
                if (tt != n->ttHot) {
                    n->ttHot = tt;
                    note_repaint(n);
                }
                if (hb != n->hotBtn) {
                    n->hotBtn = hb;
                    note_repaint(n);
                }
                if (!n->hot) {
                    TRACKMOUSEEVENT tme;
                    n->hot = 1;
                    memset(&tme, 0, sizeof(tme));
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = hwnd;
                    TrackMouseEvent(&tme);
                    note_repaint(n);
                }
                if (note_hit_grip(n, pt)) {        /* 便笺右下角的缩放手柄优先 */
                    SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
                    return 0;
                }
                {
                    int chdl = note_img_corner(n, pt, NULL, NULL, NULL, NULL);
                    int onImg;
                    if (chdl) {                    /* 四角：两个维度一起缩放的斜向光标 */
                        int diag = (chdl == (IMG_HDL_L | IMG_HDL_T)) ||
                                   (chdl == (IMG_HDL_R | IMG_HDL_B));
                        SetCursor(LoadCursorW(NULL, diag ? IDC_SIZENWSE : IDC_SIZENESW));
                        return 0;
                    }
                    onImg = note_img_hit(n, pt, NULL, NULL, NULL);
                    if (onImg != n->imgHot) {
                        n->imgHot = onImg;
                        note_repaint(n);
                    }
                    if (onImg) {                   /* 照片本体：拖动位置（双击进编辑） */
                        SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                        return 0;
                    }
                }
                if (note_hit_grip(n, pt))
                    SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
                else if (hb >= 0)
                    SetCursor(LoadCursorW(NULL, IDC_HAND));
                else
                    SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
            }
        }
        return 0;

    case WM_MOUSELEAVE:
        if (n) {
            n->hot = 0;
            n->hotBtn = -1;
            n->imgHot = 0;
            n->imgHotIdx = -1;      /* 只清悬停（影响光标），照片和选中框不受影响 */
            note_repaint(n);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (n) {
            POINT pt;
            pt.x = GET_X_LPARAM(lp);
            pt.y = GET_Y_LPARAM(lp);
            {   /* 照片四角：两个维度同时缩放（优先于便笺右下角手柄与照片拖动） */
                int iimg = -1;
                RECT cbox, ccontent, carea;
                int hdl = note_img_corner(n, pt, &iimg, &cbox, &ccontent, &carea);
                if (hdl) {
                    Schedule *is = model_by_id(n->id);
                    if (is) is->imgMode = IMGM_FLOAT;
                    n->mode = 4;
                    n->imgHdl = hdl;
                    n->imgSelIdx = (iimg >= 0 && iimg < MAX_IMGS) ? iimg : 0;
                    n->imgHotIdx = n->imgSelIdx;
                    n->imgPt0 = pt;
                    n->imgBox0 = cbox;
                    n->imgContent0 = ccontent;
                    n->imgArea0 = carea;
                    n->imgMoved = 0;
                    SetCapture(hwnd);
                    note_repaint(n);
                    return 0;
                }
            }
            if (n->imgSelIdx >= 0) {          /* 点在照片以外：退出照片编辑模式 */
                n->imgSelIdx = -1;
                note_repaint(n);
            }
            if (note_hit_grip(n, pt)) {       /* 便笺自己的右下角手柄 */
                n->mode = 2;
                n->dragged = 0;
                n->dragRc = n->rc;
                GetCursorPos(&n->dragOrg);
                SetCapture(hwnd);
                return 0;
            }
            {
                int iimg = -1;
                RECT icontent;
                NoteLayout NL;
                Schedule *is = model_by_id(n->id);
                if (is) note_layout(n, is, &NL);
                if (note_img_hit(n, pt, NULL, &icontent, &iimg)) {
                    /* v1.9：单击选中 + 拖动（只改位置）；双击进编辑窗 */
                    if (is) is->imgMode = IMGM_FLOAT;
                    n->mode = 3;
                    n->imgSelIdx = (iimg >= 0 && iimg < MAX_IMGS) ? iimg : 0;
                    n->imgHdl = 0;
                    n->imgHotIdx = n->imgSelIdx;
                    n->imgPt0 = pt;
                    n->imgBox0 = (iimg >= 0 && NL.imgBase[iimg].right > NL.imgBase[iimg].left)
                                 ? NL.imgBase[iimg] : NL.img[iimg];
                    n->imgContent0 = icontent;
                    n->imgMoved = 0;
                    n->dragRc = n->rc;
                    SetCapture(hwnd);
                    note_repaint(n);
                    return 0;
                }
            }
            {
                int nid = 0;
                if (note_tt_hit(n, pt, &nid)) {   /* \u70b9\u5728\u201c\u4e0b\u4e00\u8282\u8bfe\u201d\u4e0a\uff1a\u7b49\u677e\u624b\u518d\u8df3 */
                    n->mode = 5;
                    n->ttPressId = nid;
                    n->dragRc = n->rc;
                    SetCapture(hwnd);
                    note_repaint(n);
                    return 0;
                }
            }
            if (note_hit_grip(n, pt)) {
                n->mode = 2;
            } else {
                int hb = note_hit_btn(n, pt);
                if (hb >= 0) {
                    n->mode = 0;
                    switch (hb) {
                    case 0: note_cycle_color(n); break;
                    case 1: note_toggle_style(n); break;
                    case 2: note_toggle_topmost(n); break;
                    default: DestroyWindow(hwnd); break;
                    }
                    return 0;
                }
                n->mode = 1;
            }
            n->dragged = 0;
            n->dragRc = n->rc;
            GetCursorPos(&n->dragOrg);
            SetCapture(hwnd);
        }
        return 0;

    case WM_LBUTTONUP:
        if (n) {
            if (n->mode == 5) {               /* \u4e0b\u4e00\u8282\u8bfe\u540d\u79f0\u884c\uff1a\u677e\u624b\u4ecd\u5728\u8fd9\u884c\u4e0a\u5c31\u6253\u5f00\u5b83\u7684\u7f16\u8f91\u7a97 */
                POINT pt;
                int nid = n->ttPressId;
                pt.x = GET_X_LPARAM(lp);
                pt.y = GET_Y_LPARAM(lp);
                n->mode = 0;
                n->ttPressId = 0;
                ReleaseCapture();
                if (nid && note_tt_hit(n, pt, NULL)) {
                    PostMessageW(note_top(n->hwnd), WM_COMMAND,
                                 MAKEWPARAM(IDM_EDIT_EDIT, 1), (LPARAM)nid);
                    main_set_status(L"\u5df2\u6253\u5f00\u4e0b\u4e00\u8282\u8bfe\u7684\u7f16\u8f91\u7a97");
                }
                note_repaint(n);
                return 0;
            }
            if (n->mode == 3 || n->mode == 4) {
                int moved = n->imgMoved;
                int wasScale = (n->mode == 4);
                n->mode = 0;
                n->imgHdl = 0;
                n->imgMoved = 0;
                ReleaseCapture();
                if (moved && wasScale) {
                    model_save(model_path());
                    main_set_status(L"照片大小已调整（两维同比例，整张照片仍然完整）");
                } else if (moved) {
                    model_save(model_path());
                    main_set_status(L"照片已移到新位置（右键「照片 → 位置与大小复位」可还原）");
                } else {
                    main_set_status(L"已选中照片：拖动可移动，拖四个角可缩放，双击进入编辑窗");
                }
                note_repaint(n);
                return 0;
            }
            if (n->mode) {
                n->mode = 0;
                ReleaseCapture();
                if (n->style == NOTE_STYLE_BOOKMARK) {
                    if (n->dragged) {          /* \u62d6\u5230\u54ea\u5c31\u5438\u5230\u6700\u8fd1\u7684\u8fb9 */
                        int edge = bm_nearest_edge(&n->rc);
                        RECT bm;
                        bm_rect_for(edge, &n->rc, n->scale, &bm);
                        n->edge = edge;
                        n->rc = bm;
                        n->scale = 0;              /* \u7ad6/\u6a2a\u6392\u5b57\u4f53\u91cd\u5efa */
                        note_repaint(n);
                        SetWindowPos(n->hwnd, n->topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                                     n->rc.left, n->rc.top,
                                     (n->rc.right - n->rc.left) + MulDiv(NOTE_SHADOW, n->scale, 100),
                                     (n->rc.bottom - n->rc.top) + MulDiv(NOTE_SHADOW, n->scale, 100),
                                     SWP_NOACTIVATE);
                        main_set_status(L"\u4e66\u7b7e\u5df2\u5438\u5728\u5c4f\u5e55\u8fb9\u6cbf\uff08\u5de6\u952e\u5355\u51fb\u53ef\u8fd8\u539f\u4f4d\u7f6e\uff09");
                        note_save_state();
                    } else {                   /* \u5355\u51fb = \u8fd8\u539f\u5230\u539f\u6765\u4f4d\u7f6e */
                        main_set_status(L"\u4e66\u7b7e\u5df2\u8fd8\u539f\u4e3a\u4fbf\u7b3a");
                        note_set_bookmark(n->id, 0);
                        return 0;
                    }
                } else {
                    note_save_state();
                }
            }
        }
        return 0;

    case WM_LBUTTONDBLCLK:
        if (n) {
            PostMessageW(note_top(hwnd), WM_COMMAND, MAKEWPARAM(IDM_EDIT_EDIT, 1),
                         (LPARAM)(INT_PTR)n->id);
        }
        return 0;

    case WM_RBUTTONUP:
        if (n) {
            if (n->imgSelIdx >= 0) {          /* 右键：退出照片编辑模式 */
                n->imgSelIdx = -1;
                note_repaint(n);
            }
            note_do_menu(n);
        }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursorW(NULL, (n && n->ttHot) ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_COMMAND:
        if (n) {
            switch (LOWORD(wp)) {
            case IDM_NOTE_BOOKMARK:
                note_set_bookmark(n->id, n->style != NOTE_STYLE_BOOKMARK);
                return 0;
            case IDM_NOTE_TTNEXT:
                {
                    Schedule *sm = model_by_id(n->id);
                    if (sm) {
                        sm->ttNext = sm->ttNext ? 0 : 1;
                        model_save(model_path());
                        n->scale = 0;
                        note_repaint(n);
                        note_refresh_all();
                    }
                }
                return 0;
            case IDM_NOTE_STYLE:   note_toggle_style(n); return 0;
            case IDM_NOTE_TOPMOST: note_toggle_topmost(n); return 0;
            case IDM_NOTE_CLOSE:   DestroyWindow(hwnd); return 0;
            }
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (n) {
            if (n->hTitle) DeleteObject(n->hTitle);
            if (n->hText) DeleteObject(n->hText);
            if (n->hSmall) DeleteObject(n->hSmall);
            if (n->hNum) DeleteObject(n->hNum);
            if (n->hHint) DeleteObject(n->hHint);
            if (n->hBm) DeleteObject(n->hBm);
            n->hBm = NULL;
            n->hTitle = n->hText = n->hSmall = n->hNum = n->hHint = NULL;
            if (n->memDC) {
                if (n->oldDib) SelectObject(n->memDC, n->oldDib);
                DeleteDC(n->memDC);
                n->memDC = NULL;
            }
            if (n->dib) {
                DeleteObject(n->dib);
                n->dib = NULL;
            }
            n->oldDib = NULL;
            n->bits = NULL;
            n->dibW = n->dibH = 0;
            n->hwnd = NULL;
            n->id = 0;
        }
        note_save_state();
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void note_register_class(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = note_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = NOTE_CLASS;
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);
}

/* ======================================================================
 *  对外接口
 * ====================================================================*/
static void note_open_ex(int schedId, int style, int colorIndex, int activate,
                         const RECT *at)
{
    NoteWnd *n = note_find(schedId);
    HWND hwnd;
    int scale;

    if (!model_by_id(schedId)) return;
    if (colorIndex < 0 || colorIndex >= NOTE_COLOR_COUNT) colorIndex = 0;

    if (n) {
        if (activate) {
            SetForegroundWindow(n->hwnd);
            note_repaint(n);
        }
        return;
    }
    n = note_alloc();
    if (!n) return;

    {
        HDC hdc = GetDC(NULL);
        scale = MulDiv(GetDeviceCaps(hdc, LOGPIXELSY), 100, 96);
        ReleaseDC(NULL, hdc);
    }
    if (scale < 70) scale = 70;
    n->scale = 0;                     /* 触发字体重建 */
    n->id = schedId;
    n->style = style;
    n->color = colorIndex;
    n->topmost = 1;
    n->hotBtn = -1;
    n->imgHotIdx = -1;      /* v1.9.2：悬停只影响光标 */
    n->imgSelIdx = -1;      /* v1.9.2：点选后才进入“编辑照片”状态 */
    /* 摆放：在工作区内平铺，一行放不下就换行（像贴了一排便签） */
    {
        RECT wa;
        int k = 0, i, cols, col, row;
        int nw = MulDiv(NOTE_DEF_W, scale, 100);
        int nh = MulDiv(NOTE_DEF_H, scale, 100);
        int gapX = MulDiv(16, scale, 100);
        int gapY = MulDiv(28, scale, 100);
        for (i = 0; i < g_noteCap; i++)
            if (g_notes[i].hwnd) k++;
        wa.left = 0; wa.top = 0; wa.right = 1920; wa.bottom = 1080;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        cols = (wa.right - wa.left - MulDiv(90, scale, 100)) / (nw + gapX);
        if (cols < 1) cols = 1;
        col = k % cols;
        row = k / cols;
        n->rc.left = wa.left + MulDiv(45, scale, 100) + col * (nw + gapX);
        n->rc.top = wa.top + MulDiv(28, scale, 100) + row * (nh + gapY);
        if (n->rc.left + nw > wa.right - MulDiv(16, scale, 100))
            n->rc.left = wa.right - nw - MulDiv(16, scale, 100);
        if (n->rc.top + nh > wa.bottom - MulDiv(16, scale, 100))
            n->rc.top = wa.bottom - nh - MulDiv(16, scale, 100);
        if (n->rc.left < wa.left) n->rc.left = wa.left;
        if (n->rc.top < wa.top) n->rc.top = wa.top;
        n->rc.right = n->rc.left + nw;
        n->rc.bottom = n->rc.top + nh;
    }
    if (at) {                  /* \u6307\u5b9a\u4f4d\u7f6e\uff1a\u8bfe\u8868\u63a5\u529b\u65f6\u65b0\u4fbf\u7b3a\u8d34\u5728\u4e0a\u4e00\u8282\u7684\u4f4d\u7f6e\u4e0a */
        RECT wa;
        int w = at->right - at->left, h = at->bottom - at->top;
        if (w < MulDiv(NOTE_MIN_W, scale, 100)) w = MulDiv(NOTE_MIN_W, scale, 100);
        if (h < MulDiv(NOTE_MIN_H, scale, 100)) h = MulDiv(NOTE_MIN_H, scale, 100);
        wa.left = 0; wa.top = 0; wa.right = 1920; wa.bottom = 1080;
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        n->rc.left = at->left;
        n->rc.top = at->top;
        if (n->rc.left + w > wa.right - MulDiv(8, scale, 100))
            n->rc.left = wa.right - w - MulDiv(8, scale, 100);
        if (n->rc.top + h > wa.bottom - MulDiv(8, scale, 100))
            n->rc.top = wa.bottom - h - MulDiv(8, scale, 100);
        if (n->rc.left < wa.left) n->rc.left = wa.left;
        if (n->rc.top < wa.top) n->rc.top = wa.top;
        n->rc.right = n->rc.left + w;
        n->rc.bottom = n->rc.top + h;
    }
    look_remember(schedId, style, colorIndex);
    note_metrics(n);

    hwnd = CreateWindowExW((style != NOTE_STYLE_SYSTEM)
                               ? (WS_EX_LAYERED | WS_EX_TOOLWINDOW | (n->topmost ? WS_EX_TOPMOST : 0))
                               : (n->topmost ? WS_EX_TOPMOST : 0),
                           NOTE_CLASS, L"\u65e5\u7a0b\u4fbf\u7b3a",
                           (style != NOTE_STYLE_SYSTEM) ? WS_POPUP : WS_OVERLAPPEDWINDOW,
                           n->rc.left, n->rc.top,
                           (n->rc.right - n->rc.left) + ((style != NOTE_STYLE_SYSTEM) ? MulDiv(NOTE_SHADOW, scale, 100) : 0),
                           (n->rc.bottom - n->rc.top) + ((style != NOTE_STYLE_SYSTEM) ? MulDiv(NOTE_SHADOW, scale, 100) : 0),
                           NULL, NULL, g_hInst, NULL);
    if (!hwnd) {
        n->id = 0;
        return;
    }
    n->hwnd = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)n);
    if (style == NOTE_STYLE_SYSTEM) {
        RECT rc = { 0, 0, n->rc.right - n->rc.left, n->rc.bottom - n->rc.top };
        AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
        SetWindowPos(hwnd, NULL, n->rc.left, n->rc.top,
                     rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    ShowWindow(hwnd, activate ? SW_SHOW : SW_SHOWNA);
    if (activate) SetForegroundWindow(hwnd);
    note_repaint(n);
}

void note_open(int schedId, int style, int colorIndex, int activate)
{
    note_open_ex(schedId, style, colorIndex, activate, NULL);
}

void note_open_at(int schedId, int style, int colorIndex, const RECT *rc, int activate)
{
    note_open_ex(schedId, style, colorIndex, activate, rc);
}

void note_open_batch(const int *ids, int count, int style, int colorIndex)
{
    int i;
    for (i = 0; i < count; i++)
        note_open(ids[i], style, colorIndex, i == 0);
}

void note_close_all(void)
{
    int i;
    for (i = 0; i < g_noteCap; i++)
        if (g_notes[i].hwnd) DestroyWindow(g_notes[i].hwnd);
    note_save_state();
}

int note_is_open(int id) { return note_find(id) != NULL; }

int note_count(void)
{
    int i, k = 0;
    for (i = 0; i < g_noteCap; i++)
        if (g_notes[i].hwnd) k++;
    return k;
}

void note_set_style(int id, int style)
{
    NoteWnd *n = note_find(id);
    if (n && n->style != style) note_toggle_style(n);
}

/* \u5173\u6389\u518d\u6309\u6307\u5b9a\u5916\u89c2/\u4f4d\u7f6e\u91cd\u5f00\uff08\u53ea\u7528\u4e8e\u4e24\u79cd\u5206\u5c42\u5916\u89c2\u4e4b\u95f4\u5207\u6362\uff09 */
static void note_reopen_layered(NoteWnd *n, int style, const RECT *rc, int edge,
                                const RECT *restoreRc, int restoreStyle)
{
    int id = n->id, color = n->color, top = n->topmost;
    RECT keep = *rc;
    DestroyWindow(n->hwnd);
    n->hwnd = NULL;
    n->id = 0;
    note_open_at(id, style, color, &keep, 0);
    {
        NoteWnd *m = note_find(id);
        if (!m) return;
        m->rc = keep;
        m->topmost = top;
        m->edge = edge;
        m->restoreStyle = restoreStyle;
        if (restoreRc) m->restoreRc = *restoreRc;
        m->scale = 0;                        /* \u89e6\u53d1\u5b57\u4f53/\u5e03\u5c40\u91cd\u5efa */
        note_repaint(m);
        SetWindowPos(m->hwnd, m->topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                     m->rc.left, m->rc.top,
                     (m->rc.right - m->rc.left) + MulDiv(NOTE_SHADOW, m->scale, 100),
                     (m->rc.bottom - m->rc.top) + MulDiv(NOTE_SHADOW, m->scale, 100),
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

int note_is_bookmark(int id)
{
    NoteWnd *n = note_find(id);
    return (n && n->style == NOTE_STYLE_BOOKMARK) ? 1 : 0;
}

/* \u4e66\u7b7e\u5f62\u6001\uff1a\u5438\u5728\u5c4f\u5e55\u8fb9\u6cbf\uff1b\u5173\u6389\u5219\u8fd8\u539f\u5230\u539f\u6765\u4f4d\u7f6e */
void note_set_bookmark(int id, int on)
{
    NoteWnd *n = note_find(id);
    if (!n) return;
    if (on) {
        RECT keep, bm;
        int edge, rstyle;
        if (n->style == NOTE_STYLE_BOOKMARK) return;
        keep = n->rc;
        rstyle = (n->style == NOTE_STYLE_SYSTEM) ? NOTE_STYLE_PAPER : n->style;
        edge = bm_nearest_edge(&n->rc);
        bm_rect_for(edge, &n->rc, n->scale, &bm);
        note_reopen_layered(n, NOTE_STYLE_BOOKMARK, &bm, edge, &keep, rstyle);
    } else {
        RECT back;
        int st;
        if (n->style != NOTE_STYLE_BOOKMARK) return;
        back = n->restoreRc;
        st = n->restoreStyle ? n->restoreStyle : NOTE_STYLE_PAPER;
        if (back.right <= back.left || back.bottom <= back.top) {
            back = n->rc;
            back.right = back.left + MulDiv(NOTE_DEF_W, n->scale, 100);
            back.bottom = back.top + MulDiv(NOTE_DEF_H, n->scale, 100);
        }
        note_reopen_layered(n, st, &back, NOTE_EDGE_LEFT, NULL, st);
    }
    note_save_state();
}

void note_set_color(int id, int colorIndex)
{
    NoteWnd *n = note_find(id);
    if (!n) return;
    if (colorIndex < 0 || colorIndex >= NOTE_COLOR_COUNT) return;
    n->color = colorIndex;
    look_remember(n->id, n->style, n->color);
    note_repaint(n);
    note_save_state();
}

void note_close_one(int id)
{
    NoteWnd *n = note_find(id);
    if (n) DestroyWindow(n->hwnd);
}

void note_toggle_top(int id)
{
    NoteWnd *n = note_find(id);
    if (n) note_toggle_topmost(n);
}

void note_get_look(int id, int *style, int *colorIndex)
{
    NoteWnd *n = note_find(id);
    if (n) {
        if (style) *style = n->style;
        if (colorIndex) *colorIndex = n->color;
    }
}

/* \u4fbf\u7b3a\u5f00\u7740\u65f6\u7684\u51e0\u4f55\u4fe1\u606f\uff08\u8bfe\u8868\u63a5\u529b\u8981\u62ff\u4e0a\u4e00\u8282\u7684\u4f4d\u7f6e\u4e0e\u7eb8\u8272\uff09 */
int note_get_geom(int id, int *style, int *colorIndex, RECT *rc)
{
    NoteWnd *n = note_find(id);
    if (!n) return 0;
    if (style) *style = n->style;
    if (colorIndex) *colorIndex = n->color;
    if (rc) *rc = n->rc;
    return 1;
}

int note_list(int *ids, int max)
{
    int i, k = 0;
    for (i = 0; i < g_noteCap; i++) {
        if (!g_notes[i].hwnd) continue;
        if (k < max) ids[k] = g_notes[i].id;
        k++;
    }
    return k;
}

/* 数据变化后刷新所有便笺；日程被删除的自动关闭 */
void note_refresh_all(void)
{
    int i;
    for (i = 0; i < g_noteCap; i++) {
        if (!g_notes[i].hwnd) continue;
        if (!model_by_id(g_notes[i].id)) {
            DestroyWindow(g_notes[i].hwnd);
            continue;
        }
        note_repaint(&g_notes[i]);
    }
}

/* --------------------------------------------------------------- 持久化 */
static void note_path_init(void)
{
    const wchar_t *p;
    const wchar_t *slash;
    size_t n;
    if (g_notePath[0]) return;
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
    memcpy(g_notePath, p, n * sizeof(wchar_t));
    g_notePath[n] = 0;
    wcscat(g_notePath, L"zsched_notes.txt");
}

void note_save_state(void)
{
    WStr out;
    int i;
    HANDLE h;
    DWORD wrote = 0;
    unsigned short bom = 0xFEFF;

    note_path_init();
    if (!g_notePath[0]) return;
    ws_init(&out);
    ws_add(&out, g_ttAuto ? L"ttauto=1\r\n" : L"ttauto=0\r\n");
    for (i = 0; i < g_noteCap; i++) {
        NoteWnd *n = &g_notes[i];
        if (!n->hwnd) continue;
        ws_add(&out, L"id=");      ws_int(&out, n->id);      ws_add(&out, L" ");
        ws_add(&out, L"style=");   ws_int(&out, n->style);   ws_add(&out, L" ");
        ws_add(&out, L"color=");   ws_int(&out, n->color);   ws_add(&out, L" ");
        ws_add(&out, L"top=");     ws_int(&out, n->topmost); ws_add(&out, L" ");
        ws_add(&out, L"x=");       ws_int(&out, n->rc.left); ws_add(&out, L" ");
        ws_add(&out, L"y=");       ws_int(&out, n->rc.top);  ws_add(&out, L" ");
        ws_add(&out, L"w=");       ws_int(&out, n->rc.right - n->rc.left); ws_add(&out, L" ");
        ws_add(&out, L"h=");       ws_int(&out, n->rc.bottom - n->rc.top); ws_add(&out, L"\r\n");
        if (n->style == NOTE_STYLE_BOOKMARK) {   /* \u4e66\u7b7e\uff1a\u8bb0\u4e0b\u5438\u54ea\u6761\u8fb9 + \u8fd8\u539f\u56de\u53bb\u7684\u4f4d\u7f6e */
            ws_add(&out, L"edge=");   ws_int(&out, n->edge);         ws_add(&out, L" ");
            ws_add(&out, L"rstyle="); ws_int(&out, n->restoreStyle);  ws_add(&out, L" ");
            ws_add(&out, L"rx=");     ws_int(&out, n->restoreRc.left); ws_add(&out, L" ");
            ws_add(&out, L"ry=");     ws_int(&out, n->restoreRc.top);  ws_add(&out, L" ");
            ws_add(&out, L"rw=");     ws_int(&out, n->restoreRc.right - n->restoreRc.left); ws_add(&out, L" ");
            ws_add(&out, L"rh=");     ws_int(&out, n->restoreRc.bottom - n->restoreRc.top);
        }
        ws_add(&out, L"\r\n");
    }
    h = CreateFileW(g_notePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { ws_free(&out); return; }
    WriteFile(h, &bom, 2, &wrote, NULL);
    if (out.p) WriteFile(h, out.p, (DWORD)(out.len * sizeof(wchar_t)), &wrote, NULL);
    CloseHandle(h);
    ws_free(&out);
}

void note_load_state(void)
{
    HANDLE h;
    DWORD size, rd = 0;
    wchar_t *buf, *p;
    int slots = 0;

    if (g_noteLoaded) return;
    g_noteLoaded = 1;
    note_path_init();
    if (!g_notePath[0]) return;
    h = CreateFileW(g_notePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(h, NULL);
    if (size < 4 || size > 1024 * 1024) { CloseHandle(h); return; }
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
        int id = 0, style = NOTE_STYLE_PAPER, color = 0, top = 1;
        int x = 160, y = 160, w = 0, h2 = 0;
        int edge = NOTE_EDGE_RIGHT, rstyle = NOTE_STYLE_PAPER;
        int rx = 0, ry = 0, rw = 0, rh = 0;
        if (nl) { *nl = 0; }
        /* 逐字段解析 */
        {
            wchar_t *q = p;
            while (*q) {
                if (!wcsncmp(q, L"id=", 3))         { id = (int)wcstol(q + 3, NULL, 10); }
                else if (!wcsncmp(q, L"style=", 6)) { style = (int)wcstol(q + 6, NULL, 10); }
                else if (!wcsncmp(q, L"color=", 6)) { color = (int)wcstol(q + 6, NULL, 10); }
                else if (!wcsncmp(q, L"top=", 4))   { top = (int)wcstol(q + 4, NULL, 10); }
                else if (!wcsncmp(q, L"x=", 2))     { x = (int)wcstol(q + 2, NULL, 10); }
                else if (!wcsncmp(q, L"y=", 2))     { y = (int)wcstol(q + 2, NULL, 10); }
                else if (!wcsncmp(q, L"w=", 2))     { w = (int)wcstol(q + 2, NULL, 10); }
                else if (!wcsncmp(q, L"h=", 2))     { h2 = (int)wcstol(q + 2, NULL, 10); }
                else if (!wcsncmp(q, L"edge=", 5))  { edge = (int)wcstol(q + 5, NULL, 10); }
                else if (!wcsncmp(q, L"rstyle=", 7)){ rstyle = (int)wcstol(q + 7, NULL, 10); }
                else if (!wcsncmp(q, L"rx=", 3))    { rx = (int)wcstol(q + 3, NULL, 10); }
                else if (!wcsncmp(q, L"ry=", 3))    { ry = (int)wcstol(q + 3, NULL, 10); }
                else if (!wcsncmp(q, L"rw=", 3))    { rw = (int)wcstol(q + 3, NULL, 10); }
                else if (!wcsncmp(q, L"rh=", 3))    { rh = (int)wcstol(q + 3, NULL, 10); }
                while (*q && *q != L' ') q++;
                while (*q == L' ') q++;
            }
        }
        if (id <= 0 && !wcsncmp(p, L"ttauto=", 7))
            g_ttAuto = wcstol(p + 7, NULL, 10) ? 1 : 0;
        if (id > 0 && model_by_id(id) &&
            (NOTE_RESTORE_MAX <= 0 || slots < NOTE_RESTORE_MAX)) {
            note_open(id, style, color, 0);
            {
                NoteWnd *n = note_find(id);
                if (n) {
                    if (w <= 0) w = NOTE_DEF_W;
                    if (h2 <= 0) h2 = NOTE_DEF_H;
                    n->rc.left = x;
                    n->rc.top = y;
                    n->rc.right = x + w;
                    n->rc.bottom = y + h2;
                    n->topmost = top;
                    if (n->style == NOTE_STYLE_BOOKMARK) {
                        n->edge = edge;
                        n->restoreStyle = rstyle;
                        n->restoreRc.left = rx;
                        n->restoreRc.top = ry;
                        n->restoreRc.right = rx + (rw > 0 ? rw : MulDiv(NOTE_DEF_W, n->scale, 100));
                        n->restoreRc.bottom = ry + (rh > 0 ? rh : MulDiv(NOTE_DEF_H, n->scale, 100));
                        n->scale = 0;
                        note_repaint(n);
                    }
                    SetWindowPos(n->hwnd, top ? HWND_TOPMOST : HWND_NOTOPMOST,
                                 n->rc.left, n->rc.top,
                                 (n->rc.right - n->rc.left) + (n->style != NOTE_STYLE_SYSTEM ? MulDiv(NOTE_SHADOW, n->scale, 100) : 0),
                                 (n->rc.bottom - n->rc.top) + (n->style != NOTE_STYLE_SYSTEM ? MulDiv(NOTE_SHADOW, n->scale, 100) : 0),
                                 SWP_NOACTIVATE);
                    if (n->style == NOTE_STYLE_SYSTEM) {
                        RECT rc = { 0, 0, n->rc.right - n->rc.left, n->rc.bottom - n->rc.top };
                        AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
                        SetWindowPos(n->hwnd, NULL, n->rc.left, n->rc.top,
                                     rc.right - rc.left, rc.bottom - rc.top,
                                     SWP_NOZORDER | SWP_NOACTIVATE);
                    }
                    note_repaint(n);
                    slots++;
                }
            }
        }
        if (!nl) break;
        p = nl + 1;
    }
    free(buf);
}
