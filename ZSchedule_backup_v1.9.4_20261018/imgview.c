/* ============================================================================
 *  ZSchedule - imgview.c
 *  原图查看器：以 100% 原始像素显示图片，支持缩放 / 适应窗口 / 平移 / 多图切换
 * ==========================================================================*/
#include "zsched.h"
#include <windowsx.h>

#define IV_MAX 64
#define IV_FOOTER 30

typedef struct {
    wchar_t  paths[IV_MAX][MAX_PATH];
    int      count;
    int      cur;
    double   zoom;          /* 1.0 = 原图 100% */
    int      fit;
    int      natW, natH;
    int      scrollX, scrollY;
    int      dragging;
    POINT    dragFrom;
    int      dragScrollX, dragScrollY;
    HFONT    hFont, hFontSmall;
    HBRUSH   brBg, brFooter;
    HWND     hwnd;
} IVState;

static IVState *g_iv;

/* ------------------------------------------------------------------ 布局 */
static void iv_metrics(IVState *st, int *pw, int *ph, int *fw, int *fh)
{
    RECT rc;
    GetClientRect(st->hwnd, &rc);
    if (fw) *fw = rc.right;
    if (fh) *fh = rc.bottom;
    if (pw) *pw = rc.right;
    if (ph) *ph = rc.bottom - IV_FOOTER;
}

/* 计算当前绘制尺寸 */
static void iv_drawsize(IVState *st, int *dw, int *dh)
{
    int cw, ch;
    iv_metrics(st, NULL, NULL, &cw, &ch);
    if (ch < 10) ch = 10;
    if (cw < 10) cw = 10;
    if (st->fit) {
        double sx, sy, s;
        if (st->natW <= 0 || st->natH <= 0) { *dw = 0; *dh = 0; return; }
        sx = (double)cw / st->natW;
        sy = (double)ch / st->natH;
        s = (sx < sy) ? sx : sy;
        if (s > 1.0) s = 1.0;
        *dw = (int)(st->natW * s + 0.5);
        *dh = (int)(st->natH * s + 0.5);
    } else {
        *dw = (int)(st->natW * st->zoom + 0.5);
        *dh = (int)(st->natH * st->zoom + 0.5);
    }
    if (*dw < 1) *dw = 1;
    if (*dh < 1) *dh = 1;
}

static void iv_update_scroll(IVState *st)
{
    int cw, ch, dw, dh, maxX, maxY;
    SCROLLINFO si;
    iv_metrics(st, NULL, NULL, &cw, &ch);
    iv_drawsize(st, &dw, &dh);
    maxX = dw - cw; if (maxX < 0) maxX = 0;
    maxY = dh - ch; if (maxY < 0) maxY = 0;
    if (st->scrollX > maxX) st->scrollX = maxX;
    if (st->scrollY > maxY) st->scrollY = maxY;
    if (st->scrollX < 0) st->scrollX = 0;
    if (st->scrollY < 0) st->scrollY = 0;

    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = dw > 0 ? dw - 1 : 0;
    si.nPage = (UINT)cw;
    si.nPos = st->scrollX;
    SetScrollInfo(st->hwnd, SB_HORZ, &si, TRUE);

    si.nMax = dh > 0 ? dh - 1 : 0;
    si.nPage = (UINT)ch;
    si.nPos = st->scrollY;
    SetScrollInfo(st->hwnd, SB_VERT, &si, TRUE);
}

static void iv_title(IVState *st)
{
    wchar_t name[MAX_PATH], buf[MAX_PATH + 96];
    WBuf b;
    int dw, dh;
    iv_drawsize(st, &dw, &dh);
    file_name_of(st->paths[st->cur], name, MAX_PATH);
    wb_init(&b, buf, MAX_PATH + 96);
    wb_add(&b, name);
    wb_add(&b, L"   (");
    wb_int(&b, st->cur + 1);
    wb_add(&b, L"/");
    wb_int(&b, st->count);
    wb_add(&b, L")    ");
    wb_int(&b, st->natW);
    wb_add(&b, L" × ");
    wb_int(&b, st->natH);
    wb_add(&b, L" 像素   ");
    if (st->fit) wb_add(&b, L"适应窗口");
    else {
        wb_int(&b, (long long)(st->zoom * 100 + 0.5));
        wb_add(&b, L"%");
    }
    SetWindowTextW(st->hwnd, buf);
}

static void iv_load_current(IVState *st)
{
    int w = 0, h = 0;
    if (!img_size(st->paths[st->cur], &w, &h)) {
        st->natW = st->natH = 0;
    } else {
        st->natW = w;
        st->natH = h;
    }
    st->fit = 1;                 /* 打开时先适应窗口 */
    st->zoom = 1.0;
    st->scrollX = st->scrollY = 0;
    iv_update_scroll(st);
    iv_title(st);
    InvalidateRect(st->hwnd, NULL, TRUE);
}

static void iv_zoom_to(IVState *st, double z)
{
    int cw, ch, ox, oy;
    double cx, cy;
    if (z < 0.05) z = 0.05;
    if (z > 16.0) z = 16.0;
    iv_metrics(st, NULL, NULL, &cw, &ch);
    /* 以视口中心为锚点缩放 */
    cx = (st->scrollX + cw / 2.0) / (st->natW * st->zoom);
    cy = (st->scrollY + ch / 2.0) / (st->natH * st->zoom);
    if (cx > 1) cx = 1;
    if (cy > 1) cy = 1;
    st->fit = 0;
    st->zoom = z;
    ox = (int)(cx * st->natW * z - cw / 2.0);
    oy = (int)(cy * st->natH * z - ch / 2.0);
    st->scrollX = ox > 0 ? ox : 0;
    st->scrollY = oy > 0 ? oy : 0;
    iv_update_scroll(st);
    iv_title(st);
    InvalidateRect(st->hwnd, NULL, TRUE);
}

/* ------------------------------------------------------------------ 绘制 */
static void iv_paint(HWND hwnd)
{
    IVState *st = g_iv;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc, foot;
    int cw, ch, dw, dh, x, y;
    HDC mem;
    HBITMAP bmp, oldBmp;

    if (!st) { EndPaint(hwnd, &ps); return; }
    iv_metrics(st, NULL, NULL, &cw, &ch);
    if (cw <= 0 || ch <= 0) { EndPaint(hwnd, &ps); return; }
    iv_drawsize(st, &dw, &dh);

    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, cw, ch);
    oldBmp = (HBITMAP)SelectObject(mem, bmp);

    rc.left = 0; rc.top = 0; rc.right = cw; rc.bottom = ch;
    FillRect(mem, &rc, st->brBg);

    if (st->natW > 0) {
        x = (dw <= cw) ? (cw - dw) / 2 : -st->scrollX;
        y = (dh <= ch) ? (ch - dh) / 2 : -st->scrollY;
        img_draw_zoom(mem, st->paths[st->cur], x, y, dw, dh,
                      st->zoom < 1.0 || st->fit);
    } else {
        const wchar_t *msg = L"无法读取该图片（文件可能已被移动或格式不受支持）";
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, RGB(150, 156, 166));
        SelectObject(mem, st->hFont);
        rc.top = ch / 2 - 12;
        DrawTextW(mem, msg, -1, &rc, DT_CENTER | DT_SINGLELINE);
    }

    /* 底部信息条 */
    foot.left = 0; foot.top = ch - IV_FOOTER; foot.right = cw; foot.bottom = ch;
    FillRect(mem, &foot, st->brFooter);
    {
        HBRUSH line = CreateSolidBrush(RGB(60, 66, 74));
        RECT l = foot; l.bottom = l.top + 1;
        FillRect(mem, &l, line);
        DeleteObject(line);
    }
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(178, 185, 195));
    SelectObject(mem, st->hFontSmall);
    {
        RECT t = foot;
        wchar_t hint[256];
        t.left += 12; t.right -= 12;
        wcscpy(hint, L"滚轮缩放 · 拖拽平移 · 方向键微调 · PgUp/PgDn 切换图片 · "
                     L"0 原图 100% · F 适应窗口 · Esc 关闭");
        DrawTextW(mem, hint, -1, &t, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
    {
        RECT t = foot;
        wchar_t buf[64];
        WBuf b;
        t.left = cw - 220; t.right = cw - 12;
        wb_init(&b, buf, 64);
        if (st->fit) wb_add(&b, L"适应窗口");
        else { wb_int(&b, (long long)(st->zoom * 100 + 0.5)); wb_add(&b, L"%"); }
        DrawTextW(mem, buf, -1, &t, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    BitBlt(hdc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

/* ------------------------------------------------------------------ 过程 */
static void iv_scroll_by(IVState *st, int dx, int dy)
{
    st->scrollX += dx;
    st->scrollY += dy;
    iv_update_scroll(st);
    InvalidateRect(st->hwnd, NULL, FALSE);
}

static void iv_nav(IVState *st, int delta)
{
    int n = st->cur + delta;
    if (n < 0) n = st->count - 1;
    if (n >= st->count) n = 0;
    st->cur = n;
    iv_load_current(st);
}

static LRESULT CALLBACK iv_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    IVState *st = g_iv;

    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)g_iv);
        return 0;

    case WM_SIZE:
        if (st) { iv_update_scroll(st); iv_title(st); InvalidateRect(hwnd, NULL, TRUE); }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        iv_paint(hwnd);
        return 0;

    case WM_GETDLGCODE:
        return DLGC_WANTARROWS | DLGC_WANTCHARS;

    case WM_HSCROLL:
    case WM_VSCROLL:
        if (st) {
            SCROLLINFO si;
            int pos, delta = 0;
            memset(&si, 0, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, msg == WM_HSCROLL ? SB_HORZ : SB_VERT, &si);
            pos = si.nPos;
            switch (LOWORD(wp)) {
            case SB_LINELEFT:  delta = -24; break;
            case SB_LINERIGHT: delta = 24; break;
            case SB_PAGELEFT:  delta = -(int)si.nPage; break;
            case SB_PAGERIGHT: delta = (int)si.nPage; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: delta = si.nTrackPos - pos; break;
            default: break;
            }
            if (msg == WM_HSCROLL) iv_scroll_by(st, delta, 0);
            else                   iv_scroll_by(st, 0, delta);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st) {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            if (GetKeyState(VK_CONTROL) & 0x8000) {
                double f = (delta > 0) ? 1.25 : 0.8;
                double base = st->fit ? (double)1.0 : st->zoom;
                if (st->fit) {
                    int dw, dh, cw, ch;
                    iv_metrics(st, NULL, NULL, &cw, &ch);
                    iv_drawsize(st, &dw, &dh);
                    base = (st->natW > 0) ? (double)dw / st->natW : 1.0;
                }
                iv_zoom_to(st, base * f);
            } else {
                iv_scroll_by(st, 0, (delta > 0 ? -60 : 60));
            }
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (st) {
            st->dragging = 1;
            st->dragFrom.x = GET_X_LPARAM(lp);
            st->dragFrom.y = GET_Y_LPARAM(lp);
            st->dragScrollX = st->scrollX;
            st->dragScrollY = st->scrollY;
            SetCapture(hwnd);
            SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
        }
        return 0;

    case WM_MOUSEMOVE:
        if (st && st->dragging) {
            int dx = GET_X_LPARAM(lp) - st->dragFrom.x;
            int dy = GET_Y_LPARAM(lp) - st->dragFrom.y;
            st->scrollX = st->dragScrollX - dx;
            st->scrollY = st->dragScrollY - dy;
            iv_update_scroll(st);
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (st && !st->dragging) {
            SetCursor(LoadCursorW(NULL, st->natW > 0 ? IDC_SIZEALL : IDC_ARROW));
        }
        return 0;

    case WM_LBUTTONUP:
        if (st) { st->dragging = 0; ReleaseCapture(); }
        return 0;

    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursorW(NULL, IDC_ARROW));
            return TRUE;
        }
        break;

    case WM_KEYDOWN:
        if (!st) break;
        switch (wp) {
        case VK_ESCAPE: DestroyWindow(hwnd); return 0;
        case VK_UP:     iv_scroll_by(st, 0, -40); return 0;
        case VK_DOWN:   iv_scroll_by(st, 0, 40); return 0;
        case VK_LEFT:   iv_scroll_by(st, -40, 0); return 0;
        case VK_RIGHT:  iv_scroll_by(st, 40, 0); return 0;
        case VK_PRIOR:  iv_nav(st, -1); return 0;
        case VK_NEXT:   iv_nav(st, 1); return 0;
        case VK_ADD: case VK_OEM_PLUS:    iv_zoom_to(st, st->zoom * 1.25); return 0;
        case VK_SUBTRACT: case VK_OEM_MINUS:
            iv_zoom_to(st, st->zoom * 0.8); return 0;
        case '0': st->fit = 0; iv_zoom_to(st, 1.0); return 0;
        case 'F': st->fit = 1; iv_update_scroll(st); iv_title(st);
                  InvalidateRect(hwnd, NULL, TRUE); return 0;
        case 'W': if (GetKeyState(VK_CONTROL) & 0x8000) { DestroyWindow(hwnd); return 0; }
                  break;
        }
        break;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (st) {
            if (st->hFont) DeleteObject(st->hFont);
            if (st->hFontSmall) DeleteObject(st->hFontSmall);
            if (st->brBg) DeleteObject(st->brBg);
            if (st->brFooter) DeleteObject(st->brFooter);
        }
        g_iv = NULL;
        return 0;

    case WM_NCDESTROY:
        {
            IVState *old = (IVState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (old) free(old);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void iv_register_class(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = iv_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = IMGV_CLASS;
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);
}

/* 打开（或复用）查看器窗口 */
HWND iv_open(HWND owner, const wchar_t *const *paths, int count, int startIndex)
{
    IVState *st;
    HWND hwnd;
    int i, n = 0;

    if (count <= 0) return NULL;

    if (g_iv && IsWindow(g_iv->hwnd)) {
        hwnd = g_iv->hwnd;
        st = g_iv;
    } else {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
        st = (IVState *)calloc(1, sizeof(IVState));
        if (!st) return NULL;
        st->hFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                DEFAULT_PITCH, L"Microsoft YaHei UI");
        st->hFontSmall = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                                     OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                     DEFAULT_PITCH, L"Microsoft YaHei UI");
        st->brBg = CreateSolidBrush(RGB(38, 41, 47));
        st->brFooter = CreateSolidBrush(RGB(46, 50, 57));
        g_iv = st;
        hwnd = CreateWindowExW(WS_EX_APPWINDOW, IMGV_CLASS, L"查看原图",
                               WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
                               CW_USEDEFAULT, CW_USEDEFAULT, 980, 720,
                               owner, NULL, hInst, NULL);
        if (!hwnd) { free(st); g_iv = NULL; return NULL; }
        st->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);
    }

    st->count = 0;
    for (i = 0; i < count && n < IV_MAX; i++) {
        if (!paths[i] || !paths[i][0]) continue;
        wcsncpy(st->paths[n], paths[i], MAX_PATH - 1);
        st->paths[n][MAX_PATH - 1] = 0;
        n++;
    }
    st->count = n;
    if (st->count <= 0) { DestroyWindow(hwnd); return NULL; }
    st->cur = startIndex;
    if (st->cur < 0 || st->cur >= st->count) st->cur = 0;

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    iv_load_current(st);
    return hwnd;
}
