/* ============================================================================
 *  ZSchedule - paper.c
 *  纸质书质感绘制原语：
 *    · 程序化生成的纸纤维纹理（按底色缓存成 pattern brush）
 *    · 带柔和投影的“纸卡”绘制（投影用多层渐隐矩形模拟）
 *    · 书页折角 / 信笺横格线 / 明暗调节
 *
 *  所有纹理都是运行时算出来的，不依赖任何外部图片资源。
 * ==========================================================================*/
#include "zsched.h"

#define TILE      96          /* 纹理瓦片边长 */
#define MAX_BRUSH 24

typedef struct {
    COLORREF base;
    HBRUSH   br;
} PaperBrush;

static PaperBrush g_pb[MAX_BRUSH];
static int        g_pbCount;
static unsigned   g_seed = 20240515u;

static unsigned pb_rand(void)
{
    g_seed = g_seed * 1103515245u + 12345u;
    return (g_seed >> 16) & 0x7FFF;
}

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* 在底色上叠加极轻的纤维噪点，得到一张瓦片位图 */
static HBITMAP make_tile(COLORREF base)
{
    HDC hdc = GetDC(NULL);
    HDC mem;
    HBITMAP bmp, old;
    BITMAPINFO bi;
    BYTE *bits;
    int x, y, i;
    int br = GetRValue(base), bg = GetGValue(base), bb = GetBValue(base);

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = TILE;
    bi.bmiHeader.biHeight = -TILE;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    mem = CreateCompatibleDC(hdc);
    bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    old = (HBITMAP)SelectObject(mem, bmp);
    ReleaseDC(NULL, hdc);
    if (!bits) {
        if (bmp) DeleteObject(bmp);
        DeleteDC(mem);
        return NULL;
    }

    /* 1. 底色 + 细噪点 */
    g_seed = 20240515u ^ (base * 2654435761u);
    for (y = 0; y < TILE; y++) {
        for (x = 0; x < TILE; x++) {
            int n = (int)(pb_rand() % 5) - 2;          /* -2..2 */
            int l = (x * 7 + y * 13) % 11 == 0 ? -2 : 0;
            BYTE *p = bits + (y * TILE + x) * 4;
            p[0] = (BYTE)clampi(bb + n + l, 0, 255);
            p[1] = (BYTE)clampi(bg + n + l, 0, 255);
            p[2] = (BYTE)clampi(br + n + l, 0, 255);
            p[3] = 255;
        }
    }
    /* 2. 几根更深的纤维短划 */
    for (i = 0; i < TILE / 3; i++) {
        int sx = (int)(pb_rand() % TILE);
        int sy = (int)(pb_rand() % TILE);
        int len = 3 + (int)(pb_rand() % 9);
        int dx = (pb_rand() % 3) ? 1 : 0;
        int dy = dx ? 0 : 1;
        int k;
        for (k = 0; k < len; k++) {
            int px = (sx + dx * k) % TILE;
            int py = (sy + dy * k) % TILE;
            BYTE *p = bits + (py * TILE + px) * 4;
            p[0] = (BYTE)clampi(bb - 9, 0, 255);
            p[1] = (BYTE)clampi(bg - 8, 0, 255);
            p[2] = (BYTE)clampi(br - 7, 0, 255);
        }
    }

    SelectObject(mem, old);
    DeleteDC(mem);
    return bmp;
}

void paper_init(void)
{
    g_pbCount = 0;
    memset(g_pb, 0, sizeof(g_pb));
}

void paper_free(void)
{
    int i;
    for (i = 0; i < g_pbCount; i++) {
        if (g_pb[i].br) DeleteObject(g_pb[i].br);
        g_pb[i].br = NULL;
    }
    g_pbCount = 0;
}

/* 取（并缓存）某底色的纸纹刷 */
HBRUSH paper_brush(COLORREF base)
{
    int i;
    HBITMAP tile;
    HBRUSH br;

    for (i = 0; i < g_pbCount; i++)
        if (g_pb[i].base == base && g_pb[i].br) return g_pb[i].br;
    if (g_pbCount >= MAX_BRUSH) {
        /* 满了就回收最早的一个 */
        if (g_pb[0].br) DeleteObject(g_pb[0].br);
        memmove(&g_pb[0], &g_pb[1], sizeof(g_pb[0]) * (MAX_BRUSH - 1));
        g_pbCount--;
    }
    tile = make_tile(base);
    if (!tile) return NULL;
    br = CreatePatternBrush(tile);
    DeleteObject(tile);
    if (!br) return NULL;
    g_pb[g_pbCount].base = base;
    g_pb[g_pbCount].br = br;
    g_pbCount++;
    return br;
}

/* 用纸纹填充矩形；alignOrg=1 时让纹理对齐屏幕，多张卡片看起来是同一种纸 */
void paper_fill(HDC hdc, const RECT *rc, COLORREF base, int alignOrg)
{
    HBRUSH br = paper_brush(base);
    POINT old;
    old.x = old.y = 0;
    if (!br) {
        HBRUSH sol = CreateSolidBrush(base);
        FillRect(hdc, rc, sol);
        DeleteObject(sol);
        return;
    }
    if (alignOrg) {
        POINT p;
        GetViewportOrgEx(hdc, &p);
        SetBrushOrgEx(hdc, (rc->left - p.x) % TILE - TILE, (rc->top - p.y) % TILE - TILE, &old);
    }
    FillRect(hdc, rc, br);
    if (alignOrg) SetBrushOrgEx(hdc, old.x, old.y, NULL);
}

COLORREF paper_shade(COLORREF c, int delta)
{
    int r = GetRValue(c) + delta;
    int g = GetGValue(c) + delta;
    int b = GetBValue(c) + delta;
    return RGB(clampi(r, 0, 255), clampi(g, 0, 255), clampi(b, 0, 255));
}

/* 只在 hdc 上画一圈柔和投影（由内向外渐隐），不填充内部 */
void paper_shadow_only(HDC hdc, const RECT *rc, int radius, int depth)
{
    int i;
    for (i = depth; i >= 1; i--) {
        RECT r = *rc;
        COLORREF col;
        HBRUSH br;
        HPEN pen;
        HBRUSH ob;
        HPEN op;
        int t = 235 - (depth - i) * 7;          /* 越靠外越亮（越接近纸色） */
        col = RGB(t, t - 3, t - 9);
        r.left += i - 1;
        r.top += i - 1;
        r.right += i + 1;
        r.bottom += i + 1;
        br = CreateSolidBrush(col);
        pen = CreatePen(PS_SOLID, 1, col);
        ob = (HBRUSH)SelectObject(hdc, br);
        op = (HPEN)SelectObject(hdc, pen);
        RoundRect(hdc, r.left, r.top, r.right, r.bottom, radius + i, radius + i);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(pen);
        DeleteObject(br);
    }
}

/* 纸张卡片：投影 + 纹理纸面 + 细边 */
void paper_card(HDC hdc, const RECT *rc, COLORREF base, int radius,
                int lifted, COLORREF border)
{
    RECT r = *rc;
    HPEN pen, op;
    HBRUSH ob;
    if (lifted > 0) {
        RECT sr = r;
        sr.left += 2; sr.top += 3; sr.right -= 1; sr.bottom -= 1;
        paper_shadow_only(hdc, &sr, radius, lifted);
    }
    paper_fill(hdc, &r, base, 1);
    /* 下沿几层极轻的暗边，像纸张的厚度与自然卷曲 */
    {
        int i;
        for (i = 0; i < 4; i++) {
            RECT band = r;
            band.left += 1;
            band.right -= 1;
            band.bottom = r.bottom - 1 - i;
            band.top = band.bottom - 1;
            if (band.top <= r.top) break;
            {
                HBRUSH b = CreateSolidBrush(paper_shade(base, -(2 + i * 2)));
                FillRect(hdc, &band, b);
                DeleteObject(b);
            }
        }
    }
    pen = CreatePen(PS_SOLID, 1, border);
    op = (HPEN)SelectObject(hdc, pen);
    ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, r.left, r.top, r.right - 1, r.bottom - 1, radius, radius);
    SelectObject(hdc, ob);
    SelectObject(hdc, op);
    DeleteObject(pen);
}

/* 右下角折角：像书页被掀起一角 */
void paper_fold_corner(HDC hdc, const RECT *rc, COLORREF base, int size)
{
    POINT pt[3];
    HBRUSH br;
    HBRUSH ob;
    HPEN op;

    /* 阴影三角 */
    pt[0].x = rc->right - size; pt[0].y = rc->bottom - 1;
    pt[1].x = rc->right - 1;    pt[1].y = rc->bottom - size;
    pt[2].x = rc->right - size; pt[2].y = rc->bottom - size;
    br = CreateSolidBrush(paper_shade(base, -26));
    ob = (HBRUSH)SelectObject(hdc, br);
    op = (HPEN)SelectObject(hdc, GetStockObject(NULL_PEN));
    Polygon(hdc, pt, 3);

    /* 掀起的纸角 */
    pt[0].x = rc->right - 1;    pt[0].y = rc->bottom - size;
    pt[1].x = rc->right - size; pt[1].y = rc->bottom - 1;
    pt[2].x = rc->right - 1;    pt[2].y = rc->bottom - 1;
    SelectObject(hdc, ob);
    DeleteObject(br);
    br = CreateSolidBrush(paper_shade(base, 14));
    ob = (HBRUSH)SelectObject(hdc, br);
    Polygon(hdc, pt, 3);
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(br);
}

/* 信笺横格线 */
void paper_rules(HDC hdc, const RECT *rc, int top, int step, COLORREF col)
{
    HPEN pen = CreatePen(PS_SOLID, 1, col);
    HPEN op = (HPEN)SelectObject(hdc, pen);
    int y;
    for (y = top; y < rc->bottom; y += step) {
        MoveToEx(hdc, rc->left, y, NULL);
        LineTo(hdc, rc->right, y);
    }
    SelectObject(hdc, op);
    DeleteObject(pen);
}

/* ======================================================================
 *  笔记本：封面 + 页面 + 左侧螺旋装订
 *  client 为整个客户区（画成书的封面）；pageOut / contentLeftOut 输出页面
 *  矩形与可放置内容的左边界。
 * ====================================================================*/
void paper_notebook(HDC hdc, const RECT *client, int scale,
                    RECT *pageOut, int *contentLeftOut)
{
    RECT page;
    int margin = MulDiv(11, scale, 100);
    int bindW = MulDiv(44, scale, 100);
    int bottomGap = MulDiv(7, scale, 100);

    /* 先算出页面矩形（hdc 为 NULL 时只做测量，不绘制） */
    page.left = client->left + margin + MulDiv(10, scale, 100);
    page.top = client->top + margin;
    page.right = client->right - margin;
    page.bottom = client->bottom - margin - bottomGap;
    if (page.right < page.left + 80 || page.bottom < page.top + 80) {
        page = *client;
        bindW = 0;
    }
    if (pageOut) *pageOut = page;
    if (contentLeftOut) *contentLeftOut = page.left + bindW;
    if (!hdc) return;

    /* 1) 封面：深暖褐 + 纸纹，左侧一道更暗的书脊 */
    {
        RECT cov = *client;
        paper_fill(hdc, &cov, RGB(74, 56, 44), 0);
        {
            HBRUSH hb = CreateSolidBrush(paper_shade(RGB(74, 56, 44), -12));
            RECT b = cov;
            b.right = b.left + MulDiv(7, scale, 100);
            FillRect(hdc, &b, hb);
            DeleteObject(hb);
        }
    }
    /* 2) 页面 */
    {
        RECT sh = page;
        sh.left += 2; sh.top += 3; sh.right += 2; sh.bottom += 3;
        paper_shadow_only(hdc, &sh, 4, 6);
    }
    paper_fill(hdc, &page, TH_BG, 1);
    /* 页面下沿露出下面的纸 */
    {
        RECT e1 = page, e2 = page;
        e1.left += MulDiv(10, scale, 100); e1.right -= MulDiv(4, scale, 100);
        e1.top = page.bottom - MulDiv(5, scale, 100);
        e2.left += MulDiv(18, scale, 100); e2.right -= MulDiv(9, scale, 100);
        e2.top = page.bottom - MulDiv(2, scale, 100);
        {
            HBRUSH hb = CreateSolidBrush(RGB(228, 222, 208));
            FillRect(hdc, &e1, hb);
            DeleteObject(hb);
            hb = CreateSolidBrush(RGB(212, 205, 190));
            FillRect(hdc, &e2, hb);
            DeleteObject(hb);
        }
    }
    /* 3) 装订带与螺旋环 */
    {
        RECT band = page;
        band.right = band.left + bindW - MulDiv(12, scale, 100);
        {
            HBRUSH hb = CreateSolidBrush(RGB(230, 224, 210));
            FillRect(hdc, &band, hb);
            DeleteObject(hb);
        }
        {
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(202, 194, 178));
            HPEN op = (HPEN)SelectObject(hdc, pen);
            MoveToEx(hdc, band.right, band.top, NULL);
            LineTo(hdc, band.right, band.bottom);
            SelectObject(hdc, op);
            DeleteObject(pen);
        }
        {
            int step = MulDiv(58, scale, 100);
            int cx = band.right + MulDiv(7, scale, 100);
            int ringR = MulDiv(11, scale, 100);
            int penW = MulDiv(3, scale, 100);
            int y;
            if (step < 20) step = 20;
            if (penW < 2) penW = 2;
            for (y = step / 2; y < (page.bottom - page.top); y += step) {
                int yy = page.top + y;
                HPEN pen = CreatePen(PS_SOLID, penW, RGB(118, 110, 98));
                HPEN op = (HPEN)SelectObject(hdc, pen);
                HBRUSH ob = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                Ellipse(hdc, cx - ringR, yy - ringR, cx + ringR, yy + ringR);
                SelectObject(hdc, ob);
                SelectObject(hdc, op);
                DeleteObject(pen);
                pen = CreatePen(PS_SOLID, 1, RGB(174, 166, 152));
                op = (HPEN)SelectObject(hdc, pen);
                MoveToEx(hdc, cx - ringR + penW / 2, yy - ringR + 4, NULL);
                LineTo(hdc, cx - ringR + penW / 2, yy + ringR - 4);
                SelectObject(hdc, op);
                DeleteObject(pen);
            }
        }
    }
}
