/* ============================================================================
 *  ZSchedule - weekview.c
 *  “按周 × 小时段”的网格表格视图：
 *    · 顶部工具条：上一周 / 本周 / 下一周 + 日期选择器（快速跳到某一周）
 *                 缩放 [−] 档位条 [+] [适]，档位可视化
 *    · 一列一天、一行一小时；半小时一条浅线
 *    · 日程按【真实时长】画成占用条（9:30–10:00 就只占半小时），
 *      相互重叠的日程自动分泳道并排
 *    · “适”= 自适应：把 24 小时压进当前可视高度，整周不用滚动
 * ==========================================================================*/
#include "zsched.h"
#include <windowsx.h>

#define WK_TOPBAR   42           /* 顶部工具条高度（逻辑值，不随缩放变化） */
#define WK_HEADER   54           /* 日期表头高度（随缩放） */
#define WK_GUTTER   62           /* 左侧时间刻度宽 */
#define WK_HOUR_H   46           /* 每小时基准行高 */
#define WK_MIN_CHIP 16           /* 占用条最小高度（逻辑值） */
#define WK_MAX_DAY  64           /* 每天最多渲染的条目数 */
#define WK_DTP_ID   7001

typedef struct {
    int      id;
    time_t   when;
    int      hour, minute;
    int      startMin, endMin;   /* 当天内的分钟数 */
    int      lane, lanes;        /* 重叠排布 */
} WkItem;

typedef struct {
    long long  weekMon;
    int        selId;
    int        zoomPct;          /* 60 / 80 / 100 / 130 / 170 */
    int        fit;              /* 1 = 自适应高度 */
    int        hotBar;           /* 顶部工具条悬停区域 */
    int        hourH, headH, gutterW, topH;
    int        scrollY, contentH;
    HFONT      hFont, hSmall, hTiny, hHead, hBtn;
    HBRUSH     brSurface;
    HWND       hwnd, hDtp;
} WeekState;

static WeekState *g_week;
static void wk_scroll_to_now(WeekState *st);
static const wchar_t *kDayName[7] = { L"周一", L"周二", L"周三", L"周四",
                                      L"周五", L"周六", L"周日" };
static const int kZoomSteps[5] = { 60, 80, 100, 130, 170 };

/* ======================================================================
 *  尺寸
 * ====================================================================*/
static int wk_dpi(HWND h)
{
    HDC hdc = GetDC(h);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(h, hdc);
    return dpi;
}

static void wk_metrics(WeekState *st)
{
    int dpi = wk_dpi(st->hwnd);
    RECT rc;
    st->topH = MulDiv(WK_TOPBAR, dpi, 96);
    st->headH = MulDiv(WK_HEADER, dpi, 96);
    st->gutterW = MulDiv(WK_GUTTER, dpi, 96);
    if (st->fit) {
        int avail;
        GetClientRect(st->hwnd, &rc);
        avail = rc.bottom - st->topH - st->headH;
        st->hourH = avail > 0 ? avail / 24 : 18;
    } else {
        int base = MulDiv(WK_HOUR_H, dpi, 96);      /* 每小时基准行高（设备像素） */
        st->hourH = base * st->zoomPct / 100;       /* 再乘缩放档位百分比 */
    }
    if (st->hourH < 12) st->hourH = 12;
    st->contentH = st->hourH * 24;
}

static void wk_bar_rects(WeekState *st, RECT *prev, RECT *today, RECT *next,
                         RECT *dtp, RECT *zbar, RECT *fit)
{
    int dpi = wk_dpi(st->hwnd);
    int h = MulDiv(26, dpi, 96);
    int y = st->topH / 2 - h / 2;
    int x = MulDiv(8, dpi, 96);
    int bw = MulDiv(30, dpi, 96);
    RECT rc;
    GetClientRect(st->hwnd, &rc);

    prev->left = x; prev->right = x + bw;  prev->top = y; prev->bottom = y + h;
    x += bw + MulDiv(4, dpi, 96);
    today->left = x; today->right = x + MulDiv(58, dpi, 96);
    today->top = y; today->bottom = y + h;
    x = today->right + MulDiv(4, dpi, 96);
    next->left = x; next->right = x + bw;  next->top = y; next->bottom = y + h;

    x = next->right + MulDiv(10, dpi, 96);
    dtp->left = x; dtp->right = x + MulDiv(132, dpi, 96);
    dtp->top = y; dtp->bottom = y + h;

    {
        int fw = MulDiv(30, dpi, 96);
        int zw = MulDiv(24, dpi, 96);
        int barW = MulDiv(9, dpi, 96);
        int gap = MulDiv(3, dpi, 96);
        int right = rc.right - MulDiv(10, dpi, 96);
        fit->right = right; fit->left = right - fw;
        fit->top = y; fit->bottom = y + h;
        right = fit->left - MulDiv(8, dpi, 96);
        zbar->right = right;
        zbar->left = right - (zw + (barW + gap) * 5 + zw);
        zbar->top = y; zbar->bottom = y + h;
    }
}

/* 返回：1 上一周 2 本周 3 下一周 4 缩小 5 放大 6 适配 0 无 */
static int wk_hit_bar(WeekState *st, int x, int y)
{
    RECT a, b, c, d, e, f;
    if (y >= st->topH) return 0;
    wk_bar_rects(st, &a, &b, &c, &d, &e, &f);
    if (y < a.top || y >= a.bottom) return 0;
    if (x >= a.left && x < a.right) return 1;
    if (x >= b.left && x < b.right) return 2;
    if (x >= c.left && x < c.right) return 3;
    if (x >= e.left && x < e.right) {
        int dpi = wk_dpi(st->hwnd);
        int zw = MulDiv(24, dpi, 96);
        if (x < e.left + zw) return 4;
        if (x >= e.right - zw) return 5;
        return 0;
    }
    if (x >= f.left && x < f.right && f.left > e.right) return 6;
    return 0;
}

static void wk_dayrect(WeekState *st, int day, RECT *r)
{
    RECT rc;
    int cw, dw;
    GetClientRect(st->hwnd, &rc);
    cw = rc.right - st->gutterW;
    if (cw < 7) cw = 7;
    dw = cw / 7;
    r->left = st->gutterW + day * dw;
    r->right = (day == 6) ? rc.right : r->left + dw;
    r->top = st->topH + st->headH;
    r->bottom = rc.bottom;
}

static void wk_update_scroll(WeekState *st)
{
    RECT rc;
    SCROLLINFO si;
    int page;
    GetClientRect(st->hwnd, &rc);
    page = rc.bottom - st->topH - st->headH;
    if (page < 0) page = 0;
    memset(&si, 0, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = st->contentH > 0 ? st->contentH - 1 : 0;
    si.nPage = (UINT)page;
    si.nPos = st->scrollY;
    SetScrollInfo(st->hwnd, SB_VERT, &si, TRUE);
}

static void wk_scroll_to_now(WeekState *st)
{
    SYSTEMTIME now;
    int hour;
    if (st->fit) { st->scrollY = 0; return; }
    GetLocalTime(&now);
    hour = now.wHour - 2;
    if (hour < 7) hour = 7;
    if (hour > 20) hour = 20;
    st->scrollY = hour * st->hourH;
}

/* ======================================================================
 *  收集某天的条目：算真实时长、重叠分道
 * ====================================================================*/
static int wk_collect(WeekState *st, int day, WkItem *out, int maxOut)
{
    long long ord = st->weekMon + day;
    int i, n = 0;
    int laneEnd[16];
    int lanes = 0;

    for (i = 0; i < model_count() && n < maxOut; i++) {
        Schedule *s = model_at(i);
        time_t t;
        struct tm *pt;
        if (!s) continue;
        t = sched_occurrence_on(s, ord);
        if (t <= 0) continue;
        pt = localtime(&t);
        if (!pt) continue;
        out[n].id = s->id;
        out[n].when = t;
        out[n].hour = pt->tm_hour;
        out[n].minute = pt->tm_min;
        out[n].startMin = pt->tm_hour * 60 + pt->tm_min;
        {
            int durMin = 30;                     /* 未设结束时间就按半小时算 */
            if (s->hasEnd && s->durSec > 0) {
                durMin = (int)(s->durSec / 60);
                if (durMin < 5) durMin = 5;
            }
            out[n].endMin = out[n].startMin + durMin;
            if (out[n].endMin > 24 * 60) out[n].endMin = 24 * 60;
        }
        out[n].lane = 0;
        out[n].lanes = 1;
        n++;
    }
    for (i = 1; i < n; i++) {
        WkItem key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].startMin > key.startMin) { out[j + 1] = out[j]; j--; }
        out[j + 1] = key;
    }
    for (i = 0; i < 16; i++) laneEnd[i] = -1;
    for (i = 0; i < n; i++) {
        int l;
        out[i].lane = 15;
        for (l = 0; l < 16; l++) {
            if (laneEnd[l] <= out[i].startMin) {
                out[i].lane = l;
                laneEnd[l] = out[i].endMin;
                if (l + 1 > lanes) lanes = l + 1;
                break;
            }
        }
    }
    for (i = 0; i < n; i++) out[i].lanes = lanes > 0 ? lanes : 1;
    return n;
}

/* ======================================================================
 *  顶部工具条
 * ====================================================================*/
static void wk_draw_bar(HDC hdc, WeekState *st, int cw)
{
    RECT prev, today, next, dtp, zbar, fit;
    int dpi = wk_dpi(st->hwnd);
    int i;
    RECT full = { 0, 0, cw, st->topH };

    paper_fill(hdc, &full, paper_shade(TH_BG, -6), 0);
    {
        HPEN pen = CreatePen(PS_SOLID, 1, TH_LINE);
        HPEN op = (HPEN)SelectObject(hdc, pen);
        MoveToEx(hdc, 0, st->topH - 1, NULL);
        LineTo(hdc, cw, st->topH - 1);
        SelectObject(hdc, op);
        DeleteObject(pen);
    }
    wk_bar_rects(st, &prev, &today, &next, &dtp, &zbar, &fit);
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, st->hBtn);

    {
        RECT *rr[3];
        const wchar_t *tt[3];
        int hot[3];
        rr[0] = &prev;  tt[0] = L"\u25c0";          hot[0] = (st->hotBar == 1);
        rr[1] = &today; tt[1] = L"\u672c\u5468";    hot[1] = (st->hotBar == 2);
        rr[2] = &next;  tt[2] = L"\u25b6";          hot[2] = (st->hotBar == 3);
        for (i = 0; i < 3; i++) {
            RECT r = *rr[i];
            paper_card(hdc, &r, hot[i] ? TH_SURFACE : paper_shade(TH_BG, 4),
                       MulDiv(6, dpi, 96), hot[i] ? 3 : 1, TH_LINE);
            SetTextColor(hdc, TH_TEXT);
            DrawTextW(hdc, tt[i], -1, &r,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }

    {
        int zw = MulDiv(24, dpi, 96);
        int barW = MulDiv(9, dpi, 96);
        int gap = MulDiv(3, dpi, 96);
        int cur = -1;
        RECT r;
        for (i = 0; i < 5; i++)
            if (kZoomSteps[i] == st->zoomPct) { cur = i; break; }
        r = zbar;
        paper_card(hdc, &r, paper_shade(TH_BG, 4), MulDiv(6, dpi, 96), 1, TH_LINE);
        {
            RECT m = zbar;
            m.right = m.left + zw;
            SetTextColor(hdc, st->hotBar == 4 ? TH_ACCENT : TH_TEXT_2);
            DrawTextW(hdc, L"\u2212", -1, &m,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        for (i = 0; i < 5; i++) {
            RECT b = zbar;
            b.left = zbar.left + zw + i * (barW + gap);
            b.right = b.left + barW;
            b.top = zbar.top + MulDiv(5, dpi, 96) + (4 - i) * MulDiv(2, dpi, 96);
            b.bottom = zbar.bottom - MulDiv(5, dpi, 96);
            {
                COLORREF c = st->fit ? TH_LINE
                           : (i <= cur ? TH_ACCENT : paper_shade(TH_BG, -18));
                HBRUSH hb = CreateSolidBrush(c);
                HBRUSH ob = (HBRUSH)SelectObject(hdc, hb);
                HPEN op = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
                RoundRect(hdc, b.left, b.top, b.right, b.bottom, 3, 3);
                SelectObject(hdc, op);
                SelectObject(hdc, ob);
                DeleteObject(hb);
            }
        }
        {
            RECT m = zbar;
            m.left = zbar.right - zw;
            SetTextColor(hdc, st->hotBar == 5 ? TH_ACCENT : TH_TEXT_2);
            DrawTextW(hdc, L"+", -1, &m,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        {
            RECT m = fit;
            paper_card(hdc, &m, st->fit ? TH_SELECT : paper_shade(TH_BG, 4),
                       MulDiv(6, dpi, 96), st->hotBar == 6 ? 3 : 1,
                       st->fit ? TH_ACCENT : TH_LINE);
            SetTextColor(hdc, st->fit ? TH_ACCENT : TH_TEXT_2);
            DrawTextW(hdc, L"\u9002", -1, &m,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        {
            wchar_t zb[32];
            WBuf b;
            RECT t = zbar;
            t.right = zbar.left - MulDiv(6, dpi, 96);
            t.left = t.right - MulDiv(84, dpi, 96);
            SelectObject(hdc, st->hSmall);
            SetTextColor(hdc, TH_TEXT_2);
            wb_init(&b, zb, 32);
            if (st->fit) wb_add(&b, L"自适应");
            else {
                wb_add(&b, L"缩放 ");
                wb_int(&b, st->zoomPct);
                wb_add(&b, L"%");
            }
            DrawTextW(hdc, zb, -1, &t, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
    }
    (void)dtp;
}

/* ======================================================================
 *  绘制
 * ====================================================================*/
static void wk_paint(HWND hwnd)
{
    WeekState *st = g_week;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    HDC mem;
    HBITMAP bmp, oldBmp;
    int cw, ch, day, dpi;
    long long today = today_ord();
    int nowMin;
    {
        SYSTEMTIME n;
        GetLocalTime(&n);
        nowMin = n.wHour * 60 + n.wMinute;
    }

    if (!st) { EndPaint(hwnd, &ps); return; }
    GetClientRect(hwnd, &rc);
    cw = rc.right; ch = rc.bottom;
    if (cw <= 0 || ch <= 0) { EndPaint(hwnd, &ps); return; }
    dpi = wk_dpi(hwnd);

    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, cw, ch);
    oldBmp = (HBITMAP)SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);
    {
        RECT all = { 0, 0, cw, ch };
        paper_fill(mem, &all, TH_SURFACE, 0);
    }

    /* ---------- 日期表头 ---------- */
    {
        RECT head = { 0, st->topH, cw, st->topH + st->headH };
        paper_fill(mem, &head, paper_shade(TH_SURFACE, -3), 0);
        SelectObject(mem, st->hHead);
        for (day = 0; day < 7; day++) {
            RECT dr;
            long long ord = st->weekMon + day;
            int y, m, d, isToday;
            wchar_t line1[32], line2[32];
            WBuf b;
            wk_dayrect(st, day, &dr);
            dr.top = st->topH;
            dr.bottom = st->topH + st->headH;
            civil_from_days(ord, &y, &m, &d);
            isToday = (ord == today);
            wb_init(&b, line1, 32); wb_add(&b, kDayName[day]);
            wb_init(&b, line2, 32);
            wb_u2(&b, (unsigned)m); wb_ch(&b, L'\u6708');
            wb_u2(&b, (unsigned)d); wb_add(&b, L"\u65e5");
            if (isToday) {
                RECT hl = dr;
                hl.top += MulDiv(6, dpi, 96);
                hl.bottom -= MulDiv(6, dpi, 96);
                hl.left += 4; hl.right -= 4;
                paper_card(mem, &hl, TH_SELECT, 8, 0, TH_ACCENT);
            }
            SetTextColor(mem, isToday ? TH_ACCENT : TH_TEXT);
            {
                RECT t = dr;
                t.top += MulDiv(8, dpi, 96);
                t.bottom = t.top + st->headH / 2;
                DrawTextW(mem, line1, -1, &t, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            SelectObject(mem, st->hSmall);
            SetTextColor(mem, isToday ? TH_ACCENT : TH_TEXT_2);
            {
                RECT t = dr;
                t.top = dr.top + st->headH / 2;
                t.bottom = dr.bottom - MulDiv(4, dpi, 96);
                DrawTextW(mem, line2, -1, &t, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
            SelectObject(mem, st->hHead);
            {
                HPEN pen = CreatePen(PS_SOLID, 1, TH_LINE_SOFT);
                HPEN op = (HPEN)SelectObject(mem, pen);
                MoveToEx(mem, dr.left, st->topH, NULL);
                LineTo(mem, dr.left, ch);
                SelectObject(mem, op);
                DeleteObject(pen);
            }
        }
        {
            HPEN pen = CreatePen(PS_SOLID, 1, TH_LINE);
            HPEN op = (HPEN)SelectObject(mem, pen);
            MoveToEx(mem, 0, st->topH + st->headH - 1, NULL);
            LineTo(mem, cw, st->topH + st->headH - 1);
            SelectObject(mem, op);
            DeleteObject(pen);
        }
    }

    /* ---------- 时间刻度与网格 ---------- */
    {
        int y0 = st->topH + st->headH - st->scrollY;
        int h;
        for (h = 0; h < 24; h++) {
            int y = y0 + h * st->hourH;
            int ym = y + st->hourH / 2;
            if (y >= st->topH + st->headH && y <= ch) {
                HPEN pen = CreatePen(PS_SOLID, 1, TH_LINE);
                HPEN op = (HPEN)SelectObject(mem, pen);
                MoveToEx(mem, 0, y, NULL);
                LineTo(mem, cw, y);
                SelectObject(mem, op);
                DeleteObject(pen);
            }
            if (ym >= st->topH + st->headH && ym <= ch) {
                HPEN pen = CreatePen(PS_SOLID, 1, TH_LINE_SOFT);
                HPEN op = (HPEN)SelectObject(mem, pen);
                MoveToEx(mem, st->gutterW, ym, NULL);
                LineTo(mem, cw, ym);
                SelectObject(mem, op);
                DeleteObject(pen);
            }
        }
        {
            RECT g = { 0, st->topH + st->headH, st->gutterW, ch };
            paper_fill(mem, &g, TH_SURFACE, 0);
            SelectObject(mem, st->hSmall);
            for (h = 0; h < 24; h++) {
                int y = y0 + h * st->hourH;
                wchar_t buf[16];
                WBuf b;
                RECT t;
                if (y < st->topH + st->headH || y > ch) continue;
                SetTextColor(mem, TH_TEXT_3);
                wb_init(&b, buf, 16);
                wb_u2(&b, (unsigned)h); wb_add(&b, L":00");
                t.left = 0; t.right = st->gutterW - 10;
                t.top = y + 2; t.bottom = t.top + 16;
                DrawTextW(mem, buf, -1, &t, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
    }

    /* ---------- 日程占用条 ---------- */
    for (day = 0; day < 7; day++) {
        WkItem items[WK_MAX_DAY];
        int n = wk_collect(st, day, items, WK_MAX_DAY);
        RECT dr;
        int i;
        int minH = MulDiv(WK_MIN_CHIP, dpi, 96);
        wk_dayrect(st, day, &dr);
        for (i = 0; i < n; i++) {
            Schedule *sc = model_by_id(items[i].id);
            int y, hgt, x, w, laneW;
            RECT chip;
            if (!sc) continue;
            y = st->topH + st->headH - st->scrollY
                + (int)((long long)items[i].startMin * st->hourH / 60);
            hgt = (int)((long long)(items[i].endMin - items[i].startMin)
                        * st->hourH / 60);
            if (hgt < minH) hgt = minH;
            laneW = (dr.right - dr.left - 8) / (items[i].lanes > 0 ? items[i].lanes : 1);
            if (laneW < 30) laneW = 30;
            x = dr.left + 4 + items[i].lane * laneW;
            w = laneW - 3;
            chip.left = x; chip.top = y;
            chip.right = x + w; chip.bottom = y + hgt;
            if (chip.bottom < st->topH + st->headH || chip.top > ch) continue;

            {
                COLORREF base = sc->done ? RGB(158, 150, 138) : sc->color;
                int selected = (sc->id == st->selId);
                COLORREF fill = RGB((GetRValue(base) + 255 * 6) / 7,
                                    (GetGValue(base) + 255 * 6) / 7,
                                    (GetBValue(base) + 255 * 6) / 7);
                paper_card(mem, &chip, fill, MulDiv(5, dpi, 96), 0,
                           selected ? base : paper_shade(fill, -24));
                {
                    RECT sb = chip;
                    sb.left += 1;
                    sb.right = sb.left + MulDiv(3, dpi, 96);
                    sb.top += 2; sb.bottom -= 2;
                    HBRUSH hb = CreateSolidBrush(base);
                    HBRUSH ob = (HBRUSH)SelectObject(mem, hb);
                    HPEN op = (HPEN)SelectObject(mem, GetStockObject(NULL_PEN));
                    RoundRect(mem, sb.left, sb.top, sb.right, sb.bottom, 3, 3);
                    SelectObject(mem, op);
                    SelectObject(mem, ob);
                    DeleteObject(hb);
                }
            }
            {
                wchar_t tb[220], tt[48];
                WBuf b;
                RECT t = chip;
                int lh = MulDiv(14, dpi, 96);
                int avail;
                SelectObject(mem, st->hTiny);
                SetTextColor(mem, RGB(52, 44, 34));
                wb_init(&b, tt, 48);
                wb_u2(&b, (unsigned)items[i].hour);
                wb_ch(&b, L':');
                wb_u2(&b, (unsigned)items[i].minute);
                if (items[i].endMin > items[i].startMin &&
                    items[i].endMin != items[i].startMin + 30) {
                    wb_add(&b, L"\u2013");
                    wb_u2(&b, (unsigned)(items[i].endMin / 60));
                    wb_ch(&b, L':');
                    wb_u2(&b, (unsigned)(items[i].endMin % 60));
                }
                wb_init(&b, tb, 220);
                if (sc->kind == KIND_ANNIV) wb_add(&b, L"\u25c7 ");
                wb_add(&b, sc->title);
                t.left += MulDiv(8, dpi, 96);
                t.right -= MulDiv(4, dpi, 96);
                t.top += 1;
                t.bottom -= 1;
                avail = t.bottom - t.top;
                if (t.right > t.left + 8) {
                    if (avail >= lh * 2) {
                        /* 够高：第一行时间区间，下面写标题 */
                        RECT r1 = t;
                        r1.bottom = r1.top + lh;
                        DrawTextW(mem, tt, -1, &r1,
                                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS |
                                  DT_NOPREFIX);
                        r1.top = r1.bottom;
                        r1.bottom = t.bottom;
                        DrawTextW(mem, tb, -1, &r1,
                                  DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS |
                                  DT_NOPREFIX);
                    } else {
                        /* 只有一行高：优先显示标题，时间由位置体现 */
                        DrawTextW(mem, tb, -1, &t,
                                  DT_LEFT | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS |
                                  DT_NOPREFIX);
                    }
                }
            }
        }
    }

    /* ---------- 当前时刻指示线 ---------- */
    {
        int wd = weekday_of_day(today);
        if (today >= st->weekMon && today < st->weekMon + 7) {
            RECT dr;
            int y = st->topH + st->headH - st->scrollY
                    + (int)((long long)nowMin * st->hourH / 60);
            wk_dayrect(st, wd, &dr);
            if (y >= st->topH + st->headH && y <= ch) {
                HPEN np = CreatePen(PS_SOLID, 2, TH_DANGER);
                HPEN op = (HPEN)SelectObject(mem, np);
                MoveToEx(mem, dr.left + 2, y, NULL);
                LineTo(mem, dr.right - 2, y);
                SelectObject(mem, op);
                DeleteObject(np);
                {
                    HBRUSH hb = CreateSolidBrush(TH_DANGER);
                    HBRUSH ob = (HBRUSH)SelectObject(mem, hb);
                    HPEN op2 = (HPEN)SelectObject(mem, GetStockObject(NULL_PEN));
                    Ellipse(mem, dr.left - 2, y - 4, dr.left + 6, y + 4);
                    SelectObject(mem, op2);
                    SelectObject(mem, ob);
                    DeleteObject(hb);
                }
            }
        }
    }

    wk_draw_bar(mem, st, cw);

    BitBlt(hdc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

/* ======================================================================
 *  命中
 * ====================================================================*/
static int wk_hit(WeekState *st, int x, int y, int *pDay, int *pId)
{
    RECT rc;
    int day;
    if (pDay) *pDay = -1;
    if (pId) *pId = 0;
    GetClientRect(st->hwnd, &rc);
    if (y < st->topH + st->headH) return 0;
    for (day = 0; day < 7; day++) {
        RECT dr;
        wk_dayrect(st, day, &dr);
        if (x >= dr.left && x < dr.right) break;
    }
    if (day >= 7) return 0;
    if (pDay) *pDay = day;
    {
        WkItem items[WK_MAX_DAY];
        int n = wk_collect(st, day, items, WK_MAX_DAY), i;
        RECT dr;
        int dpi = wk_dpi(st->hwnd);
        int minH = MulDiv(WK_MIN_CHIP, dpi, 96);
        wk_dayrect(st, day, &dr);
        for (i = 0; i < n; i++) {
            int cy = st->topH + st->headH - st->scrollY
                     + (int)((long long)items[i].startMin * st->hourH / 60);
            int hgt = (int)((long long)(items[i].endMin - items[i].startMin)
                            * st->hourH / 60);
            int laneW, cx;
            if (hgt < minH) hgt = minH;
            laneW = (dr.right - dr.left - 8) / (items[i].lanes > 0 ? items[i].lanes : 1);
            if (laneW < 30) laneW = 30;
            cx = dr.left + 4 + items[i].lane * laneW;
            if (y >= cy && y < cy + hgt && x >= cx && x < cx + laneW - 3) {
                if (pId) *pId = items[i].id;
                return 1;
            }
        }
    }
    return 0;
}

/* ======================================================================
 *  过程
 * ====================================================================*/
static void wk_apply_zoom(WeekState *st)
{
    int centerMin = 0;
    if (st->hourH > 0) centerMin = (st->scrollY + 100) * 60 / st->hourH;
    wk_metrics(st);
    if (!st->fit) {
        st->scrollY = centerMin * st->hourH / 60 - 100;
        if (st->scrollY < 0) st->scrollY = 0;
    }
    wk_update_scroll(st);
    InvalidateRect(st->hwnd, NULL, TRUE);
}

void week_zoom(HWND h, int delta)
{
    int cur = 2, i;
    (void)h;
    if (!g_week) return;
    for (i = 0; i < 5; i++)
        if (kZoomSteps[i] == g_week->zoomPct) { cur = i; break; }
    g_week->fit = 0;
    if (delta == 0) {
        cur = 2;
    } else {
        cur += delta;
        if (cur < 0) cur = 0;
        if (cur > 4) cur = 4;
    }
    g_week->zoomPct = kZoomSteps[cur];
    wk_apply_zoom(g_week);
}

static void wk_toggle_fit(HWND h)
{
    if (!g_week) return;
    g_week->fit = !g_week->fit;
    if (g_week->fit) g_week->scrollY = 0;
    wk_apply_zoom(g_week);
    (void)h;
}

static LRESULT CALLBACK week_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    WeekState *st = g_week;

    switch (msg) {
    case WM_CREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)g_week);
        return 0;

    case WM_SIZE:
        if (st) {
            RECT a, b, c, d, e, f;
            wk_metrics(st);
            wk_update_scroll(st);
            wk_bar_rects(st, &a, &b, &c, &d, &e, &f);
            if (st->hDtp)
                SetWindowPos(st->hDtp, NULL, d.left, d.top,
                             d.right - d.left, d.bottom - d.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
        wk_paint(hwnd);
        return 0;

    case WM_NOTIFY:
        if (st && lp) {
            NMHDR *nh = (NMHDR *)lp;
            if (nh->idFrom == WK_DTP_ID && nh->code == DTN_DATETIMECHANGE) {
                LPNMDATETIMECHANGE ch = (LPNMDATETIMECHANGE)lp;
                if (ch->dwFlags == GDT_VALID) {
                    st->weekMon = monday_of(civil_days(ch->st.wYear, ch->st.wMonth,
                                                       ch->st.wDay));
                    wk_scroll_to_now(st);
                    wk_update_scroll(st);
                    InvalidateRect(hwnd, NULL, TRUE);
                }
            }
        }
        return 0;

    case WM_VSCROLL:
        if (st) {
            SCROLLINFO si;
            int delta = 0;
            memset(&si, 0, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &si);
            switch (LOWORD(wp)) {
            case SB_LINEUP:   delta = -st->hourH / 2; break;
            case SB_LINEDOWN: delta = st->hourH / 2; break;
            case SB_PAGEUP:   delta = -(int)si.nPage; break;
            case SB_PAGEDOWN: delta = (int)si.nPage; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: delta = si.nTrackPos - si.nPos; break;
            default: break;
            }
            st->scrollY += delta;
            if (st->scrollY > st->contentH - (int)si.nPage)
                st->scrollY = st->contentH - (int)si.nPage;
            if (st->scrollY < 0) st->scrollY = 0;
            SetScrollPos(hwnd, SB_VERT, st->scrollY, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_MOUSEWHEEL:
        if (st && (GetKeyState(VK_CONTROL) & 0x8000)) {
            int d = GET_WHEEL_DELTA_WPARAM(wp);
            week_zoom(hwnd, d > 0 ? 1 : -1);
            return 0;
        }
        if (st && !st->fit) {
            int delta = GET_WHEEL_DELTA_WPARAM(wp);
            RECT rc;
            SCROLLINFO si;
            GetClientRect(hwnd, &rc);
            memset(&si, 0, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_PAGE;
            GetScrollInfo(hwnd, SB_VERT, &si);
            st->scrollY += (delta > 0 ? -st->hourH : st->hourH);
            if (st->scrollY > st->contentH - (int)si.nPage)
                st->scrollY = st->contentH - (int)si.nPage;
            if (st->scrollY < 0) st->scrollY = 0;
            SetScrollPos(hwnd, SB_VERT, st->scrollY, TRUE);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (st) {
            int hot = wk_hit_bar(st, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (hot != st->hotBar) {
                st->hotBar = hot;
                InvalidateRect(hwnd, NULL, FALSE);
            }
            SetCursor(LoadCursorW(NULL, hot ? IDC_HAND : IDC_ARROW));
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (st) {
            int bar = wk_hit_bar(st, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            SetFocus(hwnd);
            if (bar == 1) { week_shift(hwnd, -1); return 0; }
            if (bar == 2) { week_goto_today(hwnd); return 0; }
            if (bar == 3) { week_shift(hwnd, 1); return 0; }
            if (bar == 4) { week_zoom(hwnd, -1); return 0; }
            if (bar == 5) { week_zoom(hwnd, 1); return 0; }
            if (bar == 6) { wk_toggle_fit(hwnd); return 0; }
            {
                int id = 0;
                if (wk_hit(st, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), NULL, &id) && id) {
                    st->selId = id;
                    InvalidateRect(hwnd, NULL, FALSE);
                    PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_LISTBOX, 9), 0);
                }
            }
        }
        return 0;

    case WM_LBUTTONDBLCLK:
        if (st) {
            int day = -1, id = 0;
            int hit = wk_hit(st, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &day, &id);
            if (hit && id) {
                PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDM_EDIT_EDIT, 1),
                             (LPARAM)(INT_PTR)id);
            } else if (day >= 0) {
                int y = GET_Y_LPARAM(lp);
                int hour = (y - st->topH - st->headH + st->scrollY) / st->hourH;
                if (hour < 0) hour = 0;
                if (hour > 23) hour = 23;
                PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_WEEKVIEW, 1),
                             (LPARAM)MAKELONG(day, hour));
            }
        }
        return 0;

    case WM_RBUTTONDOWN:
        if (st) {
            int day = -1, id = 0;
            int hit = wk_hit(st, GET_X_LPARAM(lp), GET_Y_LPARAM(lp), &day, &id);
            POINT p;
            if (hit && id) st->selId = id;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            ClientToScreen(hwnd, &p);
            PostMessageW(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_WEEKVIEW, 2),
                         (LPARAM)MAKELONG(p.x, p.y));
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_KEYDOWN:
        if (st) {
            switch (wp) {
            case VK_PRIOR: week_shift(hwnd, -1); return 0;
            case VK_NEXT:  week_shift(hwnd, 1); return 0;
            case VK_HOME:  week_goto_today(hwnd); return 0;
            case VK_ADD: case VK_OEM_PLUS:       week_zoom(hwnd, 1); return 0;
            case VK_SUBTRACT: case VK_OEM_MINUS: week_zoom(hwnd, -1); return 0;
            case '0':      week_zoom(hwnd, 0); return 0;
            case 'F':      wk_toggle_fit(hwnd); return 0;
            case VK_DELETE:
                PostMessageW(GetParent(hwnd), WM_COMMAND,
                             MAKEWPARAM(IDC_WEEKVIEW, 4), 0);
                return 0;
            case VK_RETURN:
                if (st->selId)
                    PostMessageW(GetParent(hwnd), WM_COMMAND,
                                 MAKEWPARAM(IDM_EDIT_EDIT, 1), (LPARAM)(INT_PTR)st->selId);
                return 0;
            }
        }
        break;

    case WM_DESTROY:
        if (st) {
            if (st->hFont) DeleteObject(st->hFont);
            if (st->hSmall) DeleteObject(st->hSmall);
            if (st->hTiny) DeleteObject(st->hTiny);
            if (st->hHead) DeleteObject(st->hHead);
            if (st->hBtn) DeleteObject(st->hBtn);
            if (st->brSurface) DeleteObject(st->brSurface);
        }
        g_week = NULL;
        return 0;

    case WM_NCDESTROY:
        {
            WeekState *old = (WeekState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (old) free(old);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void week_register_class(HINSTANCE hInst)
{
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = week_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = WEEK_CLASS;
    RegisterClassExW(&wc);
}

static int wk_font_h(int pt, int dpi)
{
    return -MulDiv(pt, dpi, 72);
}

HWND week_create(HWND parent, HINSTANCE hInst, int id)
{
    WeekState *st = (WeekState *)calloc(1, sizeof(WeekState));
    HWND hwnd;
    int dpi;
    if (!st) return NULL;
    st->weekMon = monday_of(today_ord());
    st->zoomPct = 100;
    dpi = wk_dpi(parent);
    st->hFont = CreateFontW(wk_font_h(10, dpi), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    st->hSmall = CreateFontW(wk_font_h(9, dpi), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                             DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    st->hTiny = CreateFontW(wk_font_h(8, dpi), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    st->hHead = CreateFontW(wk_font_h(10, dpi), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                            DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    st->hBtn = CreateFontW(wk_font_h(9, dpi), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    st->brSurface = CreateSolidBrush(TH_SURFACE);
    g_week = st;

    hwnd = CreateWindowExW(0, WEEK_CLASS, L"",
                           WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
                           0, 0, 10, 10, parent, (HMENU)(INT_PTR)id, hInst, NULL);
    if (!hwnd) { free(st); g_week = NULL; return NULL; }
    st->hwnd = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);

    st->hDtp = CreateWindowExW(0, DATETIMEPICK_CLASSW, L"",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_SHORTDATEFORMAT,
                               0, 0, 10, 10, hwnd, (HMENU)(INT_PTR)WK_DTP_ID, hInst, NULL);
    if (st->hDtp) {
        SYSTEMTIME now;
        GetLocalTime(&now);
        DateTime_SetFormat(st->hDtp, L"yyyy-MM-dd");
        DateTime_SetSystemtime(st->hDtp, GDT_VALID, &now);
        SendMessageW(st->hDtp, WM_SETFONT, (WPARAM)st->hSmall, TRUE);
        /* 日历弹出层也调成纸质配色，避免突冗的系统蓝 */
        SendMessageW(st->hDtp, DTM_SETMCCOLOR, MCSC_BACKGROUND, (LPARAM)TH_SURFACE_2);
        SendMessageW(st->hDtp, DTM_SETMCCOLOR, MCSC_MONTHBK, (LPARAM)TH_SURFACE);
        SendMessageW(st->hDtp, DTM_SETMCCOLOR, MCSC_TEXT, (LPARAM)TH_TEXT);
        SendMessageW(st->hDtp, DTM_SETMCCOLOR, MCSC_TITLEBK, (LPARAM)RGB(74, 56, 44));
        SendMessageW(st->hDtp, DTM_SETMCCOLOR, MCSC_TITLETEXT, (LPARAM)RGB(246, 241, 231));
        SendMessageW(st->hDtp, DTM_SETMCCOLOR, MCSC_TRAILINGTEXT, (LPARAM)TH_TEXT_3);
    }
    {
        RECT a, b, c, d, e, f;
        wk_metrics(st);
        wk_bar_rects(st, &a, &b, &c, &d, &e, &f);
        if (st->hDtp)
            SetWindowPos(st->hDtp, NULL, d.left, d.top, d.right - d.left,
                         d.bottom - d.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    wk_scroll_to_now(st);
    return hwnd;
}

void week_refresh(HWND h)
{
    (void)h;
    if (!g_week) return;
    wk_metrics(g_week);
    wk_update_scroll(g_week);
    InvalidateRect(g_week->hwnd, NULL, TRUE);
}

void week_shift(HWND h, int weeks)
{
    (void)h;
    if (!g_week) return;
    g_week->weekMon += 7LL * weeks;
    if (g_week->hDtp) {
        int y, m, d;
        SYSTEMTIME st;
        civil_from_days(g_week->weekMon, &y, &m, &d);
        memset(&st, 0, sizeof(st));
        st.wYear = (WORD)y; st.wMonth = (WORD)m; st.wDay = (WORD)d;
        DateTime_SetSystemtime(g_week->hDtp, GDT_VALID, &st);
    }
    wk_scroll_to_now(g_week);
    wk_update_scroll(g_week);
    InvalidateRect(g_week->hwnd, NULL, TRUE);
}

void week_goto_today(HWND h)
{
    (void)h;
    if (!g_week) return;
    g_week->weekMon = monday_of(today_ord());
    if (g_week->hDtp) {
        SYSTEMTIME now;
        GetLocalTime(&now);
        DateTime_SetSystemtime(g_week->hDtp, GDT_VALID, &now);
    }
    wk_scroll_to_now(g_week);
    wk_update_scroll(g_week);
    InvalidateRect(g_week->hwnd, NULL, TRUE);
}

void week_goto_time(HWND h, const SYSTEMTIME *st)
{
    (void)h;
    if (!g_week || !st) return;
    g_week->weekMon = monday_of(civil_days(st->wYear, st->wMonth, st->wDay));
    wk_update_scroll(g_week);
    InvalidateRect(g_week->hwnd, NULL, TRUE);
}

void week_set_sel(HWND h, int id)
{
    (void)h;
    if (!g_week) return;
    g_week->selId = id;
    InvalidateRect(g_week->hwnd, NULL, FALSE);
}

int week_sel_id(HWND h)
{
    (void)h;
    return g_week ? g_week->selId : 0;
}

long long week_monday(HWND h)
{
    (void)h;
    if (!g_week) return monday_of(today_ord());
    return g_week->weekMon;
}
