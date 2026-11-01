/* ============================================================================
 *  ZSchedule - ui_main.c
 *  主窗口：扁平工具条 / 倒计时面板 / 清单卡片 / 表格 / 周网格 三种视图
 *
 *  视觉：现代简约 —— 浅灰底、白卡片、圆角、单一强调色、大留白。
 *  布局：所有行高与控件尺寸都由字体度量推导，因此在任意 DPI 缩放下都不会
 *        出现文字被裁切的问题。
 * ==========================================================================*/
#include "zsched.h"
#include <windowsx.h>
#include <stdio.h>
#include <shlobj.h>            /* 选文件夹（SHBrowseForFolderW） */

#define MAX_ROWS 8192
#define CARD_PAD_X   16
#define CARD_PAD_Y   12

const COLORREF g_palette[8] = {
    RGB(59, 111, 224),   /* 蓝 */
    RGB(22, 163, 106),   /* 绿 */
    RGB(235, 148, 40),   /* 橙 */
    RGB(219, 74, 74),    /* 红 */
    RGB(124, 92, 224),   /* 紫 */
    RGB(13, 148, 168),   /* 青 */
    RGB(214, 51, 132),   /* 粉 */
    RGB(112, 122, 136)   /* 灰 */
};

HINSTANCE g_hInst;

static HWND g_hMain, g_hList, g_hTable, g_hWeek, g_hPanel, g_hStatus;
static HMENU g_mDataDir;               /* \u6587\u4ef6 \u2192 \u6570\u636e\u4fdd\u5b58\u4f4d\u7f6e \u5b50\u83dc\u5355 */
static HMENU g_mView;                  /* \u89c6\u56fe\u83dc\u5355\uff08\u6bcf\u6b21\u6253\u5f00\u65f6\u5237\u65b0\u4fbf\u7b3a\u76f8\u5173\u9879\uff09 */
static HMENU g_mFile;                  /* 文件菜单（用来刷新「后台运行」的勾选态） */
static HMENU g_mEdit;                  /* 编辑菜单（用来刷新「粘贴」的可用状态） */
static HWND g_hBoard;                 /* 便笺板（清单视图） */
static RECT g_pageRect;               /* 笔记本页面 */
static int  g_contentLeft;
static int  g_scalePct = 100;
static HWND g_btnNew, g_btnEdit, g_btnDel, g_btnCopy, g_btnPaste, g_btnNote;
static HWND g_btnList, g_btnTable, g_btnWeek;
static HWND g_btnPin, g_btnRefresh;

/* 便笺墙的“复制 / 粘贴”：只在程序内保存一份 Schedule 快照（不碰系统剪贴板） */
static Schedule g_clip;
static int      g_clipValid = 0;

static HFONT g_fUI, g_fSmall, g_fTiny, g_fTitle, g_fHead, g_fBig, g_fHuge;
static int   g_lhUI, g_lhSmall, g_lhTiny, g_lhTitle, g_lhHead, g_lhBig, g_lhHuge;
static HBRUSH g_brBg, g_brSurface, g_brSel, g_brLine, g_brAccent;
static HPEN   g_penLine;

static int    g_view = ZS_VIEW_LIST;
static int    g_sortMode = SORT_TIME;
static int    g_pinId = 0;
static int    g_order[MAX_ROWS];
static int    g_orderCount = 0;
static int    g_shown[MAX_ROWS];
static int    g_shownCount = 0;
static int    g_loading = 0;
static long   g_lastSec = -1;
static HWND   g_hRename = NULL;
static int    g_renameId = 0;
static HWND   g_hSearch = NULL;        /* 表格视图的搜索框 */
static wchar_t g_search[128];
static int    g_searchRows = -1;       /* 过滤后剩几行（-1 = 没在搜索） */
static int    g_background;            /* 1 = 主窗口收进托盘（不占任务栏） */
static int    g_rowH = 0;              /* 表格行高（由字体度量决定） */

/* 倒数日短文案：未来“还有 N 天”、过去“已过去 N 天”、当天“就是今天”。
   以前这里只写“已过去 N 天”，日期还没到就会显示成“已过去 0 天”。 */
static const wchar_t *anniv_short(const Schedule *s, wchar_t *buf, size_t cch)
{
    int fut = 0;
    int d = sched_anniv_days(s, &fut);
    WBuf b;
    wb_init(&b, buf, cch);
    if (d == 0) { wb_add(&b, L"就是今天"); return buf; }
    wb_add(&b, fut ? L"还有 " : L"已过去 ");
    wb_int(&b, d);
    wb_add(&b, L" 天");
    return buf;
}


/* ======================================================================
 *  小工具
 * ====================================================================*/
static int font_line(HFONT f)
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

static HFONT make_font(int pt, int weight, const wchar_t *face)
{
    HDC hdc = GetDC(NULL);
    int h = -MulDiv(pt, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    HFONT f;
    ReleaseDC(NULL, hdc);
    f = CreateFontW(h, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
                    OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_DONTCARE, face);
    return f;
}

static int systime_cmp(const SYSTEMTIME *a, const SYSTEMTIME *b)
{
    if (a->wYear != b->wYear)     return (a->wYear < b->wYear) ? -1 : 1;
    if (a->wMonth != b->wMonth)   return (a->wMonth < b->wMonth) ? -1 : 1;
    if (a->wDay != b->wDay)       return (a->wDay < b->wDay) ? -1 : 1;
    if (a->wHour != b->wHour)     return (a->wHour < b->wHour) ? -1 : 1;
    if (a->wMinute != b->wMinute) return (a->wMinute < b->wMinute) ? -1 : 1;
    if (a->wSecond != b->wSecond) return (a->wSecond < b->wSecond) ? -1 : 1;
    return 0;
}

static int is_active(const Schedule *s)
{
    return (s->enabled && !s->done && s->nextOcc > 0);
}

COLORREF palette_color(int idx)
{
    if (idx < 0 || idx > 7) return g_palette[0];
    return g_palette[idx];
}


static COLORREF tint(COLORREF c, int towardWhite /*0..10*/)
{
    int r = GetRValue(c), g = GetGValue(c), b = GetBValue(c);
    r = (r + 255 * towardWhite) / (towardWhite + 1);
    g = (g + 255 * towardWhite) / (towardWhite + 1);
    b = (b + 255 * towardWhite) / (towardWhite + 1);
    return RGB(r, g, b);
}

static void round_fill(HDC hdc, const RECT *r, int radius, COLORREF fill,
                       COLORREF border, int borderW)
{
    HBRUSH br = CreateSolidBrush(fill);
    HPEN pen;
    HBRUSH ob;
    HPEN op;
    pen = CreatePen(PS_SOLID, borderW, border);
    ob = (HBRUSH)SelectObject(hdc, br);
    op = (HPEN)SelectObject(hdc, pen);
    RoundRect(hdc, r->left, r->top, r->right, r->bottom, radius, radius);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(pen);
    DeleteObject(br);
}


/* ======================================================================
 *  扁平按钮（自绘，带悬停与按下态）
 * ====================================================================*/
typedef struct {
    int primary;      /* 实心强调色按钮 */
    int toggle;       /* 选中态样式（分段控件） */
    int checked;
    int hot, down;
    int disabled;
} FlatBtn;

static LRESULT CALLBACK flat_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    FlatBtn *fb = (FlatBtn *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_MOUSEMOVE:
        if (fb && !fb->hot) {
            TRACKMOUSEEVENT tme;
            fb->hot = 1;
            memset(&tme, 0, sizeof(tme));
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hwnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_MOUSELEAVE:
        if (fb) { fb->hot = 0; fb->down = 0; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;

    case WM_LBUTTONDOWN:
        if (fb && !fb->disabled) {
            fb->down = 1;
            SetCapture(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (fb) {
            int was = fb->down;
            RECT rc;
            POINT p;
            fb->down = 0;
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
            GetClientRect(hwnd, &rc);
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            if (was && PtInRect(&rc, p) && !fb->disabled) {
                HWND parent = GetParent(hwnd);
                int id = GetDlgCtrlID(hwnd);
                SendMessageW(parent, WM_COMMAND, MAKEWPARAM(id, 0), (LPARAM)hwnd);
            }
        }
        return 0;

    case WM_ENABLE:
        if (fb) { fb->disabled = !wp; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            wchar_t text[64];
            int len = GetWindowTextW(hwnd, text, 64);
            COLORREF fill, border, fg;
            GetClientRect(hwnd, &rc);
            if (!fb) {
                FillRect(hdc, &rc, g_brBg);
                EndPaint(hwnd, &ps);
                return 0;
            }
            /* 底色 */
            {
                HBRUSH b = CreateSolidBrush(TH_BG);
                FillRect(hdc, &rc, b);
                DeleteObject(b);
            }
            if (fb->disabled) {
                fill = TH_SURFACE_2; border = TH_LINE; fg = TH_TEXT_3;
            } else if (fb->toggle) {
                fill = fb->checked ? TH_SURFACE
                     : (fb->hot ? TH_HOVER : TH_BG);
                border = fb->checked ? TH_LINE : TH_LINE_SOFT;
                fg = fb->checked ? TH_ACCENT : TH_TEXT_2;
            } else if (fb->primary) {
                fill = fb->down ? tint(TH_ACCENT, 2) : (fb->hot ? tint(TH_ACCENT, 1) : TH_ACCENT);
                border = fill;
                fg = RGB(255, 255, 255);
            } else {
                fill = fb->down ? TH_PRESS : (fb->hot ? TH_HOVER : TH_BG);
                border = fb->hot ? TH_LINE : TH_BG;
                fg = TH_TEXT;
            }
            round_fill(hdc, &rc, TH_RADIUS, fill, border, 1);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, fg);
            SelectObject(hdc, g_fUI);
            rc.left += 8; rc.right -= 8;
            DrawTextW(hdc, text, len, &rc,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_NCDESTROY:
        if (fb) free(fb);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void flat_register_class(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = flat_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_HAND);
    wc.hbrBackground = NULL;
    wc.lpszClassName = FLATBTN_CLASS;
    RegisterClassExW(&wc);
}

HWND flat_create(HWND parent, const wchar_t *text, int id,
                        int primary, int toggle)
{
    FlatBtn *fb = (FlatBtn *)calloc(1, sizeof(FlatBtn));
    HWND h;
    if (!fb) return NULL;
    fb->primary = primary;
    fb->toggle = toggle;
    h = CreateWindowExW(0, FLATBTN_CLASS, text,
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
    if (!h) { free(fb); return NULL; }
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)fb);
    return h;
}

void flat_set_kind(HWND h, int primary, int toggle)
{
    FlatBtn *fb = (FlatBtn *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (fb) { fb->primary = primary; fb->toggle = toggle; InvalidateRect(h, NULL, FALSE); }
}

void flat_check(HWND h, int checked)
{
    FlatBtn *fb = (FlatBtn *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (fb) { fb->checked = checked; InvalidateRect(h, NULL, FALSE); }
}

/* 按钮理想宽度 = 文本宽 + 内边距 */
static int flat_width(HWND h, int pad)
{
    HDC hdc = GetDC(h);
    wchar_t text[64];
    SIZE sz;
    HFONT old;
    GetWindowTextW(h, text, 64);
    old = (HFONT)SelectObject(hdc, g_fUI);
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
    SelectObject(hdc, old);
    ReleaseDC(h, hdc);
    return sz.cx + pad;
}

/* ======================================================================
 *  排序
 * ====================================================================*/
static int g_curSortMode;
static int cmp_idx(const void *pa, const void *pb)
{
    int ia = *(const int *)pa, ib = *(const int *)pb;
    Schedule *a = model_at(ia), *b = model_at(ib);
    int ba, bb;
    if (!a || !b) return ia - ib;
    ba = is_active(a);
    bb = is_active(b);
    if (ba != bb) return bb - ba;
    {   /* 无时间待办不参与时间排序，统一放到最后 */
        int ta = (a->kind == KIND_TODO), tb = (b->kind == KIND_TODO);
        if (ta != tb) return ta - tb;
        if (ta && tb) {
            int r = wcscmp(a->title, b->title);
            return r ? r : (ia - ib);
        }
    }
    switch (g_curSortMode) {
    case SORT_TITLE: {
        int r = wcscmp(a->title, b->title);
        return r ? r : (ia - ib);
    }
    case SORT_START: {
        int r = systime_cmp(&a->start, &b->start);
        return r ? r : (ia - ib);
    }
    default:
        if (ba) {
            if (a->nextOcc != b->nextOcc) return (a->nextOcc < b->nextOcc) ? -1 : 1;
        } else {
            int r = systime_cmp(&a->start, &b->start);
            if (r) return -r;
        }
        return ia - ib;
    }
}

static void build_order(void)
{
    int i, n = model_count();
    if (n > MAX_ROWS) n = MAX_ROWS;
    for (i = 0; i < n; i++) g_order[i] = i;
    g_curSortMode = g_sortMode;
    if (n > 1) qsort(g_order, (size_t)n, sizeof(int), cmp_idx);
    g_orderCount = n;
}

static int row_id(int row)
{
    if (row < 0 || row >= g_shownCount) return 0;
    return g_shown[row];
}
static Schedule *row_item(int row)
{
    int id = row_id(row);
    int i = id ? model_index_by_id(id) : -1;
    return (i >= 0) ? model_at(i) : NULL;
}

/* 取当前选中的日程（清单视图支持多选） */
int main_pick_ids(int *out, int maxOut)
{
    int n = 0;
    int id = main_selected_id();
    (void)maxOut;
    if (id) out[n++] = id;
    return n;
}

/* 数据变更后统一刷新（视图 + 便笺 + 面板） */
void main_refresh_all(void)
{
    main_rebuild_views();
    note_refresh_all();
    InvalidateRect(g_hPanel, NULL, FALSE);
}

int main_selected_id(void)
{
    if (g_view == ZS_VIEW_LIST) {
        return stickies_sel(g_hBoard);
    }
    if (g_view == ZS_VIEW_TABLE) {
        int i = (int)SendMessageW(g_hTable, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
        LVITEMW lvi;
        if (i < 0) return 0;
        memset(&lvi, 0, sizeof(lvi));
        lvi.mask = LVIF_PARAM;
        lvi.iItem = i;
        if (!SendMessageW(g_hTable, LVM_GETITEMW, 0, (LPARAM)&lvi)) return 0;
        return (int)lvi.lParam;
    }
    return week_sel_id(g_hWeek);
}

void main_set_status(const wchar_t *msg)
{
    if (g_hStatus) SendMessageW(g_hStatus, SB_SETTEXTW, 0, (LPARAM)msg);
}

static void update_status_default(void)
{
    wchar_t buf[256];
    WBuf b;
    wb_init(&b, buf, 256);
    wb_add(&b, L"共 ");
    wb_int(&b, model_count());
    wb_add(&b, L" 条日程");
    {
        int i, active = 0;
        for (i = 0; i < model_count(); i++)
            if (is_active(model_at(i))) active++;
        wb_add(&b, L"　·　待办 ");
        wb_int(&b, active);
        wb_add(&b, L" 条");
    }
    if (g_pinId) {
        Schedule *s = model_by_id(g_pinId);
        if (s) { wb_add(&b, L"　·　倒计时目标："); wb_add(&b, s->title); }
    }
    if (g_view == ZS_VIEW_LIST) {
        int hid = stickies_hidden_count();
        wb_add(&b, L"　·　便笺可拖动、可缩放，照片可自由摆放，右键更多操作");
        if (hid) {
            wb_add(&b, L"　·　已收起 ");
            wb_int(&b, hid);
            wb_add(&b, L" 张");
        }
        wb_add(&b, L"　·　第 ");
        wb_int(&b, stickies_page_get(g_hBoard));
        wb_add(&b, L" / ");
        wb_int(&b, stickies_page_count(g_hBoard));
        wb_add(&b, L" 页");
        if (stickies_page_full(g_hBoard)) wb_add(&b, L"（本页已满，可点页脚「新开一页」）");
    }
    if (g_view == ZS_VIEW_TABLE && g_searchRows >= 0) {
        wb_add(&b, L"　·　搜索「");
        wb_add(&b, g_search);
        wb_add(&b, L"」：");
        wb_int(&b, g_searchRows);
        wb_add(&b, L" 条");
    }
    main_set_status(buf);
}

/* ======================================================================
 *  资源
 * ====================================================================*/
static void create_resources(void)
{
    g_fUI    = make_font(10, FW_NORMAL,   L"Microsoft YaHei UI");
    g_fSmall = make_font(9,  FW_NORMAL,   L"Microsoft YaHei UI");
    g_fTiny  = make_font(8,  FW_NORMAL,   L"Microsoft YaHei UI");
    g_fTitle = make_font(12, FW_SEMIBOLD, L"Microsoft YaHei UI");
    g_fHead  = make_font(9,  FW_SEMIBOLD, L"Microsoft YaHei UI");
    g_fBig   = make_font(20, FW_BOLD,     L"Microsoft YaHei UI");
    g_fHuge  = make_font(26, FW_BOLD,     L"Microsoft YaHei UI");

    g_lhUI = font_line(g_fUI);
    g_lhSmall = font_line(g_fSmall);
    g_lhTiny = font_line(g_fTiny);
    g_lhTitle = font_line(g_fTitle);
    g_lhHead = font_line(g_fHead);
    g_lhBig = font_line(g_fBig);
    g_lhHuge = font_line(g_fHuge);

    g_brBg = CreateSolidBrush(TH_BG);
    g_brSurface = CreateSolidBrush(TH_SURFACE);
    g_brSel = CreateSolidBrush(TH_SELECT);
    g_brLine = CreateSolidBrush(TH_LINE);
    g_brAccent = CreateSolidBrush(TH_ACCENT);
    g_penLine = CreatePen(PS_SOLID, 1, TH_LINE);
}

static void destroy_resources(void)
{
    if (g_fUI) DeleteObject(g_fUI);
    if (g_fSmall) DeleteObject(g_fSmall);
    if (g_fTiny) DeleteObject(g_fTiny);
    if (g_fTitle) DeleteObject(g_fTitle);
    if (g_fHead) DeleteObject(g_fHead);
    if (g_fBig) DeleteObject(g_fBig);
    if (g_fHuge) DeleteObject(g_fHuge);
    if (g_brBg) DeleteObject(g_brBg);
    if (g_brSurface) DeleteObject(g_brSurface);
    if (g_brSel) DeleteObject(g_brSel);
    if (g_brLine) DeleteObject(g_brLine);
    if (g_brAccent) DeleteObject(g_brAccent);
    if (g_penLine) DeleteObject(g_penLine);
}

/* ======================================================================
 *  倒计时面板
 * ====================================================================*/
static Schedule *find_nearest(void)
{
    int i, best = -1;
    time_t bt = 0;
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        if (!is_active(s)) continue;
        if (best < 0 || s->nextOcc < bt) { best = i; bt = s->nextOcc; }
    }
    return (best >= 0) ? model_at(best) : NULL;
}

/* 一个倒计时方块：返回占用高度 */
#define P_INK        RGB(56, 46, 38)     /* 书封深褐 */
#define P_INK2       RGB(74, 61, 50)
#define P_LINE       RGB(96, 80, 66)
#define P_TXT        RGB(246, 238, 224)
#define P_DIM        RGB(168, 152, 132)
#define P_NEAR       RGB(226, 168, 96)   /* 最近日程：琥珀 */
#define P_CHOSEN     RGB(148, 186, 214)  /* 自选日程：黛蓝 */

static int panel_tile(HWND hwnd, HDC mem, RECT tile, const wchar_t *label,
                      COLORREF accent, Schedule *s, const wchar_t *emptyHint,
                      int draw)
{
    int y = tile.top + 8;
    (void)hwnd;
    int bigW;
    RECT r;
    HFONT old;
    wchar_t buf[160];

    bigW = (tile.right - tile.left) / 2;

    if (draw) {
        old = (HFONT)SelectObject(mem, g_fTiny);
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, P_DIM);
        r = tile; r.left += 16; r.right = tile.right - bigW; r.top = y; r.bottom = y + g_lhTiny;
        DrawTextW(mem, label, -1, &r, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(mem, g_fTitle);
        SetTextColor(mem, s ? P_TXT : RGB(140, 124, 108));
        r.top = y + g_lhTiny + 3;
        r.bottom = r.top + g_lhTitle;
        DrawTextW(mem, s ? s->title : (emptyHint ? emptyHint : L"—"), -1, &r,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

        SelectObject(mem, g_fSmall);
        SetTextColor(mem, P_DIM);
        r.top = y + g_lhTiny + 3 + g_lhTitle + 3;
        r.bottom = r.top + g_lhSmall;
        if (s && s->kind == KIND_TODO) {
            DrawTextW(mem, L"无时间待办　·　只记标题与内容",
                      -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        } else if (s) {
            WBuf b;
            wb_init(&b, buf, 160);
            wb_add(&b, timet_text(s->nextOcc, buf + 100, 60));
            wb_add(&b, L"　·　");
            wb_add(&b, repeat_text(s, buf + 60, 40));
            DrawTextW(mem, buf, -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        }

        /* 大号倒计时 */
        {
            long long days;
            int hh, mi, ss, state;
            RECT rb = tile;
            rb.left = tile.right - bigW;
            rb.right = tile.right - 16;
            countdown_split(s ? s->nextOcc : 0, &days, &hh, &mi, &ss, &state);
            if (s && s->kind == KIND_ANNIV) {
                WBuf b;
                wchar_t tmp[160];
                SelectObject(mem, g_fHuge);
                SetTextColor(mem, accent);
                wb_init(&b, buf, 160);
                wb_add(&b, anniv_short(s, tmp, 160));
                {
                    RECT r2 = rb;
                    r2.top = tile.top + (tile.bottom - tile.top - g_lhHuge) / 2;
                    r2.bottom = r2.top + g_lhHuge;
                    DrawTextW(mem, buf, -1, &r2, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
                }
            } else if (s && state == 0) {
                WBuf b;
                wchar_t tmp[160];
                SelectObject(mem, g_fHuge);
                SetTextColor(mem, accent);
                wb_init(&b, buf, 160);
                wb_add(&b, cd_text(s, s->nextOcc, tmp, 160));
                {
                    RECT r2 = rb;
                    r2.top = tile.top + (tile.bottom - tile.top - g_lhHuge) / 2;
                    r2.bottom = r2.top + g_lhHuge;
                    DrawTextW(mem, buf, -1, &r2, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
                }
            } else {
                SelectObject(mem, g_fTitle);
                SetTextColor(mem, RGB(110, 120, 138));
                {
                    const wchar_t *t = !s ? L"—"
                                     : (s->kind == KIND_TODO
                                            ? (s->done ? L"已完成" : L"待办")
                                            : (state == 1 ? L"已过期"
                                                          : (s->kind == KIND_ANNIV ? L"纪念日"
                                                                                  : L"无后续安排")));
                    RECT r2 = rb;
                    r2.top = tile.top + (tile.bottom - tile.top - g_lhTitle) / 2;
                    r2.bottom = r2.top + g_lhTitle;
                    DrawTextW(mem, t, -1, &r2, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
                }
            }
        }
        SelectObject(mem, old);
    }
    return g_lhTiny + 3 + g_lhTitle + 3 + g_lhSmall + 10;
}

static void panel_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc, inner, tile1, tile2, sep;
    HDC mem;
    HBITMAP bmp, oldBmp;
    int w, h;
    Schedule *nearest, *chosen = NULL, *curSel = NULL;
    int selId;

    GetClientRect(hwnd, &rc);
    w = rc.right; h = rc.bottom;
    if (w <= 4 || h <= 4) { EndPaint(hwnd, &ps); return; }

    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, w, h);
    oldBmp = (HBITMAP)SelectObject(mem, bmp);

    /* 外层圆角深色卡片 */
    {
        RECT all = { 0, 0, w, h };
        HBRUSH b = CreateSolidBrush(TH_BG);
        FillRect(mem, &all, b);
        DeleteObject(b);
        inner = all;
        inner.right -= 1; inner.bottom -= 1;
        round_fill(mem, &inner, 14, P_INK, P_INK2, 1);
        {
            RECT in2 = inner;
            in2.left += 6; in2.top += 6; in2.right -= 6; in2.bottom -= 6;
            paper_fill(mem, &in2, P_INK, 0);
        }
    }

    nearest = find_nearest();
    selId = main_selected_id();
    {
        int id = g_pinId ? g_pinId : selId;
        if (id) curSel = model_by_id(id);
    }
    chosen = curSel;

    tile1.left = inner.left + 4;
    tile1.right = inner.left + (inner.right - inner.left) / 2 - 6;
    tile1.top = inner.top + 4;
    tile1.bottom = inner.bottom - 4;
    tile2.left = tile1.right + 12;
    tile2.right = inner.right - 4;
    tile2.top = tile1.top;
    tile2.bottom = tile1.bottom;

    {
        int th = panel_tile(hwnd, mem, tile1, L"最近日程", P_NEAR, nearest,
                            L"暂无即将开始的日程", 1);
        int th2 = panel_tile(hwnd, mem, tile2,
                             g_pinId ? L"自选日程 · 已固定" : L"自选日程 · 跟随选择",
                             P_CHOSEN, chosen,
                             L"选中一条日程后点「倒计时目标」", 1);
        (void)th; (void)th2;
    }
    /* 中缝 */
    sep.left = tile2.left - 6;
    sep.right = sep.left + 1;
    sep.top = inner.top + 14;
    sep.bottom = inner.bottom - 14;
    {
        HBRUSH b = CreateSolidBrush(P_LINE);
        FillRect(mem, &sep, b);
        DeleteObject(b);
    }

    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK panel_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        panel_paint(hwnd);
        return 0;
    case WM_LBUTTONDBLCLK:
        {
            int selId = main_selected_id();
            Schedule *s = selId ? model_by_id(selId) : NULL;
            if (s && s->link[0])
                ShellExecuteW(hwnd, L"open", s->link, NULL, NULL, SW_SHOWNORMAL);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ======================================================================
 *  清单卡片视图
 * ====================================================================*/
/* ======================================================================
 *  表格视图
 * ====================================================================*/
#define COL_TITLE   0
#define COL_START   1
#define COL_END     2
#define COL_REPEAT  3
#define COL_NEXT    4
#define COL_CDOWN   5
#define COL_ATT     6
#define COL_NOTE    7

static void table_create_columns(HWND h)
{
    struct { const wchar_t *t; int w; int fmt; } cols[] = {
        { L"标题",        240, LVCFMT_LEFT },
        { L"开始时间",    150, LVCFMT_LEFT },
        { L"结束时间",    150, LVCFMT_LEFT },
        { L"重复",        160, LVCFMT_LEFT },
        { L"下次发生",    150, LVCFMT_LEFT },
        { L"倒计时",      130, LVCFMT_LEFT },
        { L"附件",         56, LVCFMT_CENTER },
        { L"内容摘要",    220, LVCFMT_LEFT }
    };
    int i;
    LVCOLUMNW c;
    for (i = 0; i < (int)(sizeof(cols) / sizeof(cols[0])); i++) {
        memset(&c, 0, sizeof(c));
        c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT | LVCF_SUBITEM;
        c.pszText = (LPWSTR)cols[i].t;
        c.cx = cols[i].w;
        c.fmt = cols[i].fmt;
        c.iSubItem = i;
        SendMessageW(h, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
    }
}

/* 不区分大小写的子串查找（中文按原样比较） */
static int w_find_ci(const wchar_t *hay, const wchar_t *needle)
{
    size_t nl;
    if (!needle || !needle[0]) return 1;
    if (!hay) return 0;
    nl = wcslen(needle);
    for (; *hay; hay++) {
        size_t i;
        for (i = 0; i < nl; i++) {
            wchar_t a = hay[i], b = needle[i];
            if (!a) return 0;
            if (a >= L'A' && a <= L'Z') a = (wchar_t)(a - L'A' + L'a');
            if (b >= L'A' && b <= L'Z') b = (wchar_t)(b - L'A' + L'b');
            if (a != b) break;
        }
        if (i == nl) return 1;
    }
    return 0;
}

/* 搜索命中：标题 / 正文 / 链接（只在表格视图里过滤） */
static int table_match(const Schedule *s)
{
    if (!s || !g_search[0]) return 1;
    if (w_find_ci(s->title, g_search)) return 1;
    if (w_find_ci(s->note, g_search)) return 1;
    if (w_find_ci(s->link, g_search)) return 1;
    return 0;
}

static void table_refresh(void)
{
    int i, row = 0;
    LVITEMW it;
    wchar_t b1[128], b3[256];

    g_loading = 1;
    SendMessageW(g_hTable, WM_SETREDRAW, FALSE, 0);
    SendMessageW(g_hTable, LVM_DELETEALLITEMS, 0, 0);
    for (i = 0; i < g_orderCount; i++) {
        Schedule *s = model_at(g_order[i]);
        if (!s) continue;
        if (!table_match(s)) continue;          /* 搜索过滤：不命中的行不建 */
        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT | LVIF_PARAM | LVIF_STATE;
        it.iItem = row;
        it.iSubItem = 0;
        if (s->kind == KIND_ANNIV) {
            WBuf wb;
            wb_init(&wb, b3, 256);
            wb_add(&wb, L"◇ ");
            wb_add(&wb, s->title);
            it.pszText = b3;
        } else if (s->kind == KIND_TODO) {
            WBuf wb;
            wb_init(&wb, b3, 256);
            wb_add(&wb, s->done ? L"☑ " : L"☐ ");
            wb_add(&wb, s->title);
            it.pszText = b3;
        } else {
            it.pszText = s->title;
        }
        it.lParam = s->id;
        it.stateMask = LVIS_STATEIMAGEMASK;
        it.state = INDEXTOSTATEIMAGEMASK(s->done ? 2 : 1);
        SendMessageW(g_hTable, LVM_INSERTITEMW, 0, (LPARAM)&it);
        g_shown[row] = s->id;
        row++;

        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = row - 1; it.iSubItem = COL_START;
        it.pszText = (LPWSTR)(s->kind == KIND_TODO ? L"—" : dt_text(&s->start, b1, 128));
        SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);

        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = row - 1; it.iSubItem = COL_END;
        it.pszText = (LPWSTR)(s->kind == KIND_TODO ? L"—"
                          : (s->hasEnd ? dt_text(&s->end, b1, 128) : L"—"));
        SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);

        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = row - 1; it.iSubItem = COL_REPEAT;
        it.pszText = (LPWSTR)(s->kind == KIND_TODO ? L"—" : repeat_text(s, b1, 128));
        SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);

        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = row - 1; it.iSubItem = COL_NEXT;
        it.pszText = (LPWSTR)(s->kind == KIND_TODO ? L"—" : timet_text(s->nextOcc, b1, 128));
        SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);

        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = row - 1; it.iSubItem = COL_CDOWN;
        if (s->kind == KIND_TODO) {
            it.pszText = (LPWSTR)(s->done ? L"已完成" : L"待办");
        } else if (s->kind == KIND_ANNIV) {
            sched_anniv_text(s, b1, 128);   /* 未到来的日子显示“还有 N 天” */
            it.pszText = b1;
        }
        else if (s->done)     it.pszText = (LPWSTR)L"已完成";
        else if (!s->enabled) it.pszText = (LPWSTR)L"已暂停";
        else                  it.pszText = (LPWSTR)cd_text(s, s->nextOcc, b1, 128);
        SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);

        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = row - 1; it.iSubItem = COL_ATT;
        if (s->attachCount) {
            WBuf b;
            wb_init(&b, b3, 64);
            wb_int(&b, s->attachCount);
            wb_add(&b, L" 个");
            it.pszText = b3;
        } else {
            it.pszText = (LPWSTR)L"";
        }
        SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);

        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT; it.iItem = row - 1; it.iSubItem = COL_NOTE;
        {
            WBuf b;
            int k;
            wb_init(&b, b3, 256);
            for (k = 0; s->note[k] && (size_t)k < 200; k++) {
                wchar_t c = s->note[k];
                if (c == L'\r' || c == L'\n') break;
                wb_ch(&b, c);
            }
            if (s->link[0]) wb_add(&b, L"  [链接]");
            it.pszText = b3;
        }
        SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);
    }
    g_shownCount = row;
    g_searchRows = g_search[0] ? row : -1;
    SendMessageW(g_hTable, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g_hTable, NULL, TRUE);
    g_loading = 0;
}

/* ======================================================================
 *  视图刷新与节拍
 * ====================================================================*/
static int same_order(void)
{
    int i;
    if (g_view == ZS_VIEW_TABLE && g_search[0]) {
        /* 过滤状态下：期望 g_shown 正好是命中的那些 id（顺序一致） */
        int n = 0;
        for (i = 0; i < g_orderCount; i++) {
            Schedule *s = model_at(g_order[i]);
            if (!s || !table_match(s)) continue;
            if (n >= g_shownCount || g_shown[n] != s->id) return 0;
            n++;
        }
        return n == g_shownCount;
    }
    if (g_orderCount != g_shownCount) return 0;
    for (i = 0; i < g_orderCount; i++)
        if (g_order[i] != g_shown[i]) return 0;
    return 1;
}

void main_rebuild_views(void)
{
    int sel = main_selected_id();
    build_order();
    if (g_view == ZS_VIEW_LIST) {
        int i, n = 0;
        for (i = 0; i < g_orderCount; i++) {
            Schedule *sc = model_at(g_order[i]);
            if (!sc) continue;
            g_shown[n++] = sc->id;
        }
        g_shownCount = n;
        stickies_set_items(g_hBoard, g_shown, g_shownCount);
    } else if (g_view == ZS_VIEW_TABLE) {
        table_refresh();
    } else {
        week_refresh(g_hWeek);
    }
    if (sel) {
        int i;
        for (i = 0; i < g_shownCount; i++) {
            if (g_shown[i] == sel) {
                if (g_view == ZS_VIEW_LIST) {
                    stickies_set_sel(g_hBoard, sel);
                } else if (g_view == ZS_VIEW_TABLE) {
                    ListView_SetItemState(g_hTable, i, LVIS_SELECTED | LVIS_FOCUSED,
                                          LVIS_SELECTED | LVIS_FOCUSED);
                    ListView_EnsureVisible(g_hTable, i, FALSE);
                }
                break;
            }
        }
        if (g_view == ZS_VIEW_WEEK) week_set_sel(g_hWeek, sel);
    }
    update_status_default();
    InvalidateRect(g_hPanel, NULL, FALSE);
    note_refresh_all();
}

static void tick_update_texts(void)
{
    wchar_t b1[128];
    int i;
    if (g_view == ZS_VIEW_LIST) {
        stickies_refresh(g_hBoard);
    } else if (g_view == ZS_VIEW_TABLE) {
        LVITEMW it;
        int cnt = (int)SendMessageW(g_hTable, LVM_GETITEMCOUNT, 0, 0);
        memset(&it, 0, sizeof(it));
        it.mask = LVIF_TEXT;
        for (i = 0; i < cnt; i++) {
            Schedule *s = row_item(i);
            if (!s) continue;
            it.iItem = i;
            it.iSubItem = COL_NEXT;
            it.pszText = (LPWSTR)(s->kind == KIND_TODO ? L"—" : timet_text(s->nextOcc, b1, 128));
            SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);
            it.iSubItem = COL_CDOWN;
            if (s->kind == KIND_TODO) {
                it.pszText = (LPWSTR)(s->done ? L"已完成" : L"待办");
            }
            else if (s->kind == KIND_ANNIV) {
                sched_anniv_text(s, b1, 128);
                it.pszText = b1;
            }
            else if (s->done)     it.pszText = (LPWSTR)L"已完成";
            else if (!s->enabled) it.pszText = (LPWSTR)L"已暂停";
            else                  it.pszText = (LPWSTR)cd_text(s, s->nextOcc, b1, 128);
            SendMessageW(g_hTable, LVM_SETITEMW, 0, (LPARAM)&it);
        }
    } else {
        InvalidateRect(g_hWeek, NULL, FALSE);
    }
}

/* ======================================================================
 *  提醒：便笺弹窗 / 通知栏 / 提示音（可在编辑窗逐条选择）
 * ====================================================================*/
#define REMP_CLASS L"ZScheduleRemind"

typedef struct {
    int id;
    time_t anchor, next;
    int fired;
    int remind, lead, rep, times;      /* \u4e0a\u4e00\u6b21\u751f\u6548\u7684\u8bbe\u7f6e\uff1a\u6539\u4e86\u5c31\u91cd\u65b0\u8ba1\u65f6 */
} RemState;
static RemState g_rem[128];
static HWND     g_remPopup;
static int      g_trayAdded;
static NOTIFYICONDATAW g_tray;

static RemState *rem_find(int id, int create)
{
    int i, slot = -1;
    for (i = 0; i < 128; i++) {
        if (g_rem[i].id == id) return &g_rem[i];
        if (slot < 0 && g_rem[i].id == 0) slot = i;
    }
    if (!create || slot < 0) return NULL;
    g_rem[slot].id = id;
    g_rem[slot].fired = 0;
    g_rem[slot].next = 0;
    g_rem[slot].anchor = 0;
    g_rem[slot].remind = g_rem[slot].lead = g_rem[slot].rep = g_rem[slot].times = 0;
    return &g_rem[slot];
}

/* ---- 弹窗绘制：与独立便笺同色的便笺纸 ---- */
static void remp_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc, paper;
    Schedule *s = (Schedule *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    COLORREF col = s ? paper_shade(s->color, 62) : RGB(250, 235, 190);
    int scale, pad, y;
    wchar_t b[256], t[64];
    WBuf wb;

    GetClientRect(hwnd, &rc);
    {
        HDC h2 = GetDC(NULL);
        scale = MulDiv(GetDeviceCaps(h2, LOGPIXELSY), 100, 96);
        ReleaseDC(NULL, h2);
    }
    if (scale < 70) scale = 70;
    paper = rc;
    paper_fill(hdc, &paper, col, 0);
    {
        /* 顶部胶带 + 右下折角，和便笺一致 */
        RECT tape = rc;
        HBRUSH hb = CreateSolidBrush(RGB(247, 245, 238));
        HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(col, -22));
        HBRUSH ob = (HBRUSH)SelectObject(hdc, hb);
        HPEN op = (HPEN)SelectObject(hdc, pen);
        int w = rc.right - rc.left;
        tape.left = rc.left + w / 5;
        tape.right = tape.left + w * 3 / 5;
        tape.top = -MulDiv(4, scale, 100);
        tape.bottom = MulDiv(14, scale, 100);
        Rectangle(hdc, tape.left, tape.top, tape.right, tape.bottom);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(pen);
        DeleteObject(hb);
    }
    paper_fold_corner(hdc, &paper, col, MulDiv(22, scale, 100));
    {
        HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(col, -46));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right - 1, rc.bottom - 1);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
    SetBkMode(hdc, TRANSPARENT);
    pad = MulDiv(16, scale, 100);
    y = MulDiv(22, scale, 100);
    {
        RECT r = rc;
        HFONT f = CreateFontW(-MulDiv(11, GetDeviceCaps(hdc, LOGPIXELSY) * 130, 9600), 0, 0, 0,
                              FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                              L"Microsoft YaHei UI");
        HFONT old = (HFONT)SelectObject(hdc, f);
        r.left += pad;
        r.right -= pad;
        r.top = y;
        r.bottom = y + MulDiv(26, scale, 100);
        SetTextColor(hdc, RGB(58, 44, 30));
        DrawTextW(hdc, L"\u65e5\u7a0b\u63d0\u9192", -1, &r,
                  DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        y = r.bottom + MulDiv(4, scale, 100);
        SelectObject(hdc, old);
        DeleteObject(f);
    }
    {
        RECT r = rc;
        HFONT f = CreateFontW(-MulDiv(14, GetDeviceCaps(hdc, LOGPIXELSY) * 130, 9600), 0, 0, 0,
                              FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                              L"Microsoft YaHei UI");
        HFONT old = (HFONT)SelectObject(hdc, f);
        r.left += pad;
        r.right -= pad;
        r.top = y;
        r.bottom = y + MulDiv(30, scale, 100);
        SetTextColor(hdc, RGB(44, 34, 24));
        DrawTextW(hdc, s ? s->title : L"", -1, &r,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        y = r.bottom + MulDiv(4, scale, 100);
        SelectObject(hdc, old);
        DeleteObject(f);
    }
    {
        HFONT f = CreateFontW(-MulDiv(10, GetDeviceCaps(hdc, LOGPIXELSY) * 130, 9600), 0, 0, 0,
                              FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                              L"Microsoft YaHei UI");
        HFONT old = (HFONT)SelectObject(hdc, f);
        RECT r = rc;
        r.left += pad;
        r.right -= pad;
        r.top = y;
        r.bottom = y + MulDiv(20, scale, 100);
        SetTextColor(hdc, RGB(126, 108, 84));
        wb_init(&wb, b, 256);
        if (s) {
            wb_add(&wb, dt_text(&s->start, t, 64));
            wb_add(&wb, L"   ");
            wb_add(&wb, repeat_text(s, t, 64));
        }
        DrawTextW(hdc, b, -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        y = r.bottom + MulDiv(4, scale, 100);
        SelectObject(hdc, old);
        DeleteObject(f);
    }
    if (s && (s->remind & REM_SOUND) && s->sndMode == SND_FILE && s->sndFile[0]) {
        /* 自选铃声是“自动打开所选声音文件”（外部的播放器会弹出来），
           这里明确标注一行“这是铃声”，免得用户以为是别的东西在响 */
        HFONT f = CreateFontW(-MulDiv(10, GetDeviceCaps(hdc, LOGPIXELSY) * 130, 9600), 0, 0, 0,
                              FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                              L"Microsoft YaHei UI");
        HFONT old = (HFONT)SelectObject(hdc, f);
        RECT r = rc;
        r.left += pad;
        r.right -= pad;
        r.top = y;
        r.bottom = y + MulDiv(18, scale, 100);
        SetTextColor(hdc, RGB(150, 62, 54));
        wb_init(&wb, b, 256);
        wb_add(&wb, L"铃声：");
        wb_add(&wb, file_name_of(s->sndFile, t, 64));
        wb_add(&wb, L"（这是铃声）");
        DrawTextW(hdc, b, -1, &r, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        y = r.bottom + MulDiv(2, scale, 100);
        SelectObject(hdc, old);
        DeleteObject(f);
    }
    if (s && s->note[0]) {
        RECT r = rc;
        HFONT f = CreateFontW(-MulDiv(10, GetDeviceCaps(hdc, LOGPIXELSY) * 130, 9600), 0, 0, 0,
                              FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                              L"Microsoft YaHei UI");
        HFONT old = (HFONT)SelectObject(hdc, f);
        int k, n = 0;
        for (k = 0; s->note[k] && n < 160; k++) { b[n++] = s->note[k]; }
        b[n] = 0;
        r.left += pad;
        r.right -= pad;
        r.top = y;
        r.bottom = rc.bottom - MulDiv(30, scale, 100);
        SetTextColor(hdc, RGB(74, 62, 48));
        DrawTextW(hdc, b, -1, &r, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(hdc, old);
        DeleteObject(f);
    }
    {
        RECT r = rc;
        r.left += pad;
        r.right -= pad;
        r.bottom = rc.bottom - MulDiv(8, scale, 100);
        r.top = r.bottom - MulDiv(16, scale, 100);
        SetTextColor(hdc, RGB(142, 124, 98));
        DrawTextW(hdc, L"\u70b9\u4e00\u4e0b\u5173\u95ed\uff08\u7ea6 30 \u79d2\u540e\u81ea\u52a8\u5173\u95ed\uff09", -1, &r,
                  DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
    }
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK remp_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:      remp_paint(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:      if (wp == 1) DestroyWindow(hwnd); return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        if (g_remPopup == hwnd) {
            g_remPopup = NULL;
            snd_stop();              /* 点一下（或 30 秒自动关）就把铃声停掉 */
        }
        KillTimer(hwnd, 1);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void remp_register(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = remp_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_HAND);
    wc.hbrBackground = NULL;
    wc.lpszClassName = REMP_CLASS;
    RegisterClassExW(&wc);
}

/* “停止铃声”小窗稍后才定义：提醒弹窗要先把旧小窗收掉（不额外停声，交给后面统一处理） */
static void ring_close_quiet(void);

void remind_show_popup(const Schedule *s)
{
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(g_hMain, GWLP_HINSTANCE);
    int scale, w, h, sw, sh;
    RECT wa;
    HDC hdc;
    if (!s) return;
    /* 有大弹窗时就不需要“停止铃声”小窗了（点大弹窗一样能停） */
    ring_close_quiet();
    if (g_remPopup && IsWindow(g_remPopup)) DestroyWindow(g_remPopup);
    hdc = GetDC(NULL);
    scale = MulDiv(GetDeviceCaps(hdc, LOGPIXELSY), 100, 96);
    ReleaseDC(NULL, hdc);
    if (scale < 70) scale = 70;
    w = MulDiv(360, scale, 100);
    h = MulDiv(230, scale, 100);
    if (s && (s->remind & REM_SOUND) && s->sndMode == SND_FILE && s->sndFile[0])
        h += MulDiv(24, scale, 100);         /* 多一行“这是铃声” */
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    sw = wa.right - wa.left;
    sh = wa.bottom - wa.top;
    g_remPopup = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, REMP_CLASS,
                                 L"\u65e5\u7a0b\u63d0\u9192", WS_POPUP,
                                 wa.left + sw - w - MulDiv(24, scale, 100),
                                 wa.top + sh - h - MulDiv(24, scale, 100),
                                 w, h, NULL, NULL, hInst, NULL);
    if (!g_remPopup) return;
    SetWindowLongPtrW(g_remPopup, GWLP_USERDATA, (LONG_PTR)s);
    ShowWindow(g_remPopup, SW_SHOWNOACTIVATE);
    SetTimer(g_remPopup, 1, 30000, NULL);
    UpdateWindow(g_remPopup);
}

/* ---- 只勾了“提示音”时的小窗：给一个看得见、点得到的「停止铃声」----
 *  （有便笺弹窗时点弹窗就停，不需要这个小窗；
 *    小窗在声音播完后会自己关掉，不会一直挂着） */
#define RINGW_CLASS L"ZScheduleRingWnd"
#define RINGW_W     330
#define RINGW_H     124

typedef struct {
    wchar_t  title[MAX_TITLE];
    COLORREF color;
    HFONT    fTitle, fSmall;
} RingCtx;
static RingCtx g_ringctx;
static HWND    g_ringWnd;
static HWND    g_ringBtn;

static void ring_paint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc, r;
    int scale, pad, y;
    COLORREF col = g_ringctx.color ? g_ringctx.color : RGB(250, 235, 190);

    GetClientRect(hwnd, &rc);
    {
        HDC h2 = GetDC(NULL);
        scale = MulDiv(GetDeviceCaps(h2, LOGPIXELSY), 100, 96);
        ReleaseDC(NULL, h2);
    }
    if (scale < 70) scale = 70;
    paper_fill(hdc, &rc, col, 0);
    paper_fold_corner(hdc, &rc, col, MulDiv(18, scale, 100));
    {
        HPEN pen = CreatePen(PS_SOLID, 1, paper_shade(col, -46));
        HPEN op = (HPEN)SelectObject(hdc, pen);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Rectangle(hdc, rc.left, rc.top, rc.right - 1, rc.bottom - 1);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
    SetBkMode(hdc, TRANSPARENT);
    pad = MulDiv(14, scale, 100);
    y = MulDiv(12, scale, 100);
    {
        HFONT f = CreateFontW(-MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY) * 130, 9600), 0, 0, 0,
                              FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                              L"Microsoft YaHei UI");
        HFONT old = (HFONT)SelectObject(hdc, f);
        r = rc;
        r.left += pad;
        r.right -= pad;
        r.top = y;
        r.bottom = y + MulDiv(18, scale, 100);
        SetTextColor(hdc, RGB(126, 100, 60));
        DrawTextW(hdc, L"\u94c3\u58f0\u63d0\u9192 \u00b7 \u8fd9\u662f\u94c3\u58f0", -1, &r,
                  DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        y = r.bottom + MulDiv(2, scale, 100);
        SelectObject(hdc, old);
        DeleteObject(f);
    }
    {
        RECT ir = rc;
        HFONT f = CreateFontW(-MulDiv(11, GetDeviceCaps(hdc, LOGPIXELSY) * 130, 9600), 0, 0, 0,
                              FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_TT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                              L"Microsoft YaHei UI");
        HFONT old = (HFONT)SelectObject(hdc, f);
        ir.left += pad;
        ir.right -= pad;
        ir.top = y;
        ir.bottom = y + MulDiv(22, scale, 100);
        SetTextColor(hdc, RGB(44, 34, 24));
        DrawTextW(hdc, g_ringctx.title, -1, &ir,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(hdc, old);
        DeleteObject(f);
    }
    {
        RECT hr = rc;
        hr.left += pad;
        hr.right -= pad;
        hr.bottom = rc.bottom - MulDiv(6, scale, 100);
        hr.top = hr.bottom - MulDiv(14, scale, 100);
        SetTextColor(hdc, RGB(142, 124, 98));
        DrawTextW(hdc, L"\u70b9\u4e0b\u9762\u7684\u6309\u94ae\u505c\u6389\u94c3\u58f0\uff08\u653e\u5b8c\u4f1a\u81ea\u52a8\u5173\u95ed\uff09", -1, &hr,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK ring_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:      ring_paint(hwnd); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:      if (wp == 1 && !snd_is_active()) DestroyWindow(hwnd); return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == 1) { snd_stop(); DestroyWindow(hwnd); }
        return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        snd_stop();
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (g_ringWnd == hwnd) {
            g_ringWnd = NULL;
            g_ringBtn = NULL;
            snd_stop();                     /* 关了窗就停铃（和提醒弹窗一致） */
        }
        KillTimer(hwnd, 1);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ring_register(void)
{
    static int regd;
    WNDCLASSEXW wc;
    if (regd) return;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ring_proc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_HAND);
    wc.hbrBackground = NULL;
    wc.lpszClassName = RINGW_CLASS;
    RegisterClassExW(&wc);
    regd = 1;
}

static void ring_close_quiet(void)
{
    if (g_ringWnd && IsWindow(g_ringWnd)) {
        HWND h = g_ringWnd;
        g_ringWnd = NULL;            /* 先摘掉：WM_DESTROY 里就不会再 snd_stop */
        DestroyWindow(h);
    }
}

static void ring_stop_show(const Schedule *s)
{
    HINSTANCE hInst = g_hInst;
    int scale, w, h, sw, sh, bw, bh;
    RECT wa;
    HDC hdc;

    if (!s) return;
    ring_register();
    hdc = GetDC(NULL);
    scale = MulDiv(GetDeviceCaps(hdc, LOGPIXELSY), 100, 96);
    ReleaseDC(NULL, hdc);
    if (scale < 70) scale = 70;

    wcsncpy(g_ringctx.title, s->title, MAX_TITLE - 1);
    g_ringctx.title[MAX_TITLE - 1] = 0;
    g_ringctx.color = s->color;

    ring_close_quiet();          /* 旧窗先摘掉再销毁：别把“刚响起的新铃声”停掉 */

    w = MulDiv(RINGW_W, scale, 100);
    h = MulDiv(RINGW_H, scale, 100);
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    sw = wa.right - wa.left;
    sh = wa.bottom - wa.top;
    g_ringWnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, RINGW_CLASS,
                                L"\u94c3\u58f0\u63d0\u9192", WS_POPUP,
                                wa.left + sw - w - MulDiv(24, scale, 100),
                                wa.top + sh - h - MulDiv(24, scale, 100),
                                w, h, NULL, NULL, hInst, NULL);
    if (!g_ringWnd) return;
    /* 真按钮：看得见、点得到、Tab 也能选中 */
    bw = MulDiv(118, scale, 100);
    bh = MulDiv(28, scale, 100);
    g_ringBtn = CreateWindowExW(0, L"BUTTON", L"\u505c\u6b62\u94c3\u58f0",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                w - bw - MulDiv(14, scale, 100),
                                h - bh - MulDiv(26, scale, 100), bw, bh,
                                g_ringWnd, (HMENU)1, hInst, NULL);
    if (g_ringBtn) {
        SendMessageW(g_ringBtn, WM_SETFONT,
                     (WPARAM)(g_fSmall ? g_fSmall : (HFONT)GetStockObject(DEFAULT_GUI_FONT)),
                     TRUE);
    }
    ShowWindow(g_ringWnd, SW_SHOWNOACTIVATE);
    SetTimer(g_ringWnd, 1, 400, NULL);
    UpdateWindow(g_ringWnd);
}

/* ---- 通知栏（托盘气泡） ---- */
static void remind_toast(const Schedule *s)
{
    wchar_t buf[256];
    WBuf b;
    if (!s) return;
    memset(&g_tray, 0, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_hMain;
    g_tray.uID = 1;
    wcscpy(g_tray.szTip, APP_TITLE);
    if (!g_trayAdded) {
        g_tray.uFlags = NIF_ICON | NIF_TIP | NIF_INFO | NIF_MESSAGE;
        g_tray.hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL),
                                         MAKEINTRESOURCEW(APP_ICON_ID), IMAGE_ICON,
                                         GetSystemMetrics(SM_CXSMICON),
                                         GetSystemMetrics(SM_CYSMICON), 0);
        if (!g_tray.hIcon) g_tray.hIcon = LoadIconW(NULL, IDI_APPLICATION);
        g_tray.uCallbackMessage = WM_APP + 1;
        wcscpy(g_tray.szInfoTitle, APP_TITLE);
        wb_init(&b, buf, 256);
        wb_add(&b, s->title);
        wb_add(&b, L"   ");
        wb_add(&b, dt_text(&s->start, buf + 200, 40));
        wcscpy(g_tray.szInfo, buf);
        g_tray.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_ADD, &g_tray);
        g_trayAdded = 1;
    } else {
        g_tray.uFlags = NIF_INFO;
        wb_init(&b, buf, 256);
        wb_add(&b, s->title);
        wb_add(&b, L"   ");
        wb_add(&b, dt_text(&s->start, buf + 200, 40));
        wcscpy(g_tray.szInfo, buf);
        g_tray.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &g_tray);
    }
}

/* ---- 提示音来源：系统提示音 / 自选声音文件 ----
 * 自选文件不做解码播放，而是“自动打开所选声音文件”（交给系统默认播放器），
 * 所以 wav / mp3 / wma / m4a 都能响；提醒弹窗会写一行“这是铃声”说明它是铃声。 */
void snd_open_file(const wchar_t *path)
{
    if (!path || !path[0]) return;
    ShellExecuteW(g_hMain, L"open", path, NULL, NULL, SW_SHOWNORMAL);
}

const wchar_t *snd_desc(const Schedule *s, wchar_t *buf, size_t cch)
{
    WBuf b;
    wchar_t fn[MAX_PATH];
    wb_init(&b, buf, cch);
    if (s && s->sndMode == SND_FILE && s->sndFile[0])
        wb_add(&b, file_name_of(s->sndFile, fn, MAX_PATH));
    else
        wb_add(&b, L"系统提示音");
    return buf;
}

static void remind_fire(const Schedule *s)
{
    wchar_t buf[256];
    WBuf b;
    /* 顺序有讲究：先收掉上一张提醒窗（它会停掉上一轮铃声），再开新窗，最后播新铃声 */
    if (s->remind & REM_TOAST) remind_toast(s);
    if (s->remind & REM_POPUP) remind_show_popup(s);
    if (s->remind & REM_SOUND) {
        int played = 0;
        if (s->sndMode == SND_FILE && s->sndFile[0] &&
            GetFileAttributesW(s->sndFile) != INVALID_FILE_ATTRIBUTES) {
            /* 自选铃声：在本程序里直接播放（MF 支持 wav/mp3/flac；不行再兜底） */
            played = snd_play_file(s->sndFile) != 0;
        }
        if (!played) MessageBeep(MB_ICONEXCLAMATION);    /* 系统提示音（文件没了也退回这里） */
        /* 没有便笺弹窗可点的时候，给一个手动“停止铃声”的按钮 */
        if (played && !(s->remind & REM_POPUP)) ring_stop_show(s);
    }
    wb_init(&b, buf, 256);
    wb_add(&b, L"\u63d0\u9192\uff1a");
    wb_add(&b, s->title);
    main_set_status(buf);
}

void remind_tick(void)
{
    time_t now = time(NULL);
    int i;
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        RemState *rs;
        time_t fire;
        if (!s || !s->remind || !s->enabled || s->done) continue;
        if (s->nextOcc <= 0) continue;
        fire = s->nextOcc - (time_t)s->remLead * 60;
        rs = rem_find(s->id, 1);
        if (!rs) continue;
        if (rs->anchor != s->nextOcc || rs->remind != s->remind ||
            rs->lead != s->remLead || rs->rep != s->remRepeat ||
            rs->times != s->remTimes) {          /* \u76ee\u6807\u65f6\u95f4\u6216\u63d0\u9192\u8bbe\u7f6e\u53d8\u4e86\uff1a\u91cd\u65b0\u8ba1\u65f6 */
            rs->anchor = s->nextOcc;
            rs->remind = s->remind;
            rs->lead = s->remLead;
            rs->rep = s->remRepeat;
            rs->times = s->remTimes;
            rs->next = fire;
            rs->fired = 0;
        }
        if (now < rs->next) continue;
        if (rs->fired >= s->remTimes) continue;
        rs->fired++;
        if (s->remRepeat > 0 && rs->fired < s->remTimes)
            rs->next = rs->next + (time_t)s->remRepeat * 60;
        else
            rs->next = rs->next + (time_t)100 * 365 * 24 * 3600;   /* \u4e0d\u518d\u89e6\u53d1 */
        remind_fire(s);
    }
}

/* ======================================================================
 *  课表模式：这一节下课就关掉它的便笺，并在同一位置打开下一节
 * ====================================================================*/
/* 下课 → 换成下一节的延迟：用户要求“原课程下课后 5 秒”再把整个窗口换过去 */
#define TT_HANDOVER_SEC  5

typedef struct { int id, inSession; time_t handoverAt; } TtState;
static TtState g_tt[64];
static RECT    g_ttSlot;                 /* 上一节课便笺的位置（下一节贴在同一处） */
static int     g_ttSlotValid;

static TtState *tt_find(int id, int create)
{
    int i, slot = -1;
    for (i = 0; i < 64; i++) {
        if (g_tt[i].id == id) return &g_tt[i];
        if (slot < 0 && g_tt[i].id == 0) slot = i;
    }
    if (!create || slot < 0) return NULL;
    memset(&g_tt[slot], 0, sizeof(g_tt[slot]));
    g_tt[slot].id = id;
    return &g_tt[slot];
}

/* 在原位置开一节新课的便笺：优先用它自己上次的纸色/外观，否则沿用上一节那张 */
static void tt_open_class(int id, int style, int color)
{
    int st = style, co = color;
    TtState *ts;
    note_look_remembered(id, &st, &co);
    note_open_at(id, st, co, g_ttSlotValid ? &g_ttSlot : NULL, 0);
    ts = tt_find(id, 1);
    if (ts) ts->inSession = 1;
}

/* 有没有“下课后等 5 秒再换窗”还没到点的 */
static int tt_handover_pending(time_t now)
{
    int i;
    for (i = 0; i < 64; i++)
        if (g_tt[i].id > 0 && g_tt[i].handoverAt > 0 && now < g_tt[i].handoverAt)
            return 1;
    return 0;
}

void tt_tick(void)
{
    int i;
    time_t now = time(NULL);
    if (!note_tt_auto_get()) return;

    /* ① 每节课：该开就开；下课只记下“5 秒后交接”，先不关窗 */
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        TtState *ts;
        time_t st = 0, en = 0;
        int have, in;
        if (!s || !s->timetable) continue;
        have = sched_class_window(s, &st, &en);
        in = (have && now >= st && now < en) ? 1 : 0;
        ts = tt_find(s->id, 1);
        if (!ts) continue;
        if (in && !ts->inSession) {                     /* 上课 */
            if (tt_handover_pending(now)) continue;     /* 上一节还没交接完，先别抢位置 */
            if (!note_is_open(s->id)) tt_open_class(s->id, NOTE_STYLE_PAPER, 0);
            ts->inSession = 1;
            ts->handoverAt = 0;
        } else if (!in && ts->inSession) {              /* 下课 */
            ts->inSession = 0;
            /* 用户要的时间点：原课程下课后 5 秒，再把整个窗口换成下一节 */
            ts->handoverAt = (en > 0) ? en + TT_HANDOVER_SEC : now + TT_HANDOVER_SEC;
        }
    }

    /* ② 到期交接：关掉这一节、在原位置开下一节（连堂则下一节已经在上） */
    for (i = 0; i < 64; i++) {
        TtState *ts = &g_tt[i];
        int id;
        int style = NOTE_STYLE_PAPER, color = 0;
        RECT rc;
        Schedule *nxt;
        time_t nst = 0, nen = 0;
        int nextStarted = 0;

        if (ts->id <= 0 || ts->handoverAt <= 0 || now < ts->handoverAt) continue;
        id = ts->id;
        ts->handoverAt = 0;

        if (note_is_open(id)) {
            if (note_get_geom(id, &style, &color, &rc)) {
                g_ttSlot = rc;                          /* 记住这一节的位置 */
                g_ttSlotValid = 1;
            }
            note_close_one(id);
        }
        /* 下一节：先看现在正在上的是不是另一节（连堂），否则取今天之后最近的一节 */
        nxt = sched_class_now();
        if (nxt && nxt->id == id) nxt = NULL;
        if (!nxt) nxt = sched_class_next(now - TT_HANDOVER_SEC, &nst, &nen);
        if (nxt && nxt->id == id) nxt = NULL;
        if (nxt) {
            nextStarted = sched_class_window(nxt, &nst, &nen) && now >= nst && now < nen;
            tt_open_class(nxt->id, style, color);       /* 内部会把 inSession 置 1 */
            if (!nextStarted) {
                TtState *nts = tt_find(nxt->id, 0);
                if (nts) nts->inSession = 0;            /* 还没到上课时间：等它开始了再算 */
            }
        }
        note_save_state();
    }
}

/* ======================================================================
 *  开机自启动（HKCU\Software\Microsoft\Windows\CurrentVersion\Run）
 * ====================================================================*/
static const wchar_t *kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t *kRunVal = L"Jian";

int app_autostart_get(void)
{
    HKEY k;
    int on = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, kRunVal, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) on = 1;
        RegCloseKey(k);
    }
    return on;
}

void app_autostart_set(int on)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &k, NULL) != ERROR_SUCCESS) return;
    if (on) {
        wchar_t path[MAX_PATH + 4];
        GetModuleFileNameW(NULL, path + 1, MAX_PATH);
        path[0] = L'"';
        wcscat(path, L"\"");
        RegSetValueExW(k, kRunVal, 0, REG_SZ, (const BYTE *)path,
                       (DWORD)((wcslen(path) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, kRunVal);
    }
    RegCloseKey(k);
}

void main_tick(void)
{
    long nowsec = (long)time(NULL);
    if (nowsec == g_lastSec) return;
    g_lastSec = nowsec;

    model_refresh_next();
    remind_tick();
    tt_tick();
    build_order();
    if (g_view != ZS_VIEW_WEEK && !same_order()) {
        main_rebuild_views();
    } else {
        tick_update_texts();
        update_status_default();
    }
    InvalidateRect(g_hPanel, NULL, FALSE);
    note_refresh_all();      /* \u4fbf\u7b3a\u4e0a\u7684\u5012\u8ba1\u65f6/\u4e0b\u4e00\u8282\u8bfe\u540d\u79f0\u8981\u6bcf\u79d2\u8ddf\u7740\u8d70 */
}

/* ======================================================================
 *  操作
 * ====================================================================*/
static void schedule_default(Schedule *s)
{
    SYSTEMTIME st;
    time_t t, base;
    struct tm tmnow, *pt;
    int mins;

    memset(s, 0, sizeof(*s));
    s->enabled = 1;
    s->repInterval = 1;
    s->repKind = REP_NONE;
    s->repUnit = UNIT_DAY;
    s->ttNext = 1;                    /* 课表便笺默认把“下一节课”名称显示出来 */
    s->color = g_palette[0];

    t = time(NULL);
    pt = localtime(&t);
    if (pt) {
        tmnow = *pt;
        mins = tmnow.tm_min;
        base = t - (time_t)mins * 60 - (time_t)tmnow.tm_sec + 3600;
        if (mins > 0) base += 3600;
        pt = localtime(&base);
    }
    if (pt) {
        st.wYear = (WORD)(pt->tm_year + 1900);
        st.wMonth = (WORD)(pt->tm_mon + 1);
        st.wDay = (WORD)(pt->tm_mday);
        st.wHour = (WORD)(pt->tm_hour);
    } else {
        GetLocalTime(&st);
    }
    st.wMinute = 0; st.wSecond = 0; st.wMilliseconds = 0;
    s->start = st;
    s->end = st;
}

static void do_new_at(const SYSTEMTIME *pref)
{
    Schedule s;
    schedule_default(&s);
    if (pref) s.start = *pref;
    wcscpy(s.title, L"新建日程");
    if (edit_show_at(g_hMain, &s, pref) == 1) {
        model_add(&s);
        model_save(model_path());
        main_rebuild_views();
        main_set_status(L"已新建日程");
    }
}

static void do_edit(int id)
{
    Schedule *s = model_by_id(id);
    Schedule *tmp;
    int r;
    if (!s) {
        MessageBoxW(g_hMain, L"请先选择一条日程。", L"提示", MB_ICONINFORMATION);
        return;
    }
    tmp = (Schedule *)malloc(sizeof(Schedule));
    if (!tmp) return;
    *tmp = *s;
    r = edit_show(g_hMain, tmp, 0);
    if (r == 1) {
        *s = *tmp;
        model_normalize(s);
        model_save(model_path());
        main_rebuild_views();
        main_set_status(L"已保存修改");
    } else if (r == -1) {
        model_remove(id);
        if (g_pinId == id) g_pinId = 0;
        model_save(model_path());
        main_rebuild_views();
        main_set_status(L"已删除日程");
    }
    free(tmp);
}

static void do_delete(int id)
{
    Schedule *s = model_by_id(id);
    wchar_t msg[256];
    WBuf b;
    if (!s) {
        MessageBoxW(g_hMain, L"请先选择一条日程。", L"提示", MB_ICONINFORMATION);
        return;
    }
    wb_init(&b, msg, 256);
    wb_add(&b, L"确定删除「");
    wb_add(&b, s->title);
    wb_add(&b, L"」吗？");
    if (MessageBoxW(g_hMain, msg, L"删除确认",
                    MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDYES) {
        model_remove(id);
        if (g_pinId == id) g_pinId = 0;
        model_save(model_path());
        main_rebuild_views();
        main_set_status(L"已删除日程");
    }
}

static void set_view(int v);   /* 定义在后面 */

/* 便笺墙：把选中的便笺复制到程序内剪贴板（不立刻新建，等「粘贴」） */
static void do_copy(int id)
{
    Schedule *s = model_by_id(id);
    if (!s) {
        main_set_status(L"先选中一张便笺再复制");
        return;
    }
    g_clip = *s;
    g_clipValid = 1;
    if (g_btnPaste) EnableWindow(g_btnPaste, TRUE);
    main_set_status(L"已复制便笺：可先「新开一页」，再点「粘贴」");
}

/* 粘贴：在当前页新建一张便笺（点过「新开一页」就落在新页） */
static void do_paste(void)
{
    Schedule tmp;
    Schedule *ns;
    int nid;
    if (!g_clipValid) {
        main_set_status(L"还没有复制便笺：先选一张便笺点「复制」");
        return;
    }
    tmp = g_clip;
    tmp.id = 0;                        /* model_add 分配新 id 并重新规范化 */
    ns = model_add(&tmp);
    if (!ns) {
        MessageBoxW(g_hMain, L"粘贴失败：内存不足", L"粘贴", MB_ICONWARNING);
        return;
    }
    nid = ns->id;
    model_save(model_path());
    if (g_view != ZS_VIEW_LIST)
        set_view(ZS_VIEW_LIST);        /* 切到便笺墙；新便笺落在当前页 */
    else
        main_rebuild_views();
    stickies_set_sel(g_hBoard, nid);   /* 选中刚粘贴的便笺 */
    stickies_save_pos();               /* 把新便笺的页归属立刻落盘 */
    main_set_status(L"已粘贴便笺（在当前页）");
}

static void do_toggle_done(int id)
{
    Schedule *s = model_by_id(id);
    if (!s) return;
    s->done = !s->done;
    model_save(model_path());
    main_rebuild_views();
    main_set_status(s->done ? L"已标记为完成" : L"已恢复为未完成");
}

static void do_pin(int id)
{
    if (!id) {
        MessageBoxW(g_hMain, L"请先选择一条日程。", L"提示", MB_ICONINFORMATION);
        return;
    }
    if (g_pinId == id) {
        g_pinId = 0;
        main_set_status(L"已取消固定倒计时");
    } else {
        Schedule *s = model_by_id(id);
        wchar_t buf[256];
        WBuf b;
        g_pinId = id;
        wb_init(&b, buf, 256);
        wb_add(&b, L"倒计时已固定到：");
        if (s) wb_add(&b, s->title);
        main_set_status(buf);
    }
    update_status_default();
    InvalidateRect(g_hPanel, NULL, FALSE);
}

/* 打开便笺：单条或批量 */
static void do_note(int id, int batchOfSelection)
{
    int ids[24];
    int n = 0;
    int style = NOTE_STYLE_PAPER, color = 0;
    if (batchOfSelection) {
        n = main_pick_ids(ids, 24);
    } else if (id) {
        ids[n++] = id;
    }
    if (n <= 0) {
        MessageBoxW(g_hMain, L"请先选择一条或多条日程。", L"提示", MB_ICONINFORMATION);
        return;
    }
    /* 沿用已打开便笺的外观设置，保持一致 */
    if (note_is_open(ids[0])) note_get_look(ids[0], &style, &color);
    if (color < 0 || color >= NOTE_COLOR_COUNT) color = 0;
    {
        int i;
        for (i = 0; i < n; i++) {
            if (note_is_open(ids[i])) note_refresh_all();
            else note_open(ids[i], style, color, i == 0);
        }
    }
    {
        wchar_t buf[64];
        WBuf b;
        wb_init(&b, buf, 64);
        if (n == 1) wb_add(&b, L"已打开便笺");
        else { wb_add(&b, L"已打开 "); wb_int(&b, n); wb_add(&b, L" 张便笺"); }
        main_set_status(buf);
    }
}

static void do_note_all(void)
{
    /* v1.6：便笺窗口不再有数量上限，这里也按实际条数分配（以前写死 24 张） */
    int *ids;
    int n = 0, i, total = model_count();
    if (total <= 0) {
        MessageBoxW(g_hMain, L"当前没有待办日程。", L"提示", MB_ICONINFORMATION);
        return;
    }
    ids = (int *)malloc((size_t)total * sizeof(int));
    if (!ids) return;
    for (i = 0; i < total; i++) {
        Schedule *s = model_at(i);
        if (!s) continue;
        if (s->kind == KIND_ANNIV) { ids[n++] = s->id; continue; }
        if (s->enabled && !s->done) ids[n++] = s->id;
    }
    if (n == 0) {
        free(ids);
        MessageBoxW(g_hMain, L"当前没有待办日程。", L"提示", MB_ICONINFORMATION);
        return;
    }
    note_open_batch(ids, n, NOTE_STYLE_PAPER, 0);
    free(ids);
    main_set_status(L"已为全部待办打开便笺");
}

static void do_open_attach(int id, int index)
{
    Schedule *s = model_by_id(id);
    if (!s || s->attachCount == 0) {
        MessageBoxW(g_hMain, L"该日程没有附件。", L"提示", MB_ICONINFORMATION);
        return;
    }
    if (index < 0 || index >= s->attachCount) index = 0;
    if (GetFileAttributesW(s->attach[index]) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(g_hMain, L"附件文件不存在或已被移动。", L"提示", MB_ICONWARNING);
        return;
    }
    ShellExecuteW(g_hMain, L"open", s->attach[index], NULL, NULL, SW_SHOWNORMAL);
}

/* 用内置查看器打开该日程的所有图片 */
static void do_view_images(int id)
{
    Schedule *s = model_by_id(id);
    const wchar_t *imgs[MAX_ATTACH];
    int n = 0, i, start = 0;
    if (!s || s->attachCount == 0) {
        MessageBoxW(g_hMain, L"该日程没有附件。", L"提示", MB_ICONINFORMATION);
        return;
    }
    for (i = 0; i < s->attachCount; i++) {
        if (img_is_image(s->attach[i])) imgs[n++] = s->attach[i];
    }
    if (n == 0) {
        MessageBoxW(g_hMain, L"该日程没有图片附件。", L"提示", MB_ICONINFORMATION);
        return;
    }
    iv_open(g_hMain, imgs, n, start);
}

/* ======================================================================
 *  表格内联重命名
 * ====================================================================*/
static LRESULT CALLBACK rename_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                    UINT_PTR ref, DWORD_PTR data)
{
    (void)ref;
    switch (msg) {
    case WM_KEYDOWN:
        if (wp == VK_RETURN) {
            wchar_t buf[MAX_TITLE];
            GetWindowTextW(hwnd, buf, MAX_TITLE);
            g_renameId = (int)data;
            g_hRename = NULL;
            DestroyWindow(hwnd);
            if (g_renameId) {
                Schedule *s = model_by_id(g_renameId);
                if (s && buf[0] && wcscmp(buf, s->title)) {
                    wcsncpy(s->title, buf, MAX_TITLE - 1);
                    s->title[MAX_TITLE - 1] = 0;
                    model_save(model_path());
                }
                main_rebuild_views();
                main_set_status(L"标题已更新");
            }
            return 0;
        }
        if (wp == VK_ESCAPE) {
            g_hRename = NULL;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        if (g_hRename == hwnd) {
            wchar_t buf[MAX_TITLE];
            GetWindowTextW(hwnd, buf, MAX_TITLE);
            g_renameId = (int)data;
            g_hRename = NULL;
            DestroyWindow(hwnd);
            if (g_renameId) {
                Schedule *s = model_by_id(g_renameId);
                if (s && buf[0] && wcscmp(buf, s->title)) {
                    wcsncpy(s->title, buf, MAX_TITLE - 1);
                    s->title[MAX_TITLE - 1] = 0;
                    model_save(model_path());
                }
                main_rebuild_views();
                main_set_status(L"标题已更新");
            }
        }
        return 0;
    case WM_DESTROY:
        if (g_hRename == hwnd) g_hRename = NULL;
        return 0;
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

static void begin_rename(void)
{
    int row, id;
    Schedule *s;
    RECT rc;
    HWND h;

    if (g_view == ZS_VIEW_WEEK) return;
    if (g_view == ZS_VIEW_TABLE) {
        row = (int)SendMessageW(g_hTable, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
        if (row < 0) return;
        id = row_id(row);
        s = model_by_id(id);
        if (!s) return;
        if (g_hRename) { DestroyWindow(g_hRename); g_hRename = NULL; }
        rc.left = LVIR_LABEL; rc.top = 0; rc.right = 0; rc.bottom = 0;
        if (!SendMessageW(g_hTable, LVM_GETSUBITEMRECT, (WPARAM)row, (LPARAM)&rc)) return;
        {
            POINT p;
            p.x = rc.left; p.y = rc.top;
            MapWindowPoints(g_hTable, g_hMain, &p, 1);
            h = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", s->title,
                                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP,
                                p.x, p.y, rc.right - rc.left + 6, rc.bottom - rc.top + 4,
                                g_hMain, NULL, g_hInst, NULL);
            if (!h) return;
            SendMessageW(h, WM_SETFONT, (WPARAM)g_fUI, TRUE);
            SetWindowSubclass(h, rename_proc, 1, (DWORD_PTR)id);
            g_hRename = h;
            SetFocus(h);
            SendMessageW(h, EM_SETSEL, 0, -1);
        }
    }
}

/* ======================================================================
 *  右键菜单
 * ====================================================================*/
/* 标记 / 取消课表（单条） */
static void do_toggle_timetable(int id)
{
    Schedule *x = model_by_id(id);
    if (!x) return;
    x->timetable = x->timetable ? 0 : 1;
    model_normalize(x);
    model_save(model_path());
    note_refresh_all();
    if (x->timetable) {
        time_t st = 0, en = 0, now = time(NULL);
        if (sched_class_window(x, &st, &en) && now >= st && now < en) {
            note_open(x->id, NOTE_STYLE_PAPER, 0, 1);
            main_set_status(L"已标记为课表（正在上课，便笺已打开）");
        } else {
            main_set_status(L"已标记为课表：到点自动开关便笺、内容跟随上下课");
        }
    } else {
        main_set_status(L"已取消课表标记");
    }
}

static void do_toggle_timetable_sel(void)
{
    int ids[24];
    int k = main_pick_ids(ids, 24), j;
    if (k < 1 && main_selected_id()) { ids[0] = main_selected_id(); k = 1; }
    if (k < 1) { main_set_status(L"请先选中要标成课表的日程"); return; }
    for (j = 0; j < k; j++) do_toggle_timetable(ids[j]);
    if (k > 1) main_set_status(L"已切换所选日程的课表标记");
}

/* 设置倒计时显示精度（单条） */
static void do_set_cd_unit(int id, int unit)
{
    static const wchar_t *nm[4] = { L"天", L"时", L"分", L"秒" };
    Schedule *x = model_by_id(id);
    wchar_t buf[96];
    WBuf b;
    if (!x) return;
    if (unit < CDU_DAY || unit > CDU_SEC) unit = CDU_DEF;
    x->cdUnit = unit;
    model_save(model_path());
    wb_init(&b, buf, 96);
    wb_add(&b, L"倒计时显示到：");
    wb_add(&b, unit == CDU_DEF ? L"秒（默认）" : nm[unit - 1]);
    main_set_status(buf);
}

/* 批量 / 全局：没有选中时就把全部日程都设成这个精度 */
static void do_set_cd_unit_many(int id, int unit)
{
    int ids[24];
    int k = main_pick_ids(ids, 24), j;
    if (k < 1 && id) { ids[0] = id; k = 1; }
    if (k < 1) {
        int i;
        for (i = 0; i < model_count(); i++) {
            Schedule *x = model_at(i);
            if (x) x->cdUnit = unit;
        }
        model_save(model_path());
        main_set_status(L"全部日程的倒计时显示精度已更新");
        return;
    }
    for (j = 0; j < k; j++) {
        Schedule *x = model_by_id(ids[j]);
        if (x) x->cdUnit = unit;
    }
    model_save(model_path());
    main_set_status(k > 1 ? L"选中日程的倒计时显示精度已更新"
                          : L"倒计时显示精度已更新");
}

static void show_context_menu(int id, POINT pt)
{
    HMENU m = CreatePopupMenu();
    Schedule *s = id ? model_by_id(id) : NULL;
    int cmd;
    int hasImg = 0, i;
    if (s) {
        for (i = 0; i < s->attachCount; i++)
            if (img_is_image(s->attach[i])) { hasImg = 1; break; }
    }
    AppendMenuW(m, MF_STRING, IDM_EDIT_EDIT, L"编辑(&E)…\tEnter");
    AppendMenuW(m, MF_STRING, IDM_EDIT_RENAME, L"重命名(&R)\tF2");
    AppendMenuW(m, MF_STRING, IDM_EDIT_COPY, L"复制便笺(&C)\tCtrl+C");
    AppendMenuW(m, MF_STRING | (g_clipValid ? 0 : MF_GRAYED),
                IDM_EDIT_PASTE, L"粘贴便笺(&P)\tCtrl+V");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING | (s && s->done ? MF_CHECKED : 0), IDM_EDIT_DONE,
                L"标记为已完成\t空格");
    AppendMenuW(m, MF_STRING | (s && s->enabled ? MF_CHECKED : 0), IDM_EDIT_ENABLE,
                L"启用（参与倒计时）");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_NOTE_ONE, L"打开便笺(&N)\tCtrl+B");
    AppendMenuW(m, MF_STRING, IDM_NOTE_BATCH, L"批量打开便笺(&B)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    {   /* 无时间待办 + 便笺图片（列表、表格、周表都能用） */
        Schedule *x = id ? model_by_id(id) : NULL;
        wchar_t ip[MAX_PATH];
        int hasImg = x && sched_first_image(x, ip, MAX_PATH) != NULL;
        HMENU mi = CreatePopupMenu();
        static const int pcts[4] = { 40, 55, 75, 100 };
        int k;
        if (x) {
            AppendMenuW(m, MF_STRING | (x->kind == KIND_TODO ? MF_CHECKED : 0),
                        IDM_EDIT_TODO, L"无时间待办（不设时间）(&T)");
            AppendMenuW(m, MF_STRING | (x->timetable ? MF_CHECKED : 0), IDM_BOARD_TT,
                        L"课表模式（到点自动开关便笺）(&L)");
            {   /* 倒计时显示精度（这条日程） */
                static const wchar_t *dun2[4] = { L"天", L"时", L"分", L"秒" };
                HMENU mcd2 = CreatePopupMenu();
                int dk2;
                for (dk2 = 0; dk2 < 4; dk2++) {
                    wchar_t lb2[64];
                    WBuf bb2;
                    wb_init(&bb2, lb2, 64);
                    wb_add(&bb2, L"显示到 ");
                    wb_add(&bb2, dun2[dk2]);
                    AppendMenuW(mcd2, MF_STRING | (cd_unit_of(x) == dk2 + 1 ? MF_CHECKED : 0),
                                IDM_CDU_DAY + dk2, lb2);
                }
                AppendMenuW(m, MF_POPUP, (UINT_PTR)mcd2, L"倒计时显示精度(&U)");
            }
            AppendMenuW(m, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                        (x->showImg ? MF_CHECKED : 0), IDM_NOTE_SHOWIMG,
                        L"便笺上直接显示图片(&I)");
        }
        for (k = 0; k < 4; k++) {
            wchar_t lb[48];
            WBuf b;
            wb_init(&b, lb, 48);
            wb_int(&b, pcts[k]);
            wb_add(&b, L"%  ");
            wb_add(&b, k == 0 ? L"小" : (k == 1 ? L"中"
                                    : (k == 2 ? L"大" : L"整张")));
            AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                        (x && x->imgW == pcts[k] * 10 ? MF_CHECKED : 0),
                        3510 + k, lb);
        }
        for (k = 0; k < 3; k++) {
            static const wchar_t *pm[3] = { L"浮动（可自由摆放）(&F)",
                                            L"随文（文中）(&I)",
                                            L"文末（正文之后）(&E)" };
            AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                        (x && x->imgMode == k ? MF_CHECKED : 0), 3520 + k, pm[k]);
        }
        AppendMenuW(mi, MF_SEPARATOR, 0, NULL);
        AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                    (x && x->imgLayer == IMGL_OVER ? MF_CHECKED : 0), 3530,
                    L"浮于文字上方(&O)");
        AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED) |
                    (x && x->imgLayer == IMGL_UNDER ? MF_CHECKED : 0), 3531,
                    L"衬于文字下方(&U)");
        AppendMenuW(mi, MF_SEPARATOR, 0, NULL);
        AppendMenuW(mi, MF_STRING | (hasImg ? 0 : MF_GRAYED), IDM_BOARD_IMGRST,
                    L"位置与大小复位(&R)");
        if (x)
            AppendMenuW(m, MF_POPUP | (hasImg ? 0 : MF_GRAYED), (UINT_PTR)mi,
                        L"照片（可拖动、可缩放）(&Z)");
        else
            DestroyMenu(mi);
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    }
    if (g_view == ZS_VIEW_LIST) {
        int pinned = id ? stickies_is_pinned(id) : 0;
        AppendMenuW(m, MF_STRING | (pinned ? MF_CHECKED : 0), IDM_BOARD_PIN,
                    pinned ? L"取消固定（可拖动）(&F)\tP" : L"固定在当前位置(&F)\tP");
        AppendMenuW(m, MF_STRING, IDM_BOARD_AUTO, L"恢复自动排列(&A)");
        AppendMenuW(m, MF_STRING, IDM_BOARD_HIDE, L"收起这张便笺(&H)\tH");
        if (id && stickies_has_size(id))
            AppendMenuW(m, MF_STRING, IDM_BOARD_SIZE, L"恢复默认大小(&S)");
        AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    }
    AppendMenuW(m, MF_STRING, IDM_VIEW_PIN, L"设为倒计时目标(&P)");
    if (g_pinId) AppendMenuW(m, MF_STRING, IDM_VIEW_UNPIN, L"取消固定倒计时");
    if (hasImg)  AppendMenuW(m, MF_STRING, IDM_EDIT_VIEWIMG, L"查看原图(&V)");
    if (s && s->attachCount)
        AppendMenuW(m, MF_STRING, IDM_FILE_DIR, L"用系统程序打开首个附件(&O)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    {   /* 翻页：把这张便笺挪到别的页（多页时才出现） */
        int pc = stickies_page_count(g_hBoard);
        int pg = stickies_page_of(id);
        if (pc > 1) {
            wchar_t lb[64];
            WBuf b;
            wb_init(&b, lb, 64);
            wb_add(&b, L"这张便笺在第 ");
            wb_int(&b, pg);
            wb_add(&b, L" 页");
            AppendMenuW(m, MF_STRING | MF_DISABLED | MF_GRAYED, 0, lb);
            AppendMenuW(m, MF_STRING | (pg > 1 ? 0 : MF_GRAYED), IDM_PAGE_MOVEPREV,
                        L"移到上一页");
            AppendMenuW(m, MF_STRING | (pg < pc ? 0 : MF_GRAYED), IDM_PAGE_MOVENEXT,
                        L"移到下一页");
        }
    }
    AppendMenuW(m, MF_STRING, IDM_EDIT_DEL, L"删除(&D)\tDelete");

    cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, g_hMain, NULL);
    DestroyMenu(m);

    switch (cmd) {
    case IDM_EDIT_EDIT:   do_edit(id); break;
    case IDM_EDIT_RENAME: begin_rename(); break;
    case IDM_EDIT_COPY:   do_copy(id); break;
    case IDM_EDIT_PASTE:  do_paste(); break;
    case IDM_EDIT_DONE:   do_toggle_done(id); break;
    case IDM_EDIT_VIEWIMG: do_view_images(id); break;
    case IDM_NOTE_ONE:    do_note(id, 0); break;
    case IDM_NOTE_BATCH:  do_note(0, 1); break;
    case IDM_BOARD_PIN:
        stickies_set_pinned(id, stickies_is_pinned(id) ? 0 : 1);
        main_set_status(stickies_is_pinned(id) ? L"便笺已固定（不再随意拖动）"
                                               : L"便笺已取消固定");
        break;
    case IDM_BOARD_AUTO:
        stickies_reset_pos(id);
        main_set_status(L"已恢复自动排列");
        break;
    case IDM_BOARD_TT:      do_toggle_timetable(id); break;
    case IDM_CDU_DAY: case IDM_CDU_HOUR: case IDM_CDU_MIN: case IDM_CDU_SEC:
        do_set_cd_unit(id, cmd - IDM_CDU_DAY + 1);
        break;
    case IDM_BOARD_SIZE:
        stickies_reset_size(id);
        main_set_status(L"便笺已恢复默认大小");
        break;
    case IDM_EDIT_TODO:
        {
            Schedule *x = model_by_id(id);
            if (x) {
                x->kind = (x->kind == KIND_TODO) ? KIND_TASK : KIND_TODO;
                model_normalize(x);
                model_save(model_path());
                model_refresh_next();
                main_rebuild_views();
                note_refresh_all();
                main_set_status(x->kind == KIND_TODO
                                ? L"已设为无时间待办"
                                : L"已恢复为普通日程");
            }
        }
        break;
    case 3510: case 3511: case 3512: case 3513:
        {
            static const int pcts2[4] = { 40, 55, 75, 100 };
            Schedule *x = model_by_id(id);
            if (x) {
                wchar_t buf[80];
                WBuf b;
                int nimg = sched_image_count(x), q;
                if (nimg > MAX_IMGS) nimg = MAX_IMGS;
                if (nimg < 1) nimg = 1;
                for (q = 0; q < nimg; q++) {
                    x->imgB[q][0] = 0;
                    x->imgB[q][1] = q * (1000 / nimg);
                    x->imgB[q][2] = pcts2[cmd - 3510] * 10;
                    x->imgB[q][3] = (pcts2[cmd - 3510] * 10) / nimg;
                    if (x->imgB[q][3] < 100) x->imgB[q][3] = 100;
                }
                x->imgMode = IMGM_FLOAT;
                x->imgPct = 0;
                x->showImg = 1;
                model_save(model_path());
                main_rebuild_views();
                note_refresh_all();
                wb_init(&b, buf, 80);
                wb_add(&b, L"\u56fe\u7247\u5360\u4fbf\u7b3a ");
                wb_int(&b, pcts2[cmd - 3510]);
                wb_add(&b, L"%");
                main_set_status(buf);
            }
        }
        break;
    case 3520: case 3521: case 3522:
        {
            Schedule *x = model_by_id(id);
            if (x) {
                int q;
                x->imgMode = cmd - 3520;
                model_save(model_path());
                main_rebuild_views();
                note_refresh_all();
                for (q = 0; q < MAX_IMGS; q++)
                    if (x->imgB[q][2] > 0) {
                        /* 换版式后原来的自由位置不再合适：清掉自定义框，改成自动堆叠 */
                        x->imgB[q][0] = x->imgB[q][1] = x->imgB[q][2] = x->imgB[q][3] = 0;
                    }
                model_save(model_path());
                main_rebuild_views();
                note_refresh_all();
                main_set_status(cmd == 3520 ? L"照片：浮动（可自由摆放）"
                               : (cmd == 3521 ? L"照片：随文（排在正文里）"
                                              : L"照片：文末（排在正文之后）"));
            }
        }
        break;
    case 3530: case 3531:
        {
            Schedule *x = model_by_id(id);
            if (x) {
                x->imgLayer = (cmd == 3531) ? IMGL_UNDER : IMGL_OVER;
                model_save(model_path());
                main_rebuild_views();
                note_refresh_all();
                main_set_status(cmd == 3531 ? L"照片衬于文字下方"
                                            : L"照片浮于文字上方");
            }
        }
        break;
    case IDM_BOARD_IMGRST:
        {
            Schedule *x = model_by_id(id);
            if (x) {
                int q;
                x->imgX = x->imgY = x->imgW = x->imgH = 0;
                x->imgPct = 0;
                for (q = 0; q < MAX_IMGS; q++)
                    x->imgB[q][0] = x->imgB[q][1] = x->imgB[q][2] = x->imgB[q][3] = 0;
                model_save(model_path());
                main_rebuild_views();
                note_refresh_all();
                main_set_status(L"照片位置与大小已复位");
            }
        }
        break;
    case IDM_BOARD_HIDE:
        stickies_set_hidden(id, 1);
        main_set_status(L"已收起这张便笺（视图菜单可摆回来）");
        update_status_default();
        break;
    case IDM_BOARD_SHOWALL:
        stickies_show_all();
        main_set_status(L"已摆回所有便笺");
        update_status_default();
        break;
    case IDM_BOARD_HIDEALL:
        {
            int alloff = (stickies_hidden_count() > 0);
            stickies_hide_all(alloff ? 0 : 1);
            main_set_status(alloff ? L"已摆回所有便笺"
                                   : L"已把便笺全部收起来（视图菜单可摆回来）");
            update_status_default();
        }
        break;
    case IDM_NOTE_SHOWIMG:
        {
            Schedule *x = model_by_id(id);
            if (x) {
                x->showImg = x->showImg ? 0 : 1;
                model_save(model_path());
                main_rebuild_views();
                note_refresh_all();
                main_set_status(x->showImg ? L"便笺已改为直接显示图片"
                                           : L"便笺只显示附件数量");
            }
        }
        break;
    case IDM_EDIT_ENABLE:
        {
            Schedule *x = model_by_id(id);
            if (x) {
                x->enabled = !x->enabled;
                model_save(model_path());
                main_rebuild_views();
            }
        }
        break;
    case IDM_VIEW_PIN:    do_pin(id); break;
    case IDM_VIEW_UNPIN:  g_pinId = 0; update_status_default();
                          InvalidateRect(g_hPanel, NULL, FALSE); break;
    case IDM_FILE_DIR:    do_open_attach(id, 0); break;
    case IDM_EDIT_DEL:    do_delete(id); break;
    }
}

/* ======================================================================
 *  布局
 * ====================================================================*/
static void layout(HWND hwnd)
{
    RECT rc;
    int cw, viewY, viewH, w, bh, by, x, barH;
    HWND btns[10];
    int i;

    GetClientRect(hwnd, &rc);
    cw = rc.right;
    if (g_hStatus) SendMessageW(g_hStatus, WM_SIZE, 0, 0);
    {
        HDC hdc = GetDC(hwnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hwnd, hdc);
        g_scalePct = MulDiv(dpi, 100, 96);
        if (g_scalePct < 70) g_scalePct = 70;
        paper_notebook(NULL, &rc, g_scalePct, &g_pageRect, &g_contentLeft);
    }

    /* 工具条：贴着页面顶部 */
    bh = g_lhUI + 12;
    by = g_pageRect.top + MulDiv(12, g_scalePct, 100);
    barH = by + bh + MulDiv(8, g_scalePct, 100);
    {
        btns[0] = g_btnNew;   btns[1] = g_btnEdit;  btns[2] = g_btnDel;
        btns[3] = g_btnCopy;  btns[4] = g_btnPaste; btns[5] = g_btnNote;
        btns[6] = g_btnList;  btns[7] = g_btnTable; btns[8] = g_btnWeek;
        btns[9] = g_btnPin;
        x = g_contentLeft + MulDiv(10, g_scalePct, 100);
        for (i = 0; i < 10; i++) {
            int wd;
            if (!btns[i]) continue;
            wd = flat_width(btns[i], 28);
            if (i == 0) wd += 12;                      /* 主按钮稍宽 */
            if (i == 6) x += 14;                       /* 视图分段前留白 */
            SetWindowPos(btns[i], NULL, x, by, wd, bh, SWP_NOZORDER | SWP_NOACTIVATE);
            x += wd + ((i >= 6 && i <= 7) ? 4 : 8);    /* 分段内间距更小 */
        }
        if (g_btnRefresh) {
            w = flat_width(g_btnRefresh, 28);
            if (x + 18 + w < cw - 12) x += 18;
            SetWindowPos(g_btnRefresh, NULL, x, by, w, bh,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }
    viewY = barH;

    /* 倒计时面板：贴在页面里 */
    {
        int ph = g_lhTiny + 3 + g_lhTitle + 3 + g_lhSmall + 22;
        int px = g_contentLeft + MulDiv(10, g_scalePct, 100);
        int pw = g_pageRect.right - px - MulDiv(20, g_scalePct, 100);
        if (pw < 240) pw = 240;
        SetWindowPos(g_hPanel, NULL, px, viewY + TH_GAP, pw, ph,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        viewY += ph + TH_GAP;
    }

    /* 内容区：页面内、装订区右侧 */
    {
        int vx = g_contentLeft + MulDiv(4, g_scalePct, 100);
        int vw = g_pageRect.right - vx - MulDiv(14, g_scalePct, 100);
        int vBottom = g_pageRect.bottom - MulDiv(10, g_scalePct, 100);
        if (vw < 240) vw = 240;
        if (g_view == ZS_VIEW_TABLE && g_hSearch) {
            /* 表格视图：顶部一条搜索栏（右对齐），表格让出这一条 */
            int bar = MulDiv(32, g_scalePct, 100);
            int sw = MulDiv(320, g_scalePct, 100);
            int sx = vx + vw - sw;
            if (sx < vx) { sx = vx; sw = vw; }
            SetWindowPos(g_hSearch, NULL, sx, viewY + MulDiv(2, g_scalePct, 100),
                         sw, bar - MulDiv(6, g_scalePct, 100),
                         SWP_NOZORDER | SWP_NOACTIVATE);
            viewY += bar;
        }
        viewH = vBottom - viewY;
        if (viewH < 80) viewH = 80;
        SetWindowPos(g_hBoard, NULL, vx, viewY, vw, viewH, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(g_hTable, NULL, vx, viewY, vw, viewH, SWP_NOZORDER | SWP_NOACTIVATE);
        SetWindowPos(g_hWeek, NULL, vx, viewY, vw, viewH, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    InvalidateRect(hwnd, NULL, TRUE);
}

static void set_view(int v);   /* 定义在后面 */

static void do_search_focus(void)
{
    if (!g_hSearch) return;
    if (g_view != ZS_VIEW_TABLE) set_view(ZS_VIEW_TABLE);
    ShowWindow(g_hSearch, SW_SHOW);
    SetFocus(g_hSearch);
    SendMessageW(g_hSearch, EM_SETSEL, 0, -1);
    main_set_status(L"输入关键词即可筛选表格；Esc 清空，Ctrl+F 回到搜索框");
}

/* ======================================================================
 *  托盘与后台运行（v1.6）：主窗口可以收进托盘，不占任务栏
 * ====================================================================*/
static void tray_remove(void);

static void tray_add(void)
{
    if (g_trayAdded) return;
    memset(&g_tray, 0, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_hMain;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_tray.uCallbackMessage = WM_APP + 1;
    g_tray.hIcon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(APP_ICON_ID),
                                     IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                     GetSystemMetrics(SM_CYSMICON), 0);
    if (!g_tray.hIcon) g_tray.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wcscpy(g_tray.szTip, APP_TITLE);
    if (Shell_NotifyIconW(NIM_ADD, &g_tray)) g_trayAdded = 1;
}

static void tray_remove(void)
{
    if (!g_trayAdded) return;
    Shell_NotifyIconW(NIM_DELETE, &g_tray);
    g_trayAdded = 0;
}

static void set_background(int on);

static void tray_menu(void)
{
    HMENU m = CreatePopupMenu();
    POINT p;
    int cmd;
    AppendMenuW(m, MF_STRING, IDM_TRAY_SHOW, L"显示主窗口(&S)");
    AppendMenuW(m, MF_STRING | (g_background ? MF_CHECKED : 0), IDM_TRAY_BACK,
                L"后台运行（不占任务栏）(&B)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_TRAY_NOTES, L"打开全部便笺(&N)");
    AppendMenuW(m, MF_STRING | (snd_is_active() ? 0 : MF_GRAYED), IDM_TRAY_STOPSND,
                L"停止铃声(&P)");
    AppendMenuW(m, MF_SEPARATOR, 0, NULL);
    AppendMenuW(m, MF_STRING, IDM_TRAY_EXIT, L"退出(&X)");
    GetCursorPos(&p);
    SetForegroundWindow(g_hMain);
    cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, g_hMain, NULL);
    DestroyMenu(m);
    switch (cmd) {
    case IDM_TRAY_SHOW:  set_background(0); break;
    case IDM_TRAY_BACK:  set_background(!g_background); break;
    case IDM_TRAY_NOTES: do_note_all(); break;
    case IDM_TRAY_STOPSND:
        snd_stop();
        main_set_status(L"已停止铃声");
        break;
    case IDM_TRAY_EXIT:  PostMessageW(g_hMain, WM_CLOSE, 0, 0); break;
    }
}

static void set_background(int on)
{
    g_background = on ? 1 : 0;
    if (g_background) {
        ShowWindow(g_hMain, SW_HIDE);          /* 隐藏后任务栏按钮自然消失 */
        if (g_trayAdded) {                     /* 第一次提示一下 */
            g_tray.uFlags = NIF_INFO;
            wcscpy(g_tray.szInfoTitle, APP_TITLE);
            wcscpy(g_tray.szInfo, L"笺正在后台运行：点托盘图标回来，右键可打开全部便笺");
            g_tray.dwInfoFlags = NIIF_INFO;
            Shell_NotifyIconW(NIM_MODIFY, &g_tray);
        }
    } else {
        ShowWindow(g_hMain, SW_SHOW);
        SetForegroundWindow(g_hMain);
        main_set_status(L"已回到前台");
    }
}

/* ======================================================================
 *  主窗口过程
 * ====================================================================*/
static void set_view(int v)
{
    g_view = v;
    ShowWindow(g_hList, v == ZS_VIEW_LIST ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hTable, v == ZS_VIEW_TABLE ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hWeek, v == ZS_VIEW_WEEK ? SW_SHOW : SW_HIDE);
    if (g_hSearch) ShowWindow(g_hSearch, v == ZS_VIEW_TABLE ? SW_SHOW : SW_HIDE);
    if (g_hMain) layout(g_hMain);
    flat_check(g_btnList, v == ZS_VIEW_LIST);
    flat_check(g_btnTable, v == ZS_VIEW_TABLE);
    flat_check(g_btnWeek, v == ZS_VIEW_WEEK);
    main_rebuild_views();
}

static void refresh_from_disk(void)
{
    model_free();
    model_init();
    if (!model_load(model_path())) {
        model_seed_sample();
        model_save(model_path());
    } else if (model_loaded_legacy()) {
        model_save(model_path());
    }
    g_pinId = 0;
    main_rebuild_views();
}

/* ======================================================================
 *  数据保存位置：统一目录 + 自定义路径 + 导入旧数据
 * ====================================================================*/
#define PICK_CLASS L"ZSchedulePickDataWnd"

static void path_join(wchar_t *out, size_t cch, const wchar_t *dir, const wchar_t *name)
{
    size_t n;
    wcsncpy(out, dir ? dir : L"", cch - 1);
    out[cch - 1] = 0;
    n = wcslen(out);
    if (n && out[n - 1] != L'\\' && out[n - 1] != L'/')
        wcsncat(out, L"\\", cch - wcslen(out) - 1);
    wcsncat(out, name, cch - wcslen(out) - 1);
}

static int path_same_dir(const wchar_t *a, const wchar_t *b)
{
    wchar_t x[MAX_PATH], y[MAX_PATH];
    size_t n;
    if (!a || !b) return 0;
    wcsncpy(x, a, MAX_PATH - 1); x[MAX_PATH - 1] = 0;
    wcsncpy(y, b, MAX_PATH - 1); y[MAX_PATH - 1] = 0;
    n = wcslen(x); if (n && (x[n - 1] == L'\\' || x[n - 1] == L'/')) x[n - 1] = 0;
    n = wcslen(y); if (n && (y[n - 1] == L'\\' || y[n - 1] == L'/')) y[n - 1] = 0;
    return _wcsicmp(x, y) == 0;
}

static const wchar_t *ft_text(long long ft, wchar_t *buf, size_t cch)
{
    FILETIME f, loc;
    SYSTEMTIME st;
    WBuf b;
    f.dwLowDateTime = (DWORD)(ft & 0xFFFFFFFFu);
    f.dwHighDateTime = (DWORD)((ft >> 32) & 0xFFFFFFFFu);
    wb_init(&b, buf, cch);
    if (ft > 0 && FileTimeToLocalFileTime(&f, &loc) && FileTimeToSystemTime(&loc, &st)) {
        wb_int(&b, st.wYear); wb_ch(&b, L'-');
        wb_u2(&b, st.wMonth); wb_ch(&b, L'-');
        wb_u2(&b, st.wDay);   wb_ch(&b, L' ');
        wb_u2(&b, st.wHour);  wb_ch(&b, L':');
        wb_u2(&b, st.wMinute);
    } else {
        wb_add(&b, L"—");
    }
    return buf;
}

/* 子菜单里那行“当前：…” */
static void data_dir_label_update(void)
{
    wchar_t buf[600];
    WBuf b;
    const wchar_t *d = app_data_dir();
    if (!g_mDataDir) return;
    wb_init(&b, buf, 600);
    wb_add(&b, L"当前：");
    wb_add(&b, (d && d[0]) ? d : L"—");
    if (wcslen(buf) > 96) { buf[93] = 0; wcscat(buf, L"..."); }
    ModifyMenuW(g_mDataDir, IDM_FILE_DIRSHOW,
                MF_BYCOMMAND | MF_STRING | MF_DISABLED | MF_GRAYED,
                IDM_FILE_DIRSHOW, buf);
}

static void center_over_main(HWND hwnd, int w, int h)
{
    RECT ro, wa;
    int x, y;
    if (!g_hMain || !GetWindowRect(g_hMain, &ro)) {
        ro.left = 0; ro.top = 0; ro.right = 1024; ro.bottom = 768;
    }
    x = ro.left + ((ro.right - ro.left) - w) / 2;
    y = ro.top + ((ro.bottom - ro.top) - h) / 2;
    wa.left = 0; wa.top = 0; wa.right = 1920; wa.bottom = 1080;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (x + w > wa.right)  x = wa.right - w;
    if (y + h > wa.bottom) y = wa.bottom - h;
    if (x < wa.left) x = wa.left;
    if (y < wa.top)  y = wa.top;
    SetWindowPos(hwnd, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

static int CALLBACK browse_cb(HWND h, UINT msg, LPARAM lp, LPARAM data)
{
    (void)lp;
    if (msg == BFFM_INITIALIZED && data)
        SendMessageW(h, BFFM_SETSELECTIONW, TRUE, data);
    return 0;
}

static int pick_folder(const wchar_t *title, const wchar_t *init, wchar_t *out, size_t cch)
{
    BROWSEINFOW bi;
    LPITEMIDLIST pidl;
    int ok = 0;
    (void)cch;
    out[0] = 0;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = g_hMain;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
    bi.lpfn = browse_cb;
    bi.lParam = (LPARAM)init;
    pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        if (!SHGetPathFromIDListW(pidl, out)) out[0] = 0;
        CoTaskMemFree(pidl);
        if (out[0]) ok = 1;
    }
    return ok;
}

/* 切到新的数据目录：先让旧目录的便笺状态落盘，再换路径、重新载入一切 */
static void data_switch(const wchar_t *dir, int migrate, const wchar_t *why)
{
    wchar_t buf[700];
    WBuf b;
    note_close_all();                 /* 旧目录里的便笺状态先写盘 */
    if (!app_data_dir_set(dir, migrate)) {
        main_set_status(L"切换数据位置失败：无法创建或写入该文件夹");
        return;
    }
    note_path_forget();
    stickies_path_forget();
    app_data_reload();
    note_load_state();
    main_rebuild_views();
    note_refresh_all();
    SendMessageW(g_hStatus, SB_SETTEXTW, 1, (LPARAM)model_path());
    wb_init(&b, buf, 700);
    wb_add(&b, why ? why : L"数据位置已切换：");
    wb_add(&b, app_data_dir());
    wb_add(&b, L"（");
    wb_int(&b, model_count());
    wb_add(&b, L" 条日程）");
    main_set_status(buf);
}

/* 列出候选让用户挑一份 */
static const AppDataCand *g_pickCand;
static int g_pickCount;
static int g_pickSel;
static int g_pickAlive;

typedef struct { HWND hSelf, hList, hOk, hCancel; HFONT fUI; int scale; } PickCtx;
static PickCtx g_pick;

static void pick_fill(void)
{
    int i;
    SendMessageW(g_pick.hList, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < g_pickCount; i++) {
        wchar_t row[700], tb[64];
        WBuf b;
        int idx;
        wb_init(&b, row, 700);
        wb_int(&b, g_pickCand[i].items);
        wb_add(&b, L" 条日程 · 便笺 ");
        if (g_pickCand[i].hasNotes) { wb_int(&b, g_pickCand[i].notes); wb_add(&b, L" 条"); }
        else                        { wb_add(&b, L"无"); }
        wb_add(&b, L" · 最后修改 ");
        wb_add(&b, ft_text(g_pickCand[i].mtime, tb, 64));
        wb_add(&b, L"    ");
        wb_add(&b, g_pickCand[i].dir);
        idx = (int)SendMessageW(g_pick.hList, LB_ADDSTRING, 0, (LPARAM)row);
        if (idx >= 0) SendMessageW(g_pick.hList, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)i);
    }
    SendMessageW(g_pick.hList, LB_SETCURSEL, 0, 0);
}

static LRESULT CALLBACK pick_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        {
            int sc = g_pick.scale;
            HWND h;
            g_pick.fUI = make_font(10, FW_NORMAL, L"Microsoft YaHei UI");
            h = CreateWindowExW(0, L"STATIC",
                                L"找到以下数据（按最后修改时间排序）。选一份“改用”，当前这份会先自动存档：",
                                WS_CHILD | WS_VISIBLE, MulDiv(16, sc, 100), MulDiv(14, sc, 100),
                                MulDiv(700, sc, 100), MulDiv(24, sc, 100), hwnd, NULL, g_hInst, NULL);
            SendMessageW(h, WM_SETFONT, (WPARAM)g_pick.fUI, TRUE);
            g_pick.hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                           WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                           MulDiv(16, sc, 100), MulDiv(42, sc, 100),
                                           MulDiv(700, sc, 100), MulDiv(280, sc, 100),
                                           hwnd, (HMENU)(INT_PTR)IDP_LIST, g_hInst, NULL);
            SendMessageW(g_pick.hList, WM_SETFONT, (WPARAM)g_pick.fUI, TRUE);
            g_pick.hOk = CreateWindowExW(0, L"BUTTON", L"改用这一份",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                         MulDiv(610, sc, 100), MulDiv(334, sc, 100),
                                         MulDiv(106, sc, 100), MulDiv(30, sc, 100),
                                         hwnd, (HMENU)(INT_PTR)IDP_OK, g_hInst, NULL);
            g_pick.hCancel = CreateWindowExW(0, L"BUTTON", L"取消",
                                             WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                             MulDiv(498, sc, 100), MulDiv(334, sc, 100),
                                             MulDiv(106, sc, 100), MulDiv(30, sc, 100),
                                             hwnd, (HMENU)(INT_PTR)IDP_CANCEL, g_hInst, NULL);
            SendMessageW(g_pick.hOk, WM_SETFONT, (WPARAM)g_pick.fUI, TRUE);
            SendMessageW(g_pick.hCancel, WM_SETFONT, (WPARAM)g_pick.fUI, TRUE);
            pick_fill();
            SetFocus(g_pick.hList);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDP_OK:
            g_pickSel = (int)SendMessageW(g_pick.hList, LB_GETCURSEL, 0, 0);
            g_pickAlive = 0;
            DestroyWindow(hwnd);
            return 0;
        case IDP_CANCEL:
            g_pickSel = -1;
            g_pickAlive = 0;
            DestroyWindow(hwnd);
            return 0;
        case IDP_LIST:
            if (HIWORD(wp) == LBN_DBLCLK) {
                g_pickSel = (int)SendMessageW(g_pick.hList, LB_GETCURSEL, 0, 0);
                g_pickAlive = 0;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        break;

    case WM_CLOSE:
        g_pickSel = -1;
        g_pickAlive = 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_pick.fUI) { DeleteObject(g_pick.fUI); g_pick.fUI = NULL; }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int data_pick_dialog(const AppDataCand *c, int n)
{
    static int regd;
    HWND hwnd;
    MSG msg;
    int sc, maxPct, wa, dw, dh;
    RECT rc;
    if (!c || n <= 0) return -1;
    if (!regd) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = pick_proc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = PICK_CLASS;
        wc.hIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(APP_ICON_ID), IMAGE_ICON,
                                     GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
        wc.hIconSm = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(APP_ICON_ID), IMAGE_ICON,
                                       GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        RegisterClassExW(&wc);
        regd = 1;
    }
    g_pickCand = c;
    g_pickCount = n;
    g_pickSel = -1;
    g_pickAlive = 1;
    {
        HDC hdc = GetDC(NULL);
        sc = MulDiv(GetDeviceCaps(hdc, LOGPIXELSY), 100, 96);
        ReleaseDC(NULL, hdc);
    }
    wa = GetSystemMetrics(SM_CYSCREEN) - 60;
    maxPct = (int)((LONGLONG)wa * 100 / 430);
    if (sc > maxPct) sc = maxPct;
    maxPct = (int)((LONGLONG)(GetSystemMetrics(SM_CXSCREEN) - 40) * 100 / 748);
    if (sc > maxPct) sc = maxPct;
    if (sc < 70) sc = 70;
    g_pick.scale = sc;
    dw = MulDiv(732, sc, 100);
    dh = MulDiv(378, sc, 100);
    rc.left = 0; rc.top = 0; rc.right = dw; rc.bottom = dh;
    AdjustWindowRectEx(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, PICK_CLASS, L"迁移 / 导入已有数据",
                           WS_POPUP | WS_CAPTION | WS_SYSMENU,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top,
                           g_hMain, NULL, g_hInst, NULL);
    if (!hwnd) return -1;
    g_pick.hSelf = hwnd;
    center_over_main(hwnd, rc.right - rc.left, rc.bottom - rc.top);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    EnableWindow(g_hMain, FALSE);
    while (g_pickAlive) {
        if (!GetMessageW(&msg, NULL, 0, 0)) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    EnableWindow(g_hMain, TRUE);
    SetForegroundWindow(g_hMain);
    return g_pickSel;
}

/* 文件 → 数据保存位置 → 选择数据文件夹… */
static void do_data_dir_set(void)
{
    wchar_t dir[MAX_PATH], file[MAX_PATH], msg[900];
    AppDataCand c[10];
    int n, i, haveData = 0, items = -1, notes = 0;
    if (!pick_folder(L"选择存放笺数据的文件夹",
                     app_data_dir(), dir, MAX_PATH))
        return;
    if (path_same_dir(dir, app_data_dir())) {
        main_set_status(L"已经就是这个文件夹了");
        return;
    }
    path_join(file, MAX_PATH, dir, DATA_FILE_NAME);
    if (GetFileAttributesW(file) != INVALID_FILE_ATTRIBUTES) {
        n = app_data_find(c, 10);
        for (i = 0; i < n; i++)
            if (path_same_dir(c[i].dir, dir)) {
                items = c[i].items;
                notes = c[i].notes;
                break;
            }
        haveData = 1;
    }
    if (haveData) {
        WBuf b;
        wb_init(&b, msg, 900);
        wb_add(&b, L"这个文件夹里已经有一份笺的数据");
        if (items >= 0) {
            wb_add(&b, L"（");
            wb_int(&b, items);
            wb_add(&b, L" 条日程，便笺 ");
            wb_int(&b, notes);
            wb_add(&b, L" 条）");
        }
        wb_add(&b, L"。\n\n改用这份数据吗？\n（当前这份会先自动存档到历史里，不会丢）");
        if (MessageBoxW(g_hMain, msg, L"数据保存位置", MB_ICONQUESTION | MB_YESNO) != IDYES)
            return;
        data_switch(dir, 0, L"已改用该文件夹里的数据：");
        return;
    }
    {   /* 空文件夹：问要不要把当前数据搬过去 */
        int r = MessageBoxW(g_hMain,
                    L"这个文件夹里还没有笺的数据。\n\n"
                    L"“是”：把当前数据复制过去，以后就用这个文件夹（推荐）\n"
                    L"“否”：只把保存位置切过去，这里会是空的\n\n"
                    L"（无论选哪个，当前数据都不会被删除）",
                    L"数据保存位置", MB_ICONQUESTION | MB_YESNOCANCEL);
        if (r == IDCANCEL) return;
        data_switch(dir, r == IDYES, r == IDYES ? L"数据已复制到新位置："
                                                : L"数据位置已切换（空文件夹）：");
    }
}

/* 文件 → 数据保存位置 → 迁移 / 导入已有数据… */
static void do_data_import(void)
{
    AppDataCand all[10], list[10];
    int n, i, m = 0, sel;
    wchar_t msg[900], tb[64];
    n = app_data_find(all, 10);
    for (i = 0; i < n; i++) {
        if (path_same_dir(all[i].dir, app_data_dir())) continue;   /* 当前这份不用列 */
        list[m++] = all[i];
    }
    if (m == 0) {
        main_set_status(L"没有在本机其它位置找到笺的数据（可用“选择数据文件夹…”手动指定）");
        return;
    }
    sel = data_pick_dialog(list, m);
    if (sel < 0 || sel >= m) { main_set_status(L"已取消导入"); return; }
    {
        WBuf b;
        wb_init(&b, msg, 900);
        wb_add(&b, L"改用这一份数据吗？\n\n");
        wb_add(&b, list[sel].dir);
        wb_add(&b, L"\n");
        wb_int(&b, list[sel].items);
        wb_add(&b, L" 条日程 · 便笺 ");
        if (list[sel].hasNotes) { wb_int(&b, list[sel].notes); wb_add(&b, L" 条"); }
        else                    { wb_add(&b, L"无"); }
        wb_add(&b, L" · 最后修改 ");
        wb_add(&b, ft_text(list[sel].mtime, tb, 64));
        wb_add(&b, L"\n\n当前的这份会先自动存档；两边文件都不会被删除。");
        if (MessageBoxW(g_hMain, msg, L"迁移 / 导入已有数据",
                        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES)
            return;
    }
    {
        wchar_t why[400];
        WBuf b;
        wb_init(&b, why, 400);
        wb_add(&b, L"已导入并切换到：");
        wb_add(&b, list[sel].dir);
        data_switch(list[sel].dir, 0, why);
    }
}

/* 文件 → 数据保存位置 → 备份当前数据到文件夹… */
static void do_data_export(void)
{
    wchar_t dir[MAX_PATH], file[MAX_PATH], msg[700];
    if (!pick_folder(L"选择备份文件夹（主数据 + 便笺状态 + 便笺墙 + 历史版本都会复制过去）",
                     app_data_dir(), dir, MAX_PATH))
        return;
    if (path_same_dir(dir, app_data_dir())) {
        main_set_status(L"这就是当前数据文件夹，不用备份到自己身上");
        return;
    }
    path_join(file, MAX_PATH, dir, DATA_FILE_NAME);
    if (GetFileAttributesW(file) != INVALID_FILE_ATTRIBUTES) {
        WBuf b;
        wb_init(&b, msg, 700);
        wb_add(&b, L"这个文件夹里已经有笺的数据文件了：\n");
        wb_add(&b, file);
        wb_add(&b, L"\n\n继续会用当前数据覆盖它（该文件夹里已有的历史版本不会被删）。要继续吗？");
        if (MessageBoxW(g_hMain, msg, L"备份当前数据", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
            return;
    }
    if (app_data_copy_to(dir)) {
        WBuf b;
        wb_init(&b, msg, 700);
        wb_add(&b, L"已备份到 ");
        wb_add(&b, dir);
        wb_add(&b, L"（主数据 + 便笺状态 + 便笺墙 + 历史版本）");
        main_set_status(msg);
    } else {
        main_set_status(L"备份失败：请检查该文件夹是否可写、当前是否已有数据文件");
    }
}

/* ---------------------------------------------------------------- 迁移 */
/* 在拷贝文件夹里放一份“怎么用”的说明（UTF-8 + BOM：记事本和其它编辑器都好认） */
static void write_migrate_readme(const wchar_t *dir, const SYSTEMTIME *st)
{
    wchar_t path[MAX_PATH], text[2048], when[64];
    char utf8[4096];
    int n, wn;
    HANDLE h;
    DWORD wrote = 0;
    WBuf b;

    path_join(path, MAX_PATH, dir, L"迁移说明.txt");
    {
        WBuf tb;
        wb_init(&tb, when, 64);
        wb_int(&tb, st->wYear);
        wb_add(&tb, L"-");
        if (st->wMonth < 10) wb_ch(&tb, L'0');
        wb_int(&tb, st->wMonth);
        wb_add(&tb, L"-");
        if (st->wDay < 10) wb_ch(&tb, L'0');
        wb_int(&tb, st->wDay);
        wb_add(&tb, L" ");
        if (st->wHour < 10) wb_ch(&tb, L'0');
        wb_int(&tb, st->wHour);
        wb_add(&tb, L":");
        if (st->wMinute < 10) wb_ch(&tb, L'0');
        wb_int(&tb, st->wMinute);
        wb_add(&tb, L":");
        if (st->wSecond < 10) wb_ch(&tb, L'0');
        wb_int(&tb, st->wSecond);
    }
    wb_init(&b, text, 2048);
    wb_add(&b, L"笺 · 数据拷贝（迁移用）\r\n");
    wb_add(&b, L"生成时间：");
    wb_add(&b, when);
    wb_add(&b, L"\r\n\r\n");
    wb_add(&b, L"这个文件夹是「笺」当前数据的完整拷贝：\r\n");
    wb_add(&b, L"  zsched_data.txt    主数据：日程、课表、便笺正文与提醒设置\r\n");
    wb_add(&b, L"  zsched_notes.txt   桌面便笺：位置、尺寸、纸色、外观、置顶\r\n");
    wb_add(&b, L"  zsched_board.txt   便笺墙：位置、固定、收起、每张便笺在第几页\r\n");
    wb_add(&b, L"  zsched_history\\    历史版本存档（自动 + 手动）\r\n\r\n");
    wb_add(&b, L"怎么用它迁移到另一台电脑：\r\n");
    wb_add(&b, L"  1) 把整个文件夹拷过去；\r\n");
    wb_add(&b, L"  2) 把 笺.exe 也放进这个文件夹（程序默认读 exe 同目录的数据），双击运行即可；\r\n");
    wb_add(&b, L"     或者先运行 笺，再在「文件 → 数据保存位置 → 选择数据文件夹…」里选中这个文件夹。\r\n\r\n");
    wb_add(&b, L"注意：\r\n");
    wb_add(&b, L"  · 这里只是拷贝，原来的数据不会被动到；\r\n");
    wb_add(&b, L"  · 迁移后程序会把那个文件夹当成新的数据目录；\r\n");
    wb_add(&b, L"  · 提醒的“已提醒次数”等运行状态不会带过来（只保存日程与便笺本身）。\r\n");

    n = (int)wcslen(text);
    wn = WideCharToMultiByte(CP_UTF8, 0, text, n, utf8, (int)sizeof(utf8), NULL, NULL);
    if (wn <= 0) return;
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    {
        const BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
        WriteFile(h, bom, 3, &wrote, NULL);
    }
    WriteFile(h, utf8, (DWORD)wn, &wrote, NULL);
    CloseHandle(h);
}

/* 文件 → 数据保存位置 → 迁移：在桌面生成一份完整的数据拷贝文件夹 */
static void do_data_migrate_desktop(void)
{
    wchar_t desk[MAX_PATH], dst[MAX_PATH], nm[MAX_PATH], base[MAX_PATH], msg[900];
    SYSTEMTIME st;
    int i;

    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY | CSIDL_FLAG_CREATE, NULL,
                         SHGFP_TYPE_CURRENT, desk) != S_OK || !desk[0]) {
        MessageBoxW(g_hMain,
                    L"找不到桌面文件夹。\n\n可以改用「备份当前数据到文件夹…」自己选一个位置。",
                    L"迁移数据", MB_ICONWARNING);
        return;
    }
    GetLocalTime(&st);
    {
        WBuf b;
        wb_init(&b, base, MAX_PATH);
        wb_add(&b, L"笺数据迁移_");
        wb_int(&b, st.wYear);
        if (st.wMonth < 10) wb_ch(&b, L'0');
        wb_int(&b, st.wMonth);
        if (st.wDay < 10) wb_ch(&b, L'0');
        wb_int(&b, st.wDay);
        wb_ch(&b, L'_');
        if (st.wHour < 10) wb_ch(&b, L'0');
        wb_int(&b, st.wHour);
        if (st.wMinute < 10) wb_ch(&b, L'0');
        wb_int(&b, st.wMinute);
        if (st.wSecond < 10) wb_ch(&b, L'0');
        wb_int(&b, st.wSecond);
    }
    /* 同一秒连点两次也不覆盖：加 _2 / _3 … */
    wcsncpy(nm, base, MAX_PATH - 1);
    nm[MAX_PATH - 1] = 0;
    path_join(dst, MAX_PATH, desk, nm);
    for (i = 2; i < 100 && GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES; i++) {
        WBuf b;
        wb_init(&b, nm, MAX_PATH);
        wb_add(&b, base);
        wb_ch(&b, L'_');
        wb_int(&b, i);
        path_join(dst, MAX_PATH, desk, nm);
    }

    /* 先把内存里的状态落盘，保证拷出来的是“现在这一份” */
    if (!model_save(model_path()))
        main_set_status(L"注意：保存当前数据失败，拷贝的可能是稍早的版本");
    note_save_state();

    if (!app_data_copy_to(dst)) {
        MessageBoxW(g_hMain,
                    L"复制失败：桌面不可写，或者当前还没有数据文件。",
                    L"迁移数据", MB_ICONERROR);
        return;
    }
    write_migrate_readme(dst, &st);
    ShellExecuteW(g_hMain, L"open", dst, NULL, NULL, SW_SHOWNORMAL);
    {
        WBuf b;
        wb_init(&b, msg, 900);
        wb_add(&b, L"已在桌面生成数据拷贝：");
        wb_add(&b, dst);
        wb_add(&b, L"（主数据 + 便笺状态 + 便笺墙 + 历史版本 + 迁移说明）");
        main_set_status(msg);
    }
}

static void do_data_open_dir(void)
{
    const wchar_t *d = app_data_dir();
    if (d && d[0]) {
        ShellExecuteW(g_hMain, L"open", d, NULL, NULL, SW_SHOWNORMAL);
        main_set_status(L"已打开数据文件夹");
    }
}

static HRESULT CALLBACK about_td_cb(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LONG_PTR ref)
{
    (void)hwnd; (void)wp; (void)ref;
    if (msg == TDN_HYPERLINK_CLICKED) {
        ShellExecuteW(g_hMain, L"open", (const wchar_t *)lp, NULL, NULL, SW_SHOWNORMAL);
        return S_OK;
    }
    return S_OK;
}

static void show_about(void)
{
    static const wchar_t *content =
        L"版本：v" APP_VERSION L"\n"
        L"作者：" APP_AUTHOR L"\n\n"
        L"主要功能：\n"
        L"  · 清单卡片 / 表格 / 周网格 三种视图随时切换\n"
        L"  · 日期 + 时:分:秒 精确到秒的日程时间\n"
        L"  · 每周（可多选星期几）与自定义周期（每 N 天/周/月/年）重复\n"
        L"  · 正文富文本：同一段文字可混排不同字体、字号，支持加粗/斜体/下划线\n"
        L"  · 每条日程可插入图片与任意附件，内置查看器按原图 100% 显示\n"
        L"  · 最近日程倒计时 + 自选日程倒计时（每秒刷新）\n"
        L"  · 便笺式独立窗口：可单条/批量打开，纸色自选，可贴桌面\n"
        L"  · 便笺墙支持复制 / 粘贴，可把一张便笺复制到新开的一页\n"
        L"  · 纪念日：记录已经过去的事，自动累计天数与周年\n"
        L"  · 周表格视图支持缩放（Ctrl+滚轮）\n"
        L"  · 课表模式：标成课表的日程到点自动开关便笺，下一节在原位置接力\n"
        L"  · 倒计时显示精度可选（天 / 时 / 分 / 秒），提醒可弹窗 / 通知栏 / 提示音\n\n"
        L"快捷键：\n"
        L"  Ctrl+N 新建   Enter 编辑   F2 重命名   Delete 删除\n"
        L"  Ctrl+C 复制便笺   Ctrl+V 粘贴便笺   Ctrl+D 复制（旧别名）\n"
        L"  Space 切换完成  Ctrl+S 保存  F5 重新载入\n"
        L"  Ctrl+1/2/3 切换视图  Ctrl+T 回到本周";
    static const wchar_t *footer =
        L"项目主页：<a href=\"" APP_URL L"\">" APP_URL L"</a>";
    TASKDIALOGCONFIG cfg;
    TASKDIALOG_BUTTON btns[2];
    int nBtn = 0;
    HRESULT hr;

    ZeroMemory(&cfg, sizeof(cfg));
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = g_hMain;
    cfg.hInstance = g_hInst;
    cfg.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    cfg.pszWindowTitle = L"关于 笺";
    cfg.pszMainIcon = TD_INFORMATION_ICON;
    cfg.pszMainInstruction = L"笺 · 便笺式桌面日程管家";
    cfg.pszContent = content;
    cfg.pszFooter = footer;
    cfg.pfCallback = about_td_cb;
    btns[0].nButtonID = 1001;
    btns[0].pszButtonText = L"打开项目主页";
    btns[1].nButtonID = IDOK;
    btns[1].pszButtonText = L"关闭";
    cfg.pButtons = btns;
    cfg.cButtons = 2;
    cfg.nDefaultButton = IDOK;

    hr = TaskDialogIndirect(&cfg, &nBtn, NULL, NULL);
    if (SUCCEEDED(hr)) {
        if (nBtn == 1001)
            ShellExecuteW(g_hMain, L"open", APP_URL, NULL, NULL, SW_SHOWNORMAL);
        return;
    }
    /* 极老的系统没有 TaskDialog：退回普通消息框（链接以文字显示） */
    MessageBoxW(g_hMain,
        L"笺 · 便笺式桌面日程管家\n"
        L"版本：v" APP_VERSION L"\n"
        L"作者：" APP_AUTHOR L"\n\n"
        L"项目主页：\n" APP_URL L"\n\n"
        L"纯 C 语言 + Win32 API 实现",
        L"关于", MB_ICONINFORMATION);
}

static LRESULT CALLBACK main_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        {
            HMENU mb = CreateMenu();
            HMENU mf = CreatePopupMenu();
            g_mFile = mf;
            HMENU me = CreatePopupMenu();
            g_mEdit = me;
            HMENU mv = CreatePopupMenu();
            g_mView = mv;
            HMENU mh = CreatePopupMenu();
            AppendMenuW(mf, MF_STRING, IDM_FILE_NEW, L"新建日程(&N)\tCtrl+N");
            AppendMenuW(mf, MF_SEPARATOR, 0, NULL);
            AppendMenuW(mf, MF_STRING, IDM_FILE_SAVE, L"保存(&S)\tCtrl+S");
            AppendMenuW(mf, MF_STRING, IDM_FILE_RELOAD, L"重新载入(&R)\tF5");
            AppendMenuW(mf, MF_STRING, IDM_FILE_CSV, L"导入 CSV 课表…(&I)");
            AppendMenuW(mf, MF_STRING | (g_background ? MF_CHECKED : 0), IDM_FILE_BACK,
                        L"后台运行（不占任务栏）(&B)");
            AppendMenuW(mf, MF_STRING, IDM_FILE_DIR, L"打开首个附件(&O)");
    AppendMenuW(mf, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mf, MF_STRING | (app_autostart_get() ? MF_CHECKED : 0), IDM_FILE_AUTORUN,
                L"开机自启动(&S)");
    AppendMenuW(mf, MF_STRING, IDM_FILE_ARCH, L"存档当前版本(&A)	Ctrl+Alt+S");
    AppendMenuW(mf, MF_STRING, IDM_FILE_HIST, L"打开历史版本文件夹(&H)");
    AppendMenuW(mf, MF_STRING | (model_history_count() ? 0 : MF_GRAYED), IDM_FILE_HISTRST,
                L"恢复最近一个历史版本(&R)");
    {   /* 数据保存位置：统一目录 + 自定义路径 + 导入旧数据 */
        g_mDataDir = CreatePopupMenu();
        AppendMenuW(g_mDataDir, MF_STRING | MF_DISABLED | MF_GRAYED, IDM_FILE_DIRSHOW,
                    L"当前：—");
        AppendMenuW(g_mDataDir, MF_SEPARATOR, 0, NULL);
        AppendMenuW(g_mDataDir, MF_STRING, IDM_FILE_DIRSET, L"选择数据文件夹…(&S)");
        AppendMenuW(g_mDataDir, MF_STRING, IDM_FILE_IMPORT, L"迁移 / 导入已有数据…(&M)");
        AppendMenuW(g_mDataDir, MF_STRING, IDM_FILE_MIGRATE,
                    L"迁移：在桌面生成数据拷贝(&T)");
        AppendMenuW(g_mDataDir, MF_STRING, IDM_FILE_DIREXPORT, L"备份当前数据到文件夹…(&B)");
        AppendMenuW(g_mDataDir, MF_SEPARATOR, 0, NULL);
        AppendMenuW(g_mDataDir, MF_STRING, IDM_FILE_DIROPEN, L"打开数据文件夹(&O)");
        AppendMenuW(mf, MF_POPUP, (UINT_PTR)g_mDataDir, L"数据保存位置(&D)");
        data_dir_label_update();
    }
            AppendMenuW(mf, MF_SEPARATOR, 0, NULL);
            AppendMenuW(mf, MF_STRING, IDM_FILE_EXIT, L"退出(&X)\tAlt+F4");

            AppendMenuW(me, MF_STRING, IDM_EDIT_EDIT, L"编辑(&E)…\tEnter");
            AppendMenuW(me, MF_STRING, IDM_EDIT_RENAME, L"重命名(&M)\tF2");
            AppendMenuW(me, MF_STRING, IDM_EDIT_COPY, L"复制便笺(&C)\tCtrl+C");
            AppendMenuW(me, MF_STRING | (g_clipValid ? 0 : MF_GRAYED),
                        IDM_EDIT_PASTE, L"粘贴便笺(&P)\tCtrl+V");
            AppendMenuW(me, MF_SEPARATOR, 0, NULL);
            AppendMenuW(me, MF_STRING, IDM_EDIT_DONE, L"标记完成(&K)\t空格");
            AppendMenuW(me, MF_STRING, IDM_EDIT_ENABLE, L"启用 / 暂停");
            AppendMenuW(me, MF_STRING, IDM_EDIT_VIEWIMG, L"查看原图(&V)");
            AppendMenuW(me, MF_SEPARATOR, 0, NULL);
            AppendMenuW(me, MF_STRING, IDM_EDIT_DEL, L"删除(&D)\tDelete");

            AppendMenuW(mv, MF_STRING, IDM_VIEW_LIST, L"清单视图(&L)\tCtrl+1");
            AppendMenuW(mv, MF_STRING, IDM_VIEW_TABLE, L"表格视图(&T)\tCtrl+2");
            AppendMenuW(mv, MF_STRING, IDM_VIEW_WEEK, L"周表格视图(&W)\tCtrl+3");
            AppendMenuW(mv, MF_SEPARATOR, 0, NULL);
            AppendMenuW(mv, MF_STRING, IDM_VIEW_TODAY, L"回到本周(&H)\tCtrl+T");
            AppendMenuW(mv, MF_STRING, IDM_WEEK_ZOOMIN, L"周表放大(&+)\tCtrl+滚轮");
            AppendMenuW(mv, MF_STRING, IDM_WEEK_ZOOMOUT, L"周表缩小(&-)\tCtrl+滚轮");
            AppendMenuW(mv, MF_STRING, IDM_WEEK_ZOOMRST, L"周表恢复 100%(&0)");
            AppendMenuW(mv, MF_SEPARATOR, 0, NULL);
            AppendMenuW(mv, MF_STRING, IDM_BOARD_AUTOALL, L"\u4fbf\u7b3a\u5168\u90e8\u6062\u590d\u81ea\u52a8\u6392\u5217(&R)");
    AppendMenuW(mv, MF_STRING, IDM_BOARD_HIDE, L"\u6536\u8d77\u9009\u4e2d\u7684\u4fbf\u7b3a(&H)\tH");
    AppendMenuW(mv, MF_STRING | (stickies_hidden_count() ? 0 : MF_GRAYED),
                IDM_BOARD_SHOWALL, L"显示所有便笺（摆回收起的）(&V)");
    AppendMenuW(mv, MF_STRING, IDM_BOARD_HIDEALL,
                stickies_hidden_count() ? L"显示全部便笺(&L)"
                                        : L"把便笺全部收起来(&L)");
    AppendMenuW(mv, MF_STRING, IDM_BOARD_SIZEALL, L"便笺全部恢复默认大小(&Z)");
    AppendMenuW(mv, MF_SEPARATOR, 0, NULL);
    AppendMenuW(mv, MF_STRING, IDM_PAGE_PREV, L"上一页(&P)\tPgUp");
    AppendMenuW(mv, MF_STRING, IDM_PAGE_NEXT, L"下一页(&N)\tPgDn");
    AppendMenuW(mv, MF_STRING, IDM_PAGE_NEW,
                L"新开一页（本页满了就翻到这页继续贴）(&W)");
            AppendMenuW(mv, MF_SEPARATOR, 0, NULL);
            AppendMenuW(mv, MF_STRING, IDM_VIEW_SORTT, L"按下次发生时间排序(&T)");
            AppendMenuW(mv, MF_STRING, IDM_VIEW_SORTN, L"按标题排序(&N)");
            AppendMenuW(mv, MF_STRING, IDM_VIEW_SORTC, L"按开始时间排序(&S)");
            AppendMenuW(mv, MF_SEPARATOR, 0, NULL);
            AppendMenuW(mv, MF_STRING, IDM_VIEW_PIN, L"设为倒计时目标(&P)");
            AppendMenuW(mv, MF_STRING, IDM_VIEW_UNPIN, L"取消固定倒计时");
            {   /* 倒计时显示精度：有选中就改选中，没有就改全部 */
                HMENU mcd = CreatePopupMenu();
                AppendMenuW(mcd, MF_STRING, IDM_CDU_DAY, L"显示到 天（只显示天数）(&D)");
                AppendMenuW(mcd, MF_STRING, IDM_CDU_HOUR, L"显示到 时(&H)");
                AppendMenuW(mcd, MF_STRING, IDM_CDU_MIN, L"显示到 分(&M)");
                AppendMenuW(mcd, MF_STRING, IDM_CDU_SEC, L"显示到 秒（默认）(&S)");
                AppendMenuW(mv, MF_POPUP, (UINT_PTR)mcd, L"倒计时显示精度(&U)");
            }
            AppendMenuW(mv, MF_SEPARATOR, 0, NULL);
            AppendMenuW(mv, MF_STRING, IDM_VIEW_CLEAR, L"清除已完成日程(&C)");

            {
                HMENU mn = CreatePopupMenu();
                AppendMenuW(mn, MF_STRING, IDM_NOTE_ONE, L"为选中日程开便笺(&O)\tCtrl+B");
                AppendMenuW(mn, MF_STRING, IDM_NOTE_BATCH, L"为选中日程批量开便笺(&B)");
                AppendMenuW(mn, MF_STRING, IDM_NOTE_ALL, L"为所有待办开便笺(&A)");
                AppendMenuW(mn, MF_SEPARATOR, 0, NULL);
                AppendMenuW(mn, MF_STRING, IDM_NOTE_STYLE, L"切换便笺外观(&S)");
                AppendMenuW(mn, MF_STRING, IDM_NOTE_TOPMOST, L"便笺置顶开关(&T)");
                AppendMenuW(mn, MF_SEPARATOR, 0, NULL);
                AppendMenuW(mn, MF_STRING, IDM_BOARD_TT,
                            L"标记 / 取消课表模式（选中日程）(&L)");
                AppendMenuW(mn, MF_STRING | (note_tt_auto_get() ? MF_CHECKED : 0),
                            IDM_BOARD_TTAUTO, L"课表便笺自动跟随(&A)");
                AppendMenuW(mn, MF_STRING, IDM_BOARD_TTNOW, L"打开现在这节课的便笺(&C)");
                AppendMenuW(mn, MF_SEPARATOR, 0, NULL);
                AppendMenuW(mn, MF_STRING, IDM_NOTE_CLOSE, L"关闭全部便笺(&X)");
                AppendMenuW(mb, MF_POPUP, (UINT_PTR)mn, L"便笺(&N)");
            }
            AppendMenuW(mh, MF_STRING, IDM_HELP_HELP, L"使用说明(&H)\tF1");
            AppendMenuW(mh, MF_STRING, IDM_HELP_ABOUT, L"关于(&A)");

            AppendMenuW(mb, MF_POPUP, (UINT_PTR)mf, L"文件(&F)");
            AppendMenuW(mb, MF_POPUP, (UINT_PTR)me, L"编辑(&E)");
            AppendMenuW(mb, MF_POPUP, (UINT_PTR)mv, L"视图(&V)");
            AppendMenuW(mb, MF_POPUP, (UINT_PTR)mh, L"帮助(&H)");
            SetMenu(hwnd, mb);
        }
        return 0;

    case WM_SIZE:
        layout(hwnd);
        return 0;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO *mmi = (MINMAXINFO *)lp;
            int needW = 12;
            int i;
            HWND btns[10];
            btns[0] = g_btnNew;   btns[1] = g_btnEdit;  btns[2] = g_btnDel;
            btns[3] = g_btnCopy;  btns[4] = g_btnPaste; btns[5] = g_btnNote;
            btns[6] = g_btnList;  btns[7] = g_btnTable; btns[8] = g_btnWeek;
            btns[9] = g_btnPin;
            for (i = 0; i < 10; i++) if (btns[i]) needW += flat_width(btns[i], 28) + 8;
            if (g_btnRefresh) needW += flat_width(g_btnRefresh, 28) + 40;
            mmi->ptMinTrackSize.x = needW;
            mmi->ptMinTrackSize.y = (g_lhUI + 12) * 2 + TH_GAP * 3 + 260;
        }
        return 0;

    case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wp;
            RECT rc;
            GetClientRect(hwnd, &rc);
            paper_notebook(hdc, &rc, g_scalePct, &g_pageRect, &g_contentLeft);
        }
        return 1;

    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wp, TH_BG);
        return (LRESULT)g_brBg;


    case WM_TIMER:
        if (wp == 1) main_tick();
        return 0;

    case WM_APP + 1:                           /* 托盘图标 */
        {
            int ev = (int)LOWORD(lp);
            if (ev == WM_LBUTTONUP || ev == WM_LBUTTONDBLCLK) set_background(!g_background);
            else if (ev == WM_RBUTTONUP) tray_menu();
        }
        return 0;

    case WM_INITMENUPOPUP:
        /* 注意：WM_INITMENUPOPUP 里菜单句柄在 wParam（不是 lParam）——
           以前这里写成 lp，导致这两个子菜单打开时从来不刷新状态 */
        if ((HMENU)wp == g_mDataDir) data_dir_label_update();
        if ((HMENU)wp == g_mFile) {
            CheckMenuItem(g_mFile, IDM_FILE_BACK,
                          MF_BYCOMMAND | (g_background ? MF_CHECKED : MF_UNCHECKED));
        }
        if ((HMENU)wp == g_mEdit) {
            EnableMenuItem(g_mEdit, IDM_EDIT_PASTE,
                           MF_BYCOMMAND | (g_clipValid ? MF_ENABLED : MF_GRAYED));
        }
        if ((HMENU)wp == g_mView) {
            int hid = stickies_hidden_count();
            EnableMenuItem(g_mView, IDM_BOARD_SHOWALL,
                           MF_BYCOMMAND | (hid ? MF_ENABLED : MF_GRAYED));
            ModifyMenuW(g_mView, IDM_BOARD_HIDEALL, MF_BYCOMMAND | MF_STRING,
                        IDM_BOARD_HIDEALL,
                        hid ? L"\u663e\u793a\u5168\u90e8\u4fbf\u7b3a(&L)" : L"\u628a\u4fbf\u7b3a\u5168\u90e8\u6536\u8d77\u6765(&L)");
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_BTN_NEW: case IDM_FILE_NEW:     do_new_at(NULL); break;
        case IDC_BTN_COPY:
        case IDM_EDIT_COPY:  do_copy(main_selected_id()); break;
        case IDC_BTN_PASTE:
        case IDM_EDIT_PASTE: do_paste(); break;
        case IDC_BTN_EDIT:
        case IDM_EDIT_EDIT:
            do_edit(HIWORD(wp) == 1 ? (int)lp : main_selected_id());
            break;
        case IDC_BTN_NOTE:
        case IDM_NOTE_ONE:    do_note(main_selected_id(), 0); break;
        case IDM_NOTE_BATCH:  do_note(0, 1); break;
        case IDM_NOTE_ALL:    do_note_all(); break;
        case IDM_BOARD_TT:
            if (HIWORD(wp) == 1) do_toggle_timetable((int)lp);
            else                 do_toggle_timetable_sel();
            break;
        case IDM_BOARD_TTAUTO:
            note_tt_auto_set(note_tt_auto_get() ? 0 : 1);
            main_set_status(note_tt_auto_get()
                            ? L"课表便笺自动跟随：开（到点自动开关便笺）"
                            : L"课表便笺自动跟随：关");
            break;
        case IDM_BOARD_TTNOW:
            {
                Schedule *cur = sched_class_now();
                if (cur) {
                    note_open(cur->id, NOTE_STYLE_PAPER, 0, 1);
                    main_set_status(L"已打开现在这节课的便笺");
                } else {
                    main_set_status(L"现在没有正在上的课（先在右键菜单里标成“课表模式”）");
                }
            }
            break;
        case IDM_CDU_DAY: case IDM_CDU_HOUR: case IDM_CDU_MIN: case IDM_CDU_SEC:
            do_set_cd_unit_many(HIWORD(wp) == 1 ? (int)lp : 0,
                                LOWORD(wp) - IDM_CDU_DAY + 1);
            break;
        case IDM_NOTE_CLOSE:
            note_close_all();
            main_set_status(L"已关闭全部便笺");
            break;
        case IDM_NOTE_STYLE:
            {
                int ids[24];
                int k = main_pick_ids(ids, 24), j;
                int changed = 0;
                for (j = 0; j < k; j++) {
                    int st = NOTE_STYLE_PAPER, ci = 0;
                    if (!note_is_open(ids[j])) continue;
                    note_get_look(ids[j], &st, &ci);
                    note_set_style(ids[j], st == NOTE_STYLE_PAPER ? NOTE_STYLE_SYSTEM
                                                                 : NOTE_STYLE_PAPER);
                    changed++;
                }
                main_set_status(changed ? L"已切换便笺外观"
                                        : L"请先选中已打开便笺的日程");
            }
            break;
        case IDM_NOTE_TOPMOST:
            {
                int ids[24];
                int k = main_pick_ids(ids, 24), j;
                int changed = 0;
                for (j = 0; j < k; j++) {
                    if (note_is_open(ids[j])) { note_toggle_top(ids[j]); changed++; }
                }
                main_set_status(changed ? L"已切换便笺置顶"
                                        : L"便笺未打开：在便笺上点右上角图钉即可置顶");
            }
            break;
        case IDM_FILE_SAVE:
            if (model_save(model_path())) main_set_status(L"已保存到数据文件");
            else main_set_status(L"保存失败：无法写入数据文件");
            break;
        case IDM_FILE_RELOAD:
            refresh_from_disk();
            main_set_status(L"已重新载入数据文件");
            break;
        case IDM_FILE_DIR:   do_open_attach(main_selected_id(), 0); break;
        case IDM_FILE_AUTORUN:
            {
                int on = app_autostart_get();
                app_autostart_set(on ? 0 : 1);
                main_set_status(app_autostart_get() ? L"已设为开机自启动"
                                                    : L"已取消开机自启动");
            }
            break;
        case IDM_FILE_ARCH:
            if (model_archive(L"manual")) {
                wchar_t buf[160];
                WBuf b;
                wb_init(&b, buf, 160);
                wb_add(&b, L"已存档到 zsched_history（共 ");
                wb_int(&b, model_history_count());
                wb_add(&b, L" 份）");
                main_set_status(buf);
            } else {
                main_set_status(L"存档失败（数据文件不存在？）");
            }
            break;
        case IDM_FILE_HIST:
            {
                const wchar_t *dir = model_history_dir();
                if (dir && dir[0]) ShellExecuteW(hwnd, L"open", dir, NULL, NULL, SW_SHOWNORMAL);
                main_set_status(L"已打开历史版本文件夹");
            }
            break;
        case IDM_FILE_HISTRST:
            if (MessageBoxW(hwnd, L"确定恢复最近一个历史版本吗？\n"
                                   L"（当前版本会先自动存档，不会丢）",
                            L"恢复历史版本", MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES)
                break;
            if (model_history_restore_latest()) {
                refresh_from_disk();
                main_rebuild_views();
                main_set_status(L"已恢复最近一个历史版本");
            } else {
                main_set_status(L"没有可恢复的历史版本");
            }
            break;
        case IDM_FILE_CSV:   csv_import_show(hwnd); break;
        case IDM_FILE_BACK:
        case IDM_TRAY_BACK:
            set_background(!g_background);
            break;
        case IDM_TRAY_SHOW: set_background(0); break;
        case IDM_TRAY_NOTES: do_note_all(); break;
        case IDM_TRAY_STOPSND: snd_stop(); main_set_status(L"已停止铃声"); break;
        case IDM_TRAY_EXIT: PostMessageW(hwnd, WM_CLOSE, 0, 0); break;
        case IDM_PAGE_PREV:
            if (g_view != ZS_VIEW_LIST) set_view(ZS_VIEW_LIST);
            stickies_page_set(g_hBoard, stickies_page_get(g_hBoard) - 1);
            break;
        case IDM_PAGE_NEXT:
            if (g_view != ZS_VIEW_LIST) set_view(ZS_VIEW_LIST);
            stickies_page_set(g_hBoard, stickies_page_get(g_hBoard) + 1);
            break;
        case IDM_PAGE_NEW:
            if (g_view != ZS_VIEW_LIST) set_view(ZS_VIEW_LIST);
            stickies_page_new(g_hBoard);
            break;
        case IDM_PAGE_MOVEPREV:
        case IDM_PAGE_MOVENEXT:
            stickies_move_page(HIWORD(wp) == 1 ? (int)lp : main_selected_id(),
                               LOWORD(wp) == IDM_PAGE_MOVEPREV ? -1 : 1);
            break;
        case IDM_SEARCH_FOCUS: do_search_focus(); break;
        case IDC_SEARCH:
            if (HIWORD(wp) == EN_CHANGE) {
                GetWindowTextW(g_hSearch, g_search, 128);
                g_search[127] = 0;
                if (g_view == ZS_VIEW_TABLE) main_rebuild_views();
                update_status_default();
            }
            return 0;
        case IDM_FILE_DIRSET:    do_data_dir_set(); break;
        case IDM_FILE_IMPORT:    do_data_import(); break;
        case IDM_FILE_DIREXPORT: do_data_export(); break;
        case IDM_FILE_MIGRATE:   do_data_migrate_desktop(); break;
        case IDM_FILE_DIROPEN:   do_data_open_dir(); break;
        case IDM_FILE_EXIT:  PostMessageW(hwnd, WM_CLOSE, 0, 0); break;
        case IDC_BTN_PIN:
        case IDM_VIEW_PIN:    do_pin(HIWORD(wp) == 1 ? (int)lp : main_selected_id()); break;
        case IDM_VIEW_UNPIN:  g_pinId = 0; update_status_default();
                              InvalidateRect(g_hPanel, NULL, FALSE); break;
        case IDC_BTN_LIST: case IDM_VIEW_LIST:   set_view(ZS_VIEW_LIST); break;
        case IDC_BTN_TABLE: case IDM_VIEW_TABLE: set_view(ZS_VIEW_TABLE); break;
        case IDC_BTN_WEEK: case IDM_VIEW_WEEK:   set_view(ZS_VIEW_WEEK); break;
        case IDM_VIEW_TODAY:  week_goto_today(g_hWeek); break;
        case IDM_WEEK_ZOOMIN:  week_zoom(g_hWeek, 1); break;
        case IDM_WEEK_ZOOMOUT: week_zoom(g_hWeek, -1); break;
        case IDM_WEEK_ZOOMRST: week_zoom(g_hWeek, 0); break;
        case IDM_BOARD_HIDE:
            {
                int nid = (HIWORD(wp) == 1 && lp) ? (int)lp : main_selected_id();
                if (!nid) { main_set_status(L"\u8bf7\u5148\u9009\u4e2d\u8981\u6536\u8d77\u7684\u4fbf\u7b3a"); break; }
                stickies_set_hidden(nid, 1);
                main_set_status(L"\u5df2\u6536\u8d77\u8fd9\u5f20\u4fbf\u7b3a\uff08\u89c6\u56fe\u83dc\u5355\u53ef\u6446\u56de\u6765\uff09");
                update_status_default();
            }
            break;
        case IDM_BOARD_SHOWALL:
            stickies_show_all();
            main_set_status(L"\u5df2\u6446\u56de\u6240\u6709\u4fbf\u7b3a");
            update_status_default();
            break;
        case IDM_BOARD_HIDEALL:
            {
                int alloff = (stickies_hidden_count() > 0);
                stickies_hide_all(alloff ? 0 : 1);
                main_set_status(alloff ? L"\u5df2\u6446\u56de\u6240\u6709\u4fbf\u7b3a"
                                       : L"\u5df2\u628a\u4fbf\u7b3a\u5168\u90e8\u6536\u8d77\u6765\uff08\u89c6\u56fe\u83dc\u5355\u53ef\u6446\u56de\u6765\uff09");
                update_status_default();
            }
            break;
        case IDM_BOARD_AUTOALL:
            stickies_reset_all();
            main_set_status(L"\u4fbf\u7b3a\u5df2\u5168\u90e8\u6062\u590d\u81ea\u52a8\u6392\u5217");
            break;
        case IDM_BOARD_SIZEALL:
            stickies_reset_size_all();
            main_set_status(L"便笺已全部恢复默认大小");
            break;
        case IDC_BTN_REFRESH:
            model_refresh_next();
            main_rebuild_views();
            main_set_status(L"已刷新");
            break;
        case IDM_EDIT_RENAME: begin_rename(); break;
        case IDM_EDIT_DONE:   do_toggle_done(main_selected_id()); break;
        case IDM_EDIT_VIEWIMG: do_view_images(main_selected_id()); break;
        case IDM_EDIT_ENABLE:
            {
                Schedule *s = model_by_id(main_selected_id());
                if (s) {
                    s->enabled = !s->enabled;
                    model_save(model_path());
                    main_rebuild_views();
                }
            }
            break;
        case IDM_VIEW_SORTT: g_sortMode = SORT_TIME;  main_rebuild_views(); break;
        case IDM_VIEW_SORTN: g_sortMode = SORT_TITLE; main_rebuild_views(); break;
        case IDM_VIEW_SORTC: g_sortMode = SORT_START; main_rebuild_views(); break;
        case IDM_VIEW_CLEAR:
            {
                int i, n = 0;
                if (MessageBoxW(hwnd, L"确定删除所有已完成的日程吗？", L"清除已完成",
                                MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) != IDYES) break;
                for (i = model_count() - 1; i >= 0; i--) {
                    Schedule *s = model_at(i);
                    if (s && s->done) {
                        if (g_pinId == s->id) g_pinId = 0;
                        model_remove(s->id);
                        n++;
                    }
                }
                model_save(model_path());
                main_rebuild_views();
                main_set_status(n ? L"已清除完成项" : L"没有已完成的日程");
            }
            break;
        case IDM_HELP_ABOUT: show_about(); break;
        case IDM_HELP_HELP:  show_about(); break;

        case IDC_LISTBOX:
            if (HIWORD(wp) == 9) {              /* 便笺板选中变化 */
                update_status_default();
                InvalidateRect(g_hPanel, NULL, FALSE);
            }
            return 0;

        case IDC_WEEKVIEW:
            switch (HIWORD(wp)) {
            case 0: case 3:                       /* 双击/回车条目 */
                do_edit((int)lp);
                break;
            case 1:                               /* 双击空白格子 → 新建到该时段 */
                {
                    int day = (int)(short)LOWORD((DWORD)lp);
                    int hour = (int)(short)HIWORD((DWORD)lp);
                    SYSTEMTIME st;
                    long long ord = week_monday(g_hWeek);
                    int y, m, d;
                    ord += day;
                    civil_from_days(ord, &y, &m, &d);
                    memset(&st, 0, sizeof(st));
                    st.wYear = (WORD)y; st.wMonth = (WORD)m; st.wDay = (WORD)d;
                    st.wHour = (WORD)hour; st.wMinute = 0; st.wSecond = 0;
                    do_new_at(&st);
                }
                break;
            case 2:                               /* 右键 */
                {
                    POINT p;
                    int id = week_sel_id(g_hWeek);
                    p.x = (LONG)(short)LOWORD((DWORD)lp);
                    p.y = (LONG)(short)HIWORD((DWORD)lp);
                    show_context_menu(id, p);
                }
                break;
            case 6:                               /* 便笺板右键 */
                {
                    POINT p;
                    p.x = (LONG)(short)LOWORD((DWORD)lp);
                    p.y = (LONG)(short)HIWORD((DWORD)lp);
                    show_context_menu(stickies_sel(g_hBoard), p);
                }
                break;
            case 4:                               /* Delete */
                do_delete(week_sel_id(g_hWeek));
                break;
            }
            return 0;

        default: break;
        }
        return 0;

    case WM_NOTIFY:
        {
            NMHDR *nh = (NMHDR *)lp;
            if (nh->idFrom == IDC_LISTVIEW) {
                switch (nh->code) {
                case LVN_ITEMCHANGED:
                    {
                        NMLISTVIEW *nv = (NMLISTVIEW *)lp;
                        if (!g_loading &&
                            (nv->uNewState & LVIS_STATEIMAGEMASK) !=
                            (nv->uOldState & LVIS_STATEIMAGEMASK) &&
                            nv->iItem >= 0) {
                            int id = row_id(nv->iItem);
                            Schedule *s = model_by_id(id);
                            if (s) {
                                int chk = ((nv->uNewState & LVIS_STATEIMAGEMASK) >> 12) == 2;
                                if ((int)!!s->done != chk) {
                                    s->done = chk;
                                    model_save(model_path());
                                    model_refresh_next();
                                    main_set_status(chk ? L"已标记为完成" : L"已恢复为未完成");
                                    InvalidateRect(g_hPanel, NULL, FALSE);
                                    InvalidateRect(g_hWeek, NULL, FALSE);
                                }
                            }
                        }
                        update_status_default();
                    }
                    break;
                case NM_DBLCLK:
                    {
                        NMITEMACTIVATE *ia = (NMITEMACTIVATE *)lp;
                        if (ia->iItem >= 0) {
                            int id = row_id(ia->iItem);
                            if (ia->iSubItem == COL_ATT) do_open_attach(id, 0);
                            else do_edit(id);
                        }
                    }
                    break;
                case NM_RCLICK:
                    {
                        NMITEMACTIVATE *ia = (NMITEMACTIVATE *)lp;
                        POINT p;
                        int id = (ia->iItem >= 0) ? row_id(ia->iItem) : 0;
                        if (id) {
                            ListView_SetItemState(g_hTable, ia->iItem,
                                                  LVIS_SELECTED | LVIS_FOCUSED,
                                                  LVIS_SELECTED | LVIS_FOCUSED);
                        }
                        GetCursorPos(&p);
                        show_context_menu(id, p);
                    }
                    break;
                case LVN_KEYDOWN:
                    {
                        NMLVKEYDOWN *kd = (NMLVKEYDOWN *)lp;
                        if (kd->wVKey == VK_DELETE) do_delete(main_selected_id());
                        else if (kd->wVKey == VK_RETURN) do_edit(main_selected_id());
                        else if (kd->wVKey == VK_F2) begin_rename();
                        else if (kd->wVKey == VK_SPACE) do_toggle_done(main_selected_id());
                    }
                    break;
                case LVN_COLUMNCLICK:
                    {
                        NMLISTVIEW *nv = (NMLISTVIEW *)lp;
                        if (nv->iSubItem == COL_TITLE) g_sortMode = SORT_TITLE;
                        else if (nv->iSubItem == COL_START) g_sortMode = SORT_START;
                        else g_sortMode = SORT_TIME;
                        main_rebuild_views();
                    }
                    break;
                }
            }
        }
        return 0;

    case WM_CLOSE:
        if (g_hRename) { DestroyWindow(g_hRename); g_hRename = NULL; }
        model_save(model_path());
        note_save_state();
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        tray_remove();
        note_save_state();
        paper_free();
        destroy_resources();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main_handle_shortcut(MSG *msg, int isDialogMsg)
{
    if (isDialogMsg) return 0;
    if (msg->message == WM_KEYDOWN) {
        HWND h = msg->hwnd;
        if (g_hRename && h == g_hRename) return 0;
        if (h != g_hMain && h != g_hList && h != g_hTable && h != g_hWeek &&
            GetParent(h) != g_hMain && GetParent(h) != g_hTable) return 0;
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            switch (msg->wParam) {
            case 'N': do_new_at(NULL); return 1;
            case 'S':
                if (model_save(model_path())) main_set_status(L"已保存到数据文件");
                return 1;
            case 'D': do_copy(main_selected_id()); return 1;
            case 'C':
                if (h == g_hSearch) break;    /* 搜索框里 Ctrl+C 仍是复制文字 */
                do_copy(main_selected_id());
                return 1;
            case 'V':
                if (h == g_hSearch) break;    /* 搜索框里 Ctrl+V 仍是粘贴文字 */
                do_paste();
                return 1;
            case 'B': do_note(main_selected_id(), 0); return 1;
            case '1': set_view(ZS_VIEW_LIST); return 1;
            case '2': set_view(ZS_VIEW_TABLE); return 1;
            case '3': set_view(ZS_VIEW_WEEK); return 1;
            case 'T': week_goto_today(g_hWeek); return 1;
            case 'F': do_search_focus(); return 1;
            }
        } else {
            switch (msg->wParam) {
            case VK_DELETE:
                if (g_view == ZS_VIEW_WEEK) do_delete(week_sel_id(g_hWeek));
                else do_delete(main_selected_id());
                return 1;
            case VK_RETURN: do_edit(main_selected_id()); return 1;
            case VK_F2:     begin_rename(); return 1;
            case VK_SPACE:
                if (g_view != ZS_VIEW_WEEK) { do_toggle_done(main_selected_id()); return 1; }
                break;
            case VK_F5:     refresh_from_disk(); main_set_status(L"已重新载入"); return 1;
            case VK_F1:     show_about(); return 1;
            case VK_ESCAPE:
                /* Esc：清空表格搜索（在搜索框里或表格视图里都行） */
                if (g_hSearch && (h == g_hSearch || (g_view == ZS_VIEW_TABLE && g_search[0]))) {
                    SetWindowTextW(g_hSearch, L"");
                    return 1;
                }
                break;
            case VK_PRIOR:
                if (g_view == ZS_VIEW_LIST && (h == g_hList || h == g_hMain)) {
                    stickies_page_set(g_hBoard, stickies_page_get(g_hBoard) - 1);
                    return 1;
                }
                break;
            case VK_NEXT:
                if (g_view == ZS_VIEW_LIST && (h == g_hList || h == g_hMain)) {
                    stickies_page_set(g_hBoard, stickies_page_get(g_hBoard) + 1);
                    return 1;
                }
                break;
            }
        }
    }
    return 0;
}

/* ======================================================================
 *  创建主窗口
 * ====================================================================*/
void main_register_classes(HINSTANCE hInst)
{
    WNDCLASSEXW wc;

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = main_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = MAIN_CLASS;
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(APP_ICON_ID), IMAGE_ICON,
                                 GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
    wc.hIconSm = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(APP_ICON_ID), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    if (!wc.hIconSm) wc.hIconSm = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = panel_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = PANEL_CLASS;
    RegisterClassExW(&wc);

    flat_register_class(hInst);
    stickies_register_class(hInst);
    week_register_class(hInst);
    iv_register_class(hInst);
    note_register_class(hInst);
    remp_register(hInst);
    edit_register_class(hInst);
}

HWND main_create(void)
{
    HWND h;
    RECT rc = { 0, 0, 1220, 800 };
    HIMAGELIST hil;

    create_resources();
    paper_init();
    g_rowH = g_lhUI + 10;

    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, TRUE, 0);
    h = CreateWindowExW(0, MAIN_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT,
                        rc.right - rc.left, rc.bottom - rc.top,
                        NULL, NULL, g_hInst, NULL);
    if (!h) return NULL;
    g_hMain = h;
    note_set_main(h);

    /* 工具条按钮（扁平自绘） */
    g_btnNew     = flat_create(h, L"＋ 新建日程", IDC_BTN_NEW, 1, 0);
    g_btnEdit    = flat_create(h, L"编辑",        IDC_BTN_EDIT, 0, 0);
    g_btnDel     = flat_create(h, L"删除",        IDC_BTN_DEL, 0, 0);
    g_btnCopy    = flat_create(h, L"复制",        IDC_BTN_COPY, 0, 0);
    g_btnPaste   = flat_create(h, L"粘贴",        IDC_BTN_PASTE, 0, 0);
    g_btnNote    = flat_create(h, L"便笺",        IDC_BTN_NOTE, 0, 0);
    g_btnList    = flat_create(h, L"清单",        IDC_BTN_LIST, 0, 1);
    g_btnTable   = flat_create(h, L"表格",        IDC_BTN_TABLE, 0, 1);
    g_btnWeek    = flat_create(h, L"周表",        IDC_BTN_WEEK, 0, 1);
    g_btnPin     = flat_create(h, L"倒计时目标",  IDC_BTN_PIN, 0, 0);
    g_btnRefresh = flat_create(h, L"刷新",        IDC_BTN_REFRESH, 0, 0);
    if (g_btnPaste) EnableWindow(g_btnPaste, FALSE);   /* 剪贴板为空时置灰 */
    flat_check(g_btnList, 1);

    /* 倒计时面板 */
    g_hPanel = CreateWindowExW(0, PANEL_CLASS, L"", WS_CHILD | WS_VISIBLE,
                               0, 0, 10, 10, h, (HMENU)IDC_PANEL, g_hInst, NULL);

    /* 清单视图 */
    g_hBoard = stickies_create(h, g_hInst, IDC_LISTBOX);
    g_hList = g_hBoard;

    /* 表格视图 */
    g_hTable = CreateWindowExW(0, WC_LISTVIEWW, L"",
                               WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL |
                               LVS_SHOWSELALWAYS,
                               0, 0, 10, 10, h, (HMENU)IDC_LISTVIEW, g_hInst, NULL);
    ListView_SetExtendedListViewStyle(g_hTable,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES);
    ListView_SetBkColor(g_hTable, TH_SURFACE);
    ListView_SetTextBkColor(g_hTable, TH_SURFACE);
    ListView_SetTextColor(g_hTable, TH_TEXT);
    SendMessageW(g_hTable, WM_SETFONT, (WPARAM)g_fUI, TRUE);
    table_create_columns(g_hTable);
    /* 用图像列表把行高撑起来，观感更舒展 */
    hil = ImageList_Create(1, g_rowH, ILC_COLOR32, 1, 1);
    ListView_SetImageList(g_hTable, hil, LVSIL_SMALL);

    /* 周表格视图 */
    g_hWeek = week_create(h, g_hInst, IDC_WEEKVIEW);

    /* 搜索框：只在表格视图里出现（布局时放在表格正上方） */
    g_hSearch = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
                                0, 0, 10, 10, h, (HMENU)IDC_SEARCH, g_hInst, NULL);
    if (g_hSearch) {
        SendMessageW(g_hSearch, WM_SETFONT, (WPARAM)g_fUI, TRUE);
        SendMessageW(g_hSearch, EM_SETLIMITTEXT, 120, 0);
        SendMessageW(g_hSearch, EM_SETCUEBANNER, TRUE,
                     (LPARAM)L"搜索：标题 / 正文 / 链接（Esc 清空）");
        ShowWindow(g_hSearch, SW_HIDE);
    }

    /* 托盘图标（常驻：后台运行、打开便笺、停铃声都从这儿进） */
    tray_add();

    /* 状态栏 */
    g_hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
                                WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                0, 0, 10, 10, h, (HMENU)IDC_STATUS, g_hInst, NULL);
    SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_fSmall, TRUE);
    {
        int parts[2];
        RECT rcw;
        GetClientRect(h, &rcw);
        parts[0] = rcw.right - 420;
        parts[1] = -1;
        SendMessageW(g_hStatus, SB_SETPARTS, 2, (LPARAM)parts);
        SendMessageW(g_hStatus, SB_SETTEXTW, 1, (LPARAM)model_path());
    }

    ShowWindow(g_hTable, SW_HIDE);
    ShowWindow(g_hWeek, SW_HIDE);
    layout(h);
    main_rebuild_views();
    note_load_state();          /* 恢复上次打开的便笺 */
    SetTimer(h, 1, 250, NULL);
    return h;
}
