/* ============================================================================
 *  ZSchedule - ui_edit.c
 *  日程编辑窗口（模态）：标题 / 日期时分秒 / 每周·自定义周期 / 富文本正文 /
 *  链接 / 图片附件与预览 / 颜色 / 状态
 *
 *  正文用 RichEdit 承载，同一段文字可混排不同字体、字号，支持加粗 / 斜体 /
 *  下划线；系统字体列表来自 EnumFontFamiliesEx。若系统缺少 RichEdit 组件，
 *  自动退回普通多行文本框（功能仍可用，只是没有格式）。
 * ==========================================================================*/
#include "zsched.h"
#include <richedit.h>

#define EW_W      788
#define EW_H      872

#define PADX      16
#define LBLW      76
#define CTLX      (PADX + LBLW)          /* 92 */
#define CTRW      (EW_W - CTLX - PADX)   /* 680 */

#define ATT_LIST_W  340
#define ATT_PREV_W  212
#define ATT_BTN_W   92
#define ATT_H       178

#define MAX_FONTS   420

/* 正文文字颜色（逐段上色，参考 Word）：第 1 个是“默认/自动” */
#define TXT_COLOR_COUNT 7
#define IDE_TXTCOLOR_BASE 4200
#define IDE_TXTCOLOR_MORE 4210
static const COLORREF g_txtColors[TXT_COLOR_COUNT] = {
    RGB(0, 0, 0),        /* 默认（跟随便笺字色） */
    RGB(198, 44, 44),    /* 红 */
    RGB(214, 122, 30),   /* 橙 */
    RGB(28, 146, 76),    /* 绿 */
    RGB(42, 92, 204),    /* 蓝 */
    RGB(140, 70, 196),   /* 紫 */
    RGB(120, 120, 120)   /* 灰 */
};

typedef struct {
    Schedule item;
    int      isNew;
    int      result;
    int      alive;
    HWND     hSelf;

    HWND     hTitle;
    HWND     hDateS, hTimeS, hHasEnd, hDateE, hTimeE;
    HWND     hRep, hInterval, hUnit;
    HWND     hWeek[7];
    HWND     hFont, hSize, hBold, hItalic, hUnderline, hFmtAll;
    HWND     hTxtColor[TXT_COLOR_COUNT];
    HWND     hTxtColorMore;
    COLORREF curTxtColor;      /* 光标处的文字颜色 */
    int      curTxtAuto;       /* 1 = 自动（跟随便笺默认字色） */
    HWND     hNote;
    HWND     hLink, hLinkOpen;
    HWND     hAtt, hAttImg, hAttFile, hAttDel, hAttOpen, hAttUp, hAttDown, hAttView;
    HWND     hDone, hEnabled, hAnniv, hNoteBtn, hTodo, hShowImg;
    HWND     hRemPopup, hRemToast, hRemSound, hRemLead, hRemRepeat, hRemTimes;
    HWND     hRemRing, hRemRingInfo;      /* 提示音来源：铃声…按钮 + 当前来源 */
    HWND     hTt, hTtNext, hCdu[4];
    HWND     hColor[8], hColorMore;
    HWND     hOk, hCancel, hDel;
    HFONT    hFontUI, hFontTitle, hFontSmall;
    int      scale;
    int      rich;
    int      curColor;
    wchar_t  paths[MAX_ATTACH][MAX_PATH];
    int      pathCount;
    int      loading;
    RECT     prevRect;
    wchar_t  fontNames[MAX_FONTS][LF_FACESIZE];
    int      fontCount;
} EditCtx;

static EditCtx *g_edit;

static int SC(EditCtx *c, int v) { return MulDiv(v, c->scale, 100); }

/* ------------------------------------------------------------------ 小工具 */
static HWND mk_child(HWND parent, const wchar_t *cls, const wchar_t *txt,
                     DWORD style, DWORD ex, int x, int y, int w, int h, int id,
                     int scale)
{
    return CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | style,
                           MulDiv(x, scale, 100), MulDiv(y, scale, 100),
                           MulDiv(w, scale, 100), MulDiv(h, scale, 100),
                           parent, (HMENU)(INT_PTR)id, g_hInst, NULL);
}

static void set_font(HWND h, HFONT f)
{
    if (h) SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
}

static void ctl_text(HWND h, const wchar_t *txt)
{
    if (h) SetWindowTextW(h, txt ? txt : L"");
}

static int ctl_int(HWND h, int defv, int lo, int hi)
{
    wchar_t buf[32];
    int v;
    if (!h) return defv;
    GetWindowTextW(h, buf, 32);
    v = (int)wcstol(buf, NULL, 10);
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

static void center_over(HWND child, HWND owner, int w, int h)
{
    RECT ro, wa;
    int x, y;
    if (!owner || !GetWindowRect(owner, &ro)) {
        ro.left = 0; ro.top = 0; ro.right = 1024; ro.bottom = 768;
    }
    x = ro.left + ((ro.right - ro.left) - w) / 2;
    y = ro.top + ((ro.bottom - ro.top) - h) / 2;
    /* 整窗落在工作区内：窗口变高后底部按钮不能被屏幕切掉 */
    wa.left = 0; wa.top = 0; wa.right = 1920; wa.bottom = 1080;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (x + w > wa.right)  x = wa.right - w;
    if (y + h > wa.bottom) y = wa.bottom - h;
    if (x < wa.left) x = wa.left;
    if (y < wa.top)  y = wa.top;
    SetWindowPos(child, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

/* --------------------------------------------------------------- 附件列表 */
static void att_refresh(EditCtx *c)
{
    int i, sel = (int)SendMessageW(c->hAtt, LB_GETCURSEL, 0, 0);
    SendMessageW(c->hAtt, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < c->pathCount; i++) {
        int idx = (int)SendMessageW(c->hAtt, LB_ADDSTRING, 0, (LPARAM)L" ");
        SendMessageW(c->hAtt, LB_SETITEMDATA, (WPARAM)idx, (LPARAM)i);
    }
    if (c->pathCount > 0) {
        if (sel < 0 || sel >= c->pathCount) sel = 0;
        SendMessageW(c->hAtt, LB_SETCURSEL, (WPARAM)sel, 0);
    }
    InvalidateRect(c->hAtt, NULL, TRUE);
    if (c->prevRect.right > c->prevRect.left)
        InvalidateRect(c->hSelf, &c->prevRect, TRUE);
}

static int att_sel_index(EditCtx *c)
{
    int sel = (int)SendMessageW(c->hAtt, LB_GETCURSEL, 0, 0);
    if (sel < 0) return -1;
    return (int)SendMessageW(c->hAtt, LB_GETITEMDATA, (WPARAM)sel, 0);
}

static void att_add(EditCtx *c, const wchar_t *path)
{
    if (!path || !path[0]) return;
    if (c->pathCount >= MAX_ATTACH) {
        MessageBoxW(c->hSelf, L"附件数量已达上限（12 个）。", L"提示", MB_ICONINFORMATION);
        return;
    }
    {
        size_t n = wcslen(path);
        if (n >= MAX_PATH) n = MAX_PATH - 1;
        memcpy(c->paths[c->pathCount], path, n * sizeof(wchar_t));
        c->paths[c->pathCount][n] = 0;
    }
    c->pathCount++;
}

static int pick_files(EditCtx *c, int imagesOnly, wchar_t out[][MAX_PATH], int maxOut)
{
    static const wchar_t *f_img =
        L"图片文件 (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif)\0"
        L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif\0"
        L"所有文件 (*.*)\0*.*\0\0";
    static const wchar_t *f_all =
        L"所有文件 (*.*)\0*.*\0"
        L"图片文件 (*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif)\0"
        L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif\0\0";
    OPENFILENAMEW ofn;
    wchar_t *buf;
    const size_t bufcch = 32 * MAX_PATH;
    int n = 0;

    buf = (wchar_t *)calloc(bufcch, sizeof(wchar_t));
    if (!buf) return 0;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = c->hSelf;
    ofn.lpstrFilter = imagesOnly ? f_img : f_all;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = (DWORD)bufcch;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn)) {
        wchar_t *p = buf;
        wchar_t dir[MAX_PATH];
        size_t dl = wcslen(p);
        if (p[dl + 1] == 0) {
            wcsncpy(out[n], p, MAX_PATH - 1);
            out[n][MAX_PATH - 1] = 0;
            n++;
        } else {
            wcsncpy(dir, p, MAX_PATH - 1);
            dir[MAX_PATH - 1] = 0;
            p += dl + 1;
            while (*p && n < maxOut) {
                wchar_t full[MAX_PATH * 2];
                if (dir[0]) {
                    wcsncpy(full, dir, MAX_PATH * 2 - 1);
                    full[MAX_PATH * 2 - 1] = 0;
                    if (full[wcslen(full) - 1] != L'\\')
                        wcsncat(full, L"\\", MAX_PATH * 2 - wcslen(full) - 1);
                    wcsncat(full, p, MAX_PATH * 2 - wcslen(full) - 1);
                } else {
                    wcsncpy(full, p, MAX_PATH * 2 - 1);
                    full[MAX_PATH * 2 - 1] = 0;
                }
                wcsncpy(out[n], full, MAX_PATH - 1);
                out[n][MAX_PATH - 1] = 0;
                n++;
                p += wcslen(p) + 1;
            }
        }
    }
    free(buf);
    return n;
}

/* ------------------------------------------------------- 附件列表自绘 */
static void draw_attach_item(EditCtx *c, LPDRAWITEMSTRUCT dis)
{
    RECT rc = dis->rcItem;
    int idx = (int)dis->itemID;
    int i, sel;
    const wchar_t *path;
    wchar_t name[MAX_PATH];
    COLORREF cb;
    int ts = SC(c, 34);
    int tx = rc.left + SC(c, 5);

    if (idx < 0) {
        HBRUSH b = CreateSolidBrush(TH_SURFACE);
        FillRect(dis->hDC, &rc, b);
        DeleteObject(b);
        return;
    }
    i = (int)SendMessageW(c->hAtt, LB_GETITEMDATA, (WPARAM)idx, 0);
    if (i < 0 || i >= c->pathCount) return;
    path = c->paths[i];
    sel = (dis->itemState & ODS_SELECTED) ? 1 : 0;
    cb = sel ? TH_SELECT : TH_SURFACE;
    {
        HBRUSH b = CreateSolidBrush(cb);
        FillRect(dis->hDC, &rc, b);
        DeleteObject(b);
    }
    SetBkMode(dis->hDC, TRANSPARENT);
    if (img_is_image(path)) {
        RECT tr;
        tr.left = tx; tr.top = rc.top + SC(c, 5);
        tr.right = tr.left + ts; tr.bottom = tr.top + ts;
        {
            HBRUSH b = CreateSolidBrush(TH_SURFACE_2);
            FillRect(dis->hDC, &tr, b);
            DeleteObject(b);
        }
        img_draw(dis->hDC, path, tr.left, tr.top, ts, ts);
    } else {
        img_draw_file_icon(dis->hDC, path, tx, rc.top + SC(c, 5), ts, ts);
    }
    file_name_of(path, name, MAX_PATH);
    SelectObject(dis->hDC, c->hFontUI);
    SetTextColor(dis->hDC, TH_TEXT);
    {
        RECT t = rc;
        t.left = tx + ts + SC(c, 10);
        t.top += SC(c, 5);
        t.right -= SC(c, 6);
        t.bottom = t.top + SC(c, 19);
        DrawTextW(dis->hDC, name, -1, &t, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    SelectObject(dis->hDC, c->hFontSmall);
    SetTextColor(dis->hDC, TH_TEXT_3);
    {
        RECT t = rc;
        t.left = tx + ts + SC(c, 10);
        t.top += SC(c, 25);
        t.right -= SC(c, 6);
        t.bottom = t.top + SC(c, 16);
        DrawTextW(dis->hDC, path, -1, &t, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
}

/* ------------------------------------------------------------ 颜色按钮 */
static void draw_color_btn(EditCtx *c, LPDRAWITEMSTRUCT dis)
{
    int i = (int)dis->CtlID - IDE_COLOR_BASE;
    RECT rc = dis->rcItem;
    HBRUSH br, ob;
    HPEN pen, op;
    int sel = (c->curColor == i);

    br = CreateSolidBrush(g_palette[i]);
    ob = (HBRUSH)SelectObject(dis->hDC, br);
    pen = CreatePen(PS_SOLID, 1, TH_LINE);
    op = (HPEN)SelectObject(dis->hDC, pen);
    Rectangle(dis->hDC, rc.left, rc.top, rc.right, rc.bottom);
    SelectObject(dis->hDC, op);
    SelectObject(dis->hDC, ob);
    DeleteObject(pen);
    DeleteObject(br);
    if (sel) {
        HPEN p2 = CreatePen(PS_SOLID, 2, TH_TEXT);
        HPEN o2 = (HPEN)SelectObject(dis->hDC, p2);
        HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH o3 = (HBRUSH)SelectObject(dis->hDC, nb);
        Rectangle(dis->hDC, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
        SelectObject(dis->hDC, o3);
        SelectObject(dis->hDC, o2);
        DeleteObject(p2);
    }
}

static void fmt_sync_from_caret(EditCtx *c);

static void draw_txtcolor_btn(EditCtx *c, LPDRAWITEMSTRUCT dis)
{
    int i = (int)dis->CtlID - IDE_TXTCOLOR_BASE;
    RECT rc = dis->rcItem;
    HBRUSH br, ob;
    HPEN pen, op;
    int sel;
    if (i < 0 || i >= TXT_COLOR_COUNT) return;
    sel = (i == 0) ? (c->curTxtAuto ? 1 : 0)
                   : (!c->curTxtAuto && c->curTxtColor == g_txtColors[i] ? 1 : 0);
    /* 外框（做成一枚小色块，像调色盘） */
    br = CreateSolidBrush(g_txtColors[i]);
    ob = (HBRUSH)SelectObject(dis->hDC, br);
    pen = CreatePen(PS_SOLID, 1, TH_LINE);
    op = (HPEN)SelectObject(dis->hDC, pen);
    Rectangle(dis->hDC, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1);
    SelectObject(dis->hDC, op);
    SelectObject(dis->hDC, ob);
    DeleteObject(pen);
    DeleteObject(br);
    if (i == 0) {           /* “默认”那枚画一道斜杠，表示自动 */
        HPEN p3 = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
        HPEN o3 = (HPEN)SelectObject(dis->hDC, p3);
        MoveToEx(dis->hDC, rc.left + 3, rc.bottom - 3, NULL);
        LineTo(dis->hDC, rc.right - 3, rc.top + 3);
        SelectObject(dis->hDC, o3);
        DeleteObject(p3);
    }
    if (sel) {
        HPEN p2 = CreatePen(PS_SOLID, 2, TH_TEXT);
        HPEN o2 = (HPEN)SelectObject(dis->hDC, p2);
        HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH o4 = (HBRUSH)SelectObject(dis->hDC, nb);
        Rectangle(dis->hDC, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(dis->hDC, o4);
        SelectObject(dis->hDC, o2);
        DeleteObject(p2);
    }
}

/* 把颜色应用到选区（idx = -1 表示用自定义颜色 col） */
static void fmt_apply_color(EditCtx *c, int idx)
{
    COLORREF col = 0;
    int mode;
    if (!c->rich) return;
    if (idx < 0) { col = c->curTxtColor; mode = 1; }
    else if (idx == 0) { mode = 2; }              /* 默认：恢复自动色 */
    else { col = g_txtColors[idx]; mode = 1; }
    rt_apply(c->hNote, NULL, 0, -1, -1, -1, mode, col, 0);
    SetFocus(c->hNote);
    fmt_sync_from_caret(c);
}

/* ----------------------------------------------------- 预览区（原图缩放） */
static void draw_preview(EditCtx *c, HDC hdc)
{
    RECT r = c->prevRect;
    int idx;
    if (r.right <= r.left) return;
    {
        HBRUSH b = CreateSolidBrush(TH_SURFACE_2);
        HPEN p = CreatePen(PS_SOLID, 1, TH_LINE);
        HBRUSH ob = (HBRUSH)SelectObject(hdc, b);
        HPEN op = (HPEN)SelectObject(hdc, p);
        Rectangle(hdc, r.left, r.top, r.right, r.bottom);
        SelectObject(hdc, op);
        SelectObject(hdc, ob);
        DeleteObject(p);
        DeleteObject(b);
    }
    SetBkMode(hdc, TRANSPARENT);
    idx = att_sel_index(c);
    if (idx < 0 || idx >= c->pathCount) {
        RECT t = r;
        SelectObject(hdc, c->hFontSmall);
        SetTextColor(hdc, TH_TEXT_3);
        DrawTextW(hdc, L"选中附件后在此预览\n图片将等比缩放到预览框",
                  -1, &t, DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
        return;
    }
    if (img_is_image(c->paths[idx])) {
        int w = 0, h = 0;
        img_draw_zoom(hdc, c->paths[idx], r.left + 2, r.top + 2,
                      r.right - r.left - 4, r.bottom - r.top - 4, 1);
        if (!img_size(c->paths[idx], &w, &h)) {
            RECT t = r;
            SelectObject(hdc, c->hFontSmall);
            SetTextColor(hdc, TH_DANGER);
            DrawTextW(hdc, L"图片无法读取", -1, &t,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        /* 右下角标注原始尺寸 */
        if (w > 0) {
            wchar_t buf[64];
            WBuf b;
            RECT t = r;
            wb_init(&b, buf, 64);
            wb_add(&b, L"原图 ");
            wb_int(&b, w);
            wb_add(&b, L"×");
            wb_int(&b, h);
            SelectObject(hdc, c->hFontSmall);
            SetTextColor(hdc, TH_TEXT_2);
            t.left += 6; t.right -= 6; t.bottom -= 4; t.top = t.bottom - 16;
            DrawTextW(hdc, buf, -1, &t, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
        }
    } else {
        wchar_t name[MAX_PATH];
        RECT t = r;
        file_name_of(c->paths[idx], name, MAX_PATH);
        img_draw_file_icon(hdc, c->paths[idx], (r.left + r.right) / 2 - SC(c, 26),
                           r.top + SC(c, 34), SC(c, 52), SC(c, 52));
        t.top = r.top + SC(c, 96);
        SelectObject(hdc, c->hFontSmall);
        SetTextColor(hdc, TH_TEXT_2);
        DrawTextW(hdc, name, -1, &t, DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    }
}

/* ------------------------------------------------------------ 重复方式联动 */
static void rep_sync_ui(EditCtx *c, int forceUnit)
{
    int sel = (int)SendMessageW(c->hRep, CB_GETCURSEL, 0, 0);
    int unitIdx = (int)SendMessageW(c->hUnit, CB_GETCURSEL, 0, 0);
    int intervalOn = (sel != 0);
    int unitOn = (sel == 5);
    int weekOn = 0;
    int i;

    if (sel == 2)      { unitIdx = UNIT_WEEK;  weekOn = 1; }
    else if (sel == 1) { unitIdx = UNIT_DAY; }
    else if (sel == 3) { unitIdx = UNIT_MONTH; }
    else if (sel == 4) { unitIdx = UNIT_YEAR; }
    else if (sel == 5) { weekOn = (unitIdx == UNIT_WEEK); }
    if (forceUnit && unitIdx >= 0)
        SendMessageW(c->hUnit, CB_SETCURSEL, (WPARAM)unitIdx, 0);

    EnableWindow(c->hInterval, intervalOn);
    EnableWindow(c->hUnit, unitOn);
    for (i = 0; i < 7; i++) EnableWindow(c->hWeek[i], weekOn);
    if (!intervalOn) ctl_text(c->hInterval, L"1");
}

/* ------------------------------------------------------- 富文本工具条同步 */
static void fmt_sync_from_caret(EditCtx *c)
{
    wchar_t face[LF_FACESIZE];
    int pt = 0, b = 0, i = 0, u = 0;
    wchar_t buf[16];
    WBuf wb;
    if (!c->rich || c->loading) return;
    rt_get_format(c->hNote, face, LF_FACESIZE, &pt, &b, &i, &u,
                  &c->curTxtColor, &c->curTxtAuto);
    c->loading = 1;
    if (face[0]) SetWindowTextW(c->hFont, face);
    if (pt > 0) {
        wb_init(&wb, buf, 16);
        wb_int(&wb, pt);
        SetWindowTextW(c->hSize, buf);
    }
    flat_check(c->hBold, b);
    flat_check(c->hItalic, i);
    flat_check(c->hUnderline, u);
    {   int k; for (k = 0; k < TXT_COLOR_COUNT; k++) InvalidateRect(c->hTxtColor[k], NULL, FALSE); }
    c->loading = 0;
}

static void fmt_apply(EditCtx *c, int setFace, int setSize,
                      int bold, int italic, int underline, int toAll)
{
    wchar_t face[LF_FACESIZE];
    wchar_t buf[32];
    int pt = -1;
    if (!c->rich) return;
    face[0] = 0;
    if (setFace) GetWindowTextW(c->hFont, face, LF_FACESIZE);
    if (setSize) {
        GetWindowTextW(c->hSize, buf, 32);
        pt = (int)wcstol(buf, NULL, 10);
        if (pt < 4) pt = 4;
        if (pt > 200) pt = 200;
    }
    rt_apply(c->hNote, setFace ? face : NULL, pt,
             bold, italic, underline, 0, 0, toAll);
    SetFocus(c->hNote);
}

/* ------------------------------------------------------------ 初始化控件 */
static void edit_create_children(EditCtx *c)
{
    HWND h = c->hSelf;
    static const wchar_t *wd[7] = { L"日", L"一", L"二", L"三", L"四", L"五", L"六" };
    static const int sizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 32, 40, 48, 64 };
    int i, s = c->scale;
    int y;

    c->hFontUI    = CreateFontW(-MulDiv(40, s, 300), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    c->hFontTitle = CreateFontW(-MulDiv(48, s, 300), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0,
                                DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
    c->hFontSmall = CreateFontW(-MulDiv(32, s, 300), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");

    /* 标题 */
    set_font(mk_child(h, L"STATIC", L"标题", SS_RIGHT, 0, PADX, 22, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hTitle = mk_child(h, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER,
                         0, CTLX, 18, CTRW, 28, IDE_TITLE, s);
    set_font(c->hTitle, c->hFontTitle);
    SendMessageW(c->hTitle, EM_SETLIMITTEXT, MAX_TITLE - 1, 0);

    /* 开始时间 */
    set_font(mk_child(h, L"STATIC", L"开始", SS_RIGHT, 0, PADX, 60, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hDateS = mk_child(h, DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_SHORTDATEFORMAT,
                         0, CTLX, 56, 168, 28, IDE_DATE_S, s);
    c->hTimeS = mk_child(h, DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_TIMEFORMAT,
                         0, CTLX + 176, 56, 116, 28, IDE_TIME_S, s);
    DateTime_SetFormat(c->hDateS, L"yyyy-MM-dd");
    DateTime_SetFormat(c->hTimeS, L"HH:mm:ss");
    set_font(c->hDateS, c->hFontUI);
    set_font(c->hTimeS, c->hFontUI);
    set_font(mk_child(h, L"STATIC", L"日期  时:分:秒", SS_LEFT, 0,
                      CTLX + 302, 60, 240, 22, 0, s), c->hFontSmall);

    /* 结束时间 */
    set_font(mk_child(h, L"STATIC", L"结束", SS_RIGHT, 0, PADX, 96, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hHasEnd = mk_child(h, L"BUTTON", L"启用", WS_TABSTOP | BS_AUTOCHECKBOX,
                          0, CTLX, 94, 62, 24, IDE_HASEND, s);
    c->hDateE = mk_child(h, DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_SHORTDATEFORMAT,
                         0, CTLX + 70, 92, 168, 28, IDE_DATE_E, s);
    c->hTimeE = mk_child(h, DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_TIMEFORMAT,
                         0, CTLX + 246, 92, 116, 28, IDE_TIME_E, s);
    DateTime_SetFormat(c->hDateE, L"yyyy-MM-dd");
    DateTime_SetFormat(c->hTimeE, L"HH:mm:ss");
    set_font(c->hHasEnd, c->hFontUI);
    set_font(c->hDateE, c->hFontUI);
    set_font(c->hTimeE, c->hFontUI);

    /* 重复 */
    set_font(mk_child(h, L"STATIC", L"重复", SS_RIGHT, 0, PADX, 136, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hRep = mk_child(h, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                       0, CTLX, 132, 172, 220, IDE_REP, s);
    set_font(mk_child(h, L"STATIC", L"每", SS_LEFT, 0, CTLX + 180, 136, 22, 22, 0, s), c->hFontUI);
    c->hInterval = mk_child(h, L"EDIT", L"1", WS_TABSTOP | ES_NUMBER | ES_CENTER | WS_BORDER,
                            0, CTLX + 204, 132, 54, 26, IDE_INTERVAL, s);
    c->hUnit = mk_child(h, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                        0, CTLX + 264, 132, 96, 200, IDE_UNIT, s);
    {
        static const wchar_t *repItems[6] = {
            L"不重复", L"每天", L"每周(可多选)", L"每月", L"每年", L"自定义周期"
        };
        static const wchar_t *unitItems[4] = { L"天", L"周", L"月", L"年" };
        int k;
        for (k = 0; k < 6; k++)
            SendMessageW(c->hRep, CB_ADDSTRING, 0, (LPARAM)repItems[k]);
        for (k = 0; k < 4; k++)
            SendMessageW(c->hUnit, CB_ADDSTRING, 0, (LPARAM)unitItems[k]);
        SendMessageW(c->hRep, CB_SETCURSEL, 0, 0);
        SendMessageW(c->hUnit, CB_SETCURSEL, 0, 0);
    }
    set_font(c->hRep, c->hFontUI);
    set_font(c->hInterval, c->hFontUI);
    set_font(c->hUnit, c->hFontUI);

    /* 星期 */
    set_font(mk_child(h, L"STATIC", L"星期", SS_RIGHT, 0, PADX - 20, 172, LBLW + 12, 22, 0, s),
             c->hFontSmall);
    for (i = 0; i < 7; i++) {
        c->hWeek[i] = mk_child(h, L"BUTTON", wd[i], WS_TABSTOP | BS_AUTOCHECKBOX,
                               0, CTLX + i * 52, 170, 46, 26, IDE_WD_BASE + i, s);
        set_font(c->hWeek[i], c->hFontUI);
    }

    /* 颜色 */
    set_font(mk_child(h, L"STATIC", L"颜色", SS_RIGHT, 0, PADX, 210, LBLW - 8, 22, 0, s), c->hFontUI);
    for (i = 0; i < 8; i++)
        c->hColor[i] = mk_child(h, L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW,
                                0, CTLX + i * 38, 206, 32, 28, IDE_COLOR_BASE + i, s);
    c->hColorMore = mk_child(h, L"BUTTON", L"更多颜色…", WS_TABSTOP | BS_PUSHBUTTON,
                             0, CTLX + 8 * 38 + 6, 206, 100, 28, IDE_COLOR_MORE, s);
    set_font(c->hColorMore, c->hFontSmall);

    /* 正文（富文本） */
    set_font(mk_child(h, L"STATIC", L"正文", SS_RIGHT, 0, PADX, 244, LBLW - 8, 22, 0, s), c->hFontUI);
    if (c->rich) {
        int bx = CTLX + 8;
        c->hFont = mk_child(h, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                            0, bx, 236, 168, 320, IDE_FONT, s);
        c->hSize = mk_child(h, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWN | WS_VSCROLL,
                            0, bx + 176, 236, 62, 240, IDE_SIZE, s);
        for (i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++) {
            wchar_t buf[16];
            WBuf wb;
            wb_init(&wb, buf, 16);
            wb_int(&wb, sizes[i]);
            SendMessageW(c->hSize, CB_ADDSTRING, 0, (LPARAM)buf);
        }
        ctl_text(c->hSize, L"10");
        c->hBold = flat_create(h, L"B", IDE_BOLD, 0, 1);
        c->hItalic = flat_create(h, L"I", IDE_ITALIC, 0, 1);
        c->hUnderline = flat_create(h, L"U", IDE_UNDERLINE, 0, 1);
        c->hFmtAll = flat_create(h, L"整篇应用", IDE_FMT_ALL, 0, 0);
        c->hTxtColorMore = flat_create(h, L"颜色…", IDE_TXTCOLOR_MORE, 0, 0);
        for (i = 0; i < TXT_COLOR_COUNT; i++)
            c->hTxtColor[i] = mk_child(h, L"BUTTON", L"",
                                       WS_TABSTOP | BS_OWNERDRAW, 0,
                                       562 + i * 21, 236, 18, 26,
                                       IDE_TXTCOLOR_BASE + i, s);
        set_font(c->hFont, c->hFontUI);
        set_font(c->hSize, c->hFontUI);

        c->fontCount = rt_enum_fonts(c->fontNames, MAX_FONTS);
        for (i = 0; i < c->fontCount; i++)
            SendMessageW(c->hFont, CB_ADDSTRING, 0, (LPARAM)c->fontNames[i]);
        ctl_text(c->hFont, L"Microsoft YaHei UI");
        if ((int)SendMessageW(c->hFont, CB_GETCURSEL, 0, 0) < 0) {
            /* 系统字体表里没有该名字时，手工补一个并选中 */
            SendMessageW(c->hFont, CB_INSERTSTRING, 0, (LPARAM)L"Microsoft YaHei UI");
            SendMessageW(c->hFont, CB_SETCURSEL, 0, 0);
        }

        c->hNote = CreateWindowExW(WS_EX_CLIENTEDGE, rt_class(), L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                    ES_MULTILINE | ES_WANTRETURN | ES_AUTOVSCROLL,
                    MulDiv(CTLX, s, 100), MulDiv(268, s, 100),
                    MulDiv(CTRW, s, 100), MulDiv(150, s, 100),
                    h, (HMENU)IDE_NOTE, g_hInst, NULL);
        if (c->hNote) {
            SendMessageW(c->hNote, EM_SETEVENTMASK, 0,
                         ENM_SELCHANGE | ENM_CHANGE);
            SendMessageW(c->hNote, WM_SETFONT, (WPARAM)c->hFontUI, TRUE);
        }
    } else {
        c->hNote = mk_child(h, L"EDIT", L"",
                            WS_TABSTOP | WS_BORDER | WS_VSCROLL | ES_MULTILINE |
                            ES_WANTRETURN | ES_AUTOVSCROLL,
                            WS_EX_CLIENTEDGE, CTLX, 240, CTRW, 178, IDE_NOTE, s);
    }
    if (c->hNote) SendMessageW(c->hNote, EM_SETLIMITTEXT, MAX_NOTE - 1, 0);

    /* 链接 */
    set_font(mk_child(h, L"STATIC", L"链接", SS_RIGHT, 0, PADX, 430, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hLink = mk_child(h, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | WS_BORDER,
                        0, CTLX, 426, CTRW - 96, 26, IDE_LINK, s);
    c->hLinkOpen = mk_child(h, L"BUTTON", L"打开", WS_TABSTOP | BS_PUSHBUTTON,
                            0, CTLX + CTRW - 88, 425, 88, 28, IDE_LINKOPEN, s);
    set_font(c->hLink, c->hFontUI);
    set_font(c->hLinkOpen, c->hFontUI);

    /* 附件 + 预览 */
    set_font(mk_child(h, L"STATIC", L"附件", SS_RIGHT, 0, PADX, 472, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hAtt = mk_child(h, L"LISTBOX", L"",
                       WS_TABSTOP | WS_VSCROLL | WS_BORDER | LBS_OWNERDRAWFIXED |
                       LBS_NOTIFY | LBS_HASSTRINGS,
                       WS_EX_CLIENTEDGE, CTLX, 466, ATT_LIST_W, ATT_H, IDE_ATT_LIST, s);
    SendMessageW(c->hAtt, LB_SETITEMHEIGHT, 0, (LPARAM)SC(c, 44));
    set_font(c->hAtt, c->hFontUI);

    c->prevRect.left = MulDiv(CTLX + ATT_LIST_W + 8, s, 100);
    c->prevRect.top = MulDiv(466, s, 100);
    c->prevRect.right = MulDiv(CTLX + ATT_LIST_W + 8 + ATT_PREV_W, s, 100);
    c->prevRect.bottom = MulDiv(466 + ATT_H, s, 100);

    y = 466;
    {
        int bx = CTLX + ATT_LIST_W + 8 + ATT_PREV_W + 8;
        c->hAttImg  = mk_child(h, L"BUTTON", L"添加图片…", WS_TABSTOP | BS_PUSHBUTTON, 0,
                               bx, y, ATT_BTN_W, 26, IDE_ATT_IMG, s);
        c->hAttFile = mk_child(h, L"BUTTON", L"添加文件…", WS_TABSTOP | BS_PUSHBUTTON, 0,
                               bx, y + 32, ATT_BTN_W, 26, IDE_ATT_FILE, s);
        c->hAttView = mk_child(h, L"BUTTON", L"查看原图", WS_TABSTOP | BS_PUSHBUTTON, 0,
                               bx, y + 64, ATT_BTN_W, 26, IDE_ATT_VIEW, s);
        c->hAttOpen = mk_child(h, L"BUTTON", L"外部打开", WS_TABSTOP | BS_PUSHBUTTON, 0,
                               bx, y + 96, ATT_BTN_W, 26, IDE_ATT_OPEN, s);
        c->hAttUp   = mk_child(h, L"BUTTON", L"上移", WS_TABSTOP | BS_PUSHBUTTON, 0,
                               bx, y + 128, 44, 26, IDE_ATT_UP, s);
        c->hAttDown = mk_child(h, L"BUTTON", L"下移", WS_TABSTOP | BS_PUSHBUTTON, 0,
                               bx + 48, y + 128, 44, 26, IDE_ATT_DOWN, s);
        c->hAttDel  = mk_child(h, L"BUTTON", L"移除", WS_TABSTOP | BS_PUSHBUTTON, 0,
                               bx, y + 156, ATT_BTN_W, 26, IDE_ATT_DEL, s);
        set_font(c->hAttImg, c->hFontSmall);
        set_font(c->hAttFile, c->hFontSmall);
        set_font(c->hAttView, c->hFontSmall);
        set_font(c->hAttOpen, c->hFontSmall);
        set_font(c->hAttUp, c->hFontSmall);
        set_font(c->hAttDown, c->hFontSmall);
        set_font(c->hAttDel, c->hFontSmall);
    }

    /* 状态 */
    set_font(mk_child(h, L"STATIC", L"状态", SS_RIGHT, 0, PADX, 664, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hEnabled = mk_child(h, L"BUTTON", L"启用（参与倒计时提醒）", WS_TABSTOP | BS_AUTOCHECKBOX,
                           0, CTLX, 662, 250, 26, IDE_ENABLED, s);
    c->hDone = mk_child(h, L"BUTTON", L"标记为已完成", WS_TABSTOP | BS_AUTOCHECKBOX,
                        0, CTLX + 260, 662, 190, 26, IDE_DONE, s);
    c->hAnniv = mk_child(h, L"BUTTON", L"纪念日（记录已过去的事）",
                         WS_TABSTOP | BS_AUTOCHECKBOX,
                         0, CTLX + 456, 662, 226, 26, IDE_ANNIV, s);
    c->hTodo = mk_child(h, L"BUTTON", L"\u65e0\u65f6\u95f4\u5f85\u529e\uff08\u4e0d\u8bbe\u65f6\u95f4\uff0c\u53ea\u8bb0\u6807\u9898\u4e0e\u5185\u5bb9\uff09",
                        WS_TABSTOP | BS_AUTOCHECKBOX, 0,
                        CTLX, 692, 340, 26, IDE_TODO, s);
    c->hShowImg = mk_child(h, L"BUTTON", L"\u4fbf\u7b3a\u4e0a\u76f4\u63a5\u663e\u793a\u63d2\u5165\u7684\u56fe\u7247",
                           WS_TABSTOP | BS_AUTOCHECKBOX, 0,
                           CTLX + 350, 692, 260, 26, IDE_SHOWIMG, s);
    set_font(c->hEnabled, c->hFontUI);
    set_font(c->hDone, c->hFontUI);
    set_font(c->hAnniv, c->hFontUI);
    set_font(c->hTodo, c->hFontUI);
    set_font(c->hShowImg, c->hFontUI);

    /* 提醒 */
    set_font(mk_child(h, L"STATIC", L"\u63d0\u9192", SS_RIGHT, 0, PADX, 724, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hRemPopup = mk_child(h, L"BUTTON", L"\u4fbf\u7b3a\u5f39\u7a97", WS_TABSTOP | BS_AUTOCHECKBOX,
                            0, CTLX, 722, 96, 26, IDE_REM_POPUP, s);
    c->hRemToast = mk_child(h, L"BUTTON", L"\u901a\u77e5\u680f", WS_TABSTOP | BS_AUTOCHECKBOX,
                            0, CTLX + 100, 722, 84, 26, IDE_REM_TOAST, s);
    c->hRemSound = mk_child(h, L"BUTTON", L"\u63d0\u793a\u97f3", WS_TABSTOP | BS_AUTOCHECKBOX,
                            0, CTLX + 188, 722, 84, 26, IDE_REM_SOUND, s);
    set_font(mk_child(h, L"STATIC", L"\u63d0\u524d", SS_RIGHT, 0, CTLX + 282, 724, 36, 22, 0, s), c->hFontUI);
    c->hRemLead = mk_child(h, L"EDIT", L"0", WS_TABSTOP | ES_NUMBER | WS_BORDER,
                           0, CTLX + 322, 722, 46, 24, IDE_REM_LEAD, s);
    set_font(mk_child(h, L"STATIC", L"\u5206\u949f\u3001\u6bcf", SS_LEFT, 0, CTLX + 372, 724, 74, 22, 0, s), c->hFontUI);
    c->hRemRepeat = mk_child(h, L"EDIT", L"0", WS_TABSTOP | ES_NUMBER | WS_BORDER,
                             0, CTLX + 442, 722, 46, 24, IDE_REM_REPEAT, s);
    set_font(mk_child(h, L"STATIC", L"\u5206\u949f\u91cd\u590d\u3001\u5171", SS_LEFT, 0, CTLX + 492, 724, 110, 22, 0, s), c->hFontUI);
    c->hRemTimes = mk_child(h, L"EDIT", L"1", WS_TABSTOP | ES_NUMBER | WS_BORDER,
                            0, CTLX + 592, 722, 40, 24, IDE_REM_TIMES, s);
    set_font(mk_child(h, L"STATIC", L"\u6b21", SS_LEFT, 0, CTLX + 636, 724, 34, 22, 0, s), c->hFontUI);
    set_font(c->hRemPopup, c->hFontUI);
    set_font(c->hRemToast, c->hFontUI);
    set_font(c->hRemSound, c->hFontUI);
    set_font(c->hRemLead, c->hFontUI);
    set_font(c->hRemRepeat, c->hFontUI);
    set_font(c->hRemTimes, c->hFontUI);

    /* \u8bfe\u8868\u6a21\u5f0f */
    set_font(mk_child(h, L"STATIC", L"\u8bfe\u8868", SS_RIGHT, 0, PADX, 754, LBLW - 8, 22, 0, s), c->hFontUI);
    c->hTt = mk_child(h, L"BUTTON", L"\u6807\u8bb0\u4e3a\u8bfe\u8868\uff1a\u5230\u70b9\u81ea\u52a8\u5f00\u5173\u4fbf\u7b3a\u3001\u5185\u5bb9\u8ddf\u968f\u4e0a\u4e0b\u8bfe",
                      WS_TABSTOP | BS_AUTOCHECKBOX, 0, CTLX, 752, 460, 26, IDE_TT, s);
    set_font(c->hTt, c->hFontUI);

    /* \u63d0\u793a\u97f3\u6765\u6e90\uff1a\u7cfb\u7edf\u63d0\u793a\u97f3 / \u81ea\u9009\u58f0\u97f3\u6587\u4ef6\uff08\u5c31\u5728\u201c\u63d0\u793a\u97f3\u201d\u540c\u4e00\u533a\uff09 */
    c->hRemRing = mk_child(h, L"BUTTON", L"\u94c3\u58f0\u2026", WS_TABSTOP | BS_PUSHBUTTON, 0,
                           CTLX + 470, 752, 92, 26, IDE_REM_RING, s);
    c->hRemRingInfo = mk_child(h, L"STATIC", L"", SS_LEFT | SS_ENDELLIPSIS, 0,
                               CTLX + 568, 755, 112, 22, IDE_REM_RING_INFO, s);
    set_font(c->hRemRing, c->hFontSmall);
    set_font(c->hRemRingInfo, c->hFontSmall);

    /* \u5012\u8ba1\u65f6\u663e\u793a\u7cbe\u5ea6 */
    set_font(mk_child(h, L"STATIC", L"\u5012\u8ba1\u65f6", SS_RIGHT, 0, PADX, 786, LBLW - 8, 22, 0, s), c->hFontUI);
    {
        static const wchar_t *du[4] = { L"\u5929", L"\u65f6", L"\u5206", L"\u79d2" };
        int dk;
        for (dk = 0; dk < 4; dk++) {
            c->hCdu[dk] = mk_child(h, L"BUTTON", du[dk],
                                   WS_TABSTOP | BS_AUTORADIOBUTTON | (dk == 0 ? WS_GROUP : 0),
                                   0, CTLX + dk * 62, 782, 58, 26,
                                   IDE_CDU_DAY + dk, s);
            set_font(c->hCdu[dk], c->hFontUI);
        }
        set_font(mk_child(h, L"STATIC", L"\u663e\u793a\u5230\u8be5\u7cbe\u5ea6\uff08\u8bfe\u8868\u9ed8\u8ba4\u5230\u5206\uff09", SS_LEFT, 0,
                          CTLX + 256, 786, 240, 22, 0, s), c->hFontUI);
        /* \u72ec\u7acb\u5f00\u5173\uff1a\u8bfe\u8868\u4fbf\u7b3a\u5728\u5012\u8ba1\u65f6\u4e0b\u65b9\u518d\u663e\u793a\u201c\u4e0b\u4e00\u8282\u8bfe\u201d\u540d\u79f0\uff08\u53ef\u70b9\u51fb\u8df3\u7f16\u8f91\uff09 */
        c->hTtNext = mk_child(h, L"BUTTON", L"\u8bfe\u8868\uff1a\u663e\u793a\u4e0b\u4e00\u8282\u8bfe\u540d\u79f0",
                              WS_TABSTOP | BS_AUTOCHECKBOX, 0,
                              CTLX + 504, 782, 176, 26, IDE_TTNEXT, s);
        set_font(c->hTtNext, c->hFontUI);
    }

    /* \u5e95\u90e8\u6309\u94ae */
    c->hDel = mk_child(h, L"BUTTON", L"\u5220\u9664", WS_TABSTOP | WS_GROUP | BS_PUSHBUTTON, 0,
                       PADX, EW_H - 46, 96, 32, IDE_DEL, s);
    c->hNoteBtn = mk_child(h, L"BUTTON", L"便笺…", WS_TABSTOP | BS_PUSHBUTTON, 0,
                           PADX + 106, EW_H - 46, 96, 32, IDE_NOTECOLOR, s);
    set_font(c->hNoteBtn, c->hFontUI);
    c->hCancel = mk_child(h, L"BUTTON", L"取消", WS_TABSTOP | BS_PUSHBUTTON, 0,
                          EW_W - PADX - 212, EW_H - 46, 106, 32, IDE_CANCEL, s);
    c->hOk = mk_child(h, L"BUTTON", L"确定", WS_TABSTOP | BS_DEFPUSHBUTTON, 0,
                      EW_W - PADX - 106, EW_H - 46, 106, 32, IDE_OK, s);
    set_font(c->hDel, c->hFontUI);
    set_font(c->hCancel, c->hFontUI);
    set_font(c->hOk, c->hFontUI);
}

static void edit_sync_kind(EditCtx *c);

/* 提示音来源那一行文字：“铃声：系统提示音” / “铃声：xxx.mp3” */
static void edit_ring_info(EditCtx *c)
{
    wchar_t d[MAX_PATH], line[MAX_PATH + 24];
    WBuf rb;
    snd_desc(&c->item, d, MAX_PATH);
    wb_init(&rb, line, MAX_PATH + 24);
    wb_add(&rb, L"铃声：");
    wb_add(&rb, d);
    ctl_text(c->hRemRingInfo, line);
}

/* 选择自选铃声文件（音频过滤，默认程序能播的都行） */
static int pick_sound_file(EditCtx *c, wchar_t *out)
{
    static const wchar_t *f_snd =
        L"声音文件 (*.wav;*.mp3;*.wma;*.m4a;*.aac;*.ogg;*.flac;*.mid;*.midi)\0"
        L"*.wav;*.mp3;*.wma;*.m4a;*.aac;*.ogg;*.flac;*.mid;*.midi\0"
        L"所有文件 (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn;
    if (!out) return 0;
    out[0] = 0;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = c->hSelf;
    ofn.lpstrFilter = f_snd;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"选择铃声文件";
    ofn.lpstrFile = out;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameW(&ofn) ? 1 : 0;
}

/* 把 item 的数据装进控件 */
static void edit_load(EditCtx *c)
{
    Schedule *it = &c->item;
    wchar_t buf[64];
    int i, sel = 0;

    c->loading = 1;
    c->curTxtColor = RGB(0, 0, 0);
    c->curTxtAuto = 1;
    ctl_text(c->hTitle, it->title);
    DateTime_SetSystemtime(c->hDateS, GDT_VALID, &it->start);
    DateTime_SetSystemtime(c->hTimeS, GDT_VALID, &it->start);
    {
        SYSTEMTIME e = it->end;
        if (!it->hasEnd) {
            struct tm t;
            time_t v;
            memset(&t, 0, sizeof(t));
            t.tm_year = it->start.wYear - 1900;
            t.tm_mon  = it->start.wMonth - 1;
            t.tm_mday = it->start.wDay;
            t.tm_hour = it->start.wHour;
            t.tm_min  = it->start.wMinute;
            t.tm_sec  = it->start.wSecond;
            t.tm_isdst = -1;
            v = mktime(&t) + 3600;
            {
                struct tm *pt = localtime(&v);
                if (pt) {
                    e.wYear = (WORD)(pt->tm_year + 1900);
                    e.wMonth = (WORD)(pt->tm_mon + 1);
                    e.wDay = (WORD)(pt->tm_mday);
                    e.wHour = (WORD)(pt->tm_hour);
                    e.wMinute = (WORD)(pt->tm_min);
                    e.wSecond = (WORD)(pt->tm_sec);
                }
            }
        }
        DateTime_SetSystemtime(c->hDateE, GDT_VALID, &e);
        DateTime_SetSystemtime(c->hTimeE, GDT_VALID, &e);
    }
    SendMessageW(c->hHasEnd, BM_SETCHECK, it->hasEnd ? BST_CHECKED : BST_UNCHECKED, 0);

    if (it->repKind == REP_NONE) {
        sel = 0;
    } else if (it->repKind == REP_WEEKLY) {
        sel = 2;
    } else {
        if (it->repUnit == UNIT_DAY && it->repInterval == 1)        sel = 1;
        else if (it->repUnit == UNIT_MONTH && it->repInterval == 1) sel = 3;
        else if (it->repUnit == UNIT_YEAR && it->repInterval == 1)  sel = 4;
        else                                                        sel = 5;
    }
    SendMessageW(c->hRep, CB_SETCURSEL, (WPARAM)sel, 0);
    SendMessageW(c->hUnit, CB_SETCURSEL, (WPARAM)it->repUnit, 0);
    {
        WBuf b;
        wb_init(&b, buf, 64);
        wb_int(&b, it->repInterval);
        ctl_text(c->hInterval, buf);
    }
    for (i = 0; i < 7; i++)
        SendMessageW(c->hWeek[i], BM_SETCHECK,
                     (it->weekMask & (1 << i)) ? BST_CHECKED : BST_UNCHECKED, 0);
    rep_sync_ui(c, 1);

    /* 正文：优先富文本，其次纯文本 */
    rt_set_text(c->hNote, c->rich ? it->noteRtf : L"", it->note);
    if (c->rich && !it->noteRtf[0]) {
        rt_apply(c->hNote, L"Microsoft YaHei UI", 10, -1, -1, -1, 0, 0, 1);
    }
    ctl_text(c->hLink, it->link);
    SendMessageW(c->hEnabled, BM_SETCHECK, it->enabled ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(c->hDone, BM_SETCHECK, it->done ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(c->hAnniv, BM_SETCHECK,
                 it->kind == KIND_ANNIV ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(c->hTodo, BM_SETCHECK,
                 it->kind == KIND_TODO ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(c->hShowImg, BM_SETCHECK, it->showImg ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(c->hTt, BM_SETCHECK, it->timetable ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(c->hTtNext, BM_SETCHECK, it->ttNext ? BST_CHECKED : BST_UNCHECKED, 0);
    {   /* \u5012\u8ba1\u65f6\u663e\u793a\u7cbe\u5ea6\uff08\u672a\u8bbe\u7f6e\u6309\u201c\u79d2\u201d\u663e\u793a\uff09 */
        int u = it->cdUnit, dk;
        if (u < CDU_DAY || u > CDU_SEC) u = CDU_SEC;
        for (dk = 0; dk < 4; dk++)
            SendMessageW(c->hCdu[dk], BM_SETCHECK,
                         (u == CDU_DAY + dk) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    SendMessageW(c->hRemPopup, BM_SETCHECK, (it->remind & REM_POPUP) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(c->hRemToast, BM_SETCHECK, (it->remind & REM_TOAST) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(c->hRemSound, BM_SETCHECK, (it->remind & REM_SOUND) ? BST_CHECKED : BST_UNCHECKED, 0);
    {   /* 提醒参数（数字框） */
        wchar_t nb[24];
        WBuf bb;
        wb_init(&bb, nb, 24);
        wb_int(&bb, it->remLead > 0 ? it->remLead : 0);
        ctl_text(c->hRemLead, nb);
        wb_init(&bb, nb, 24);
        wb_int(&bb, it->remRepeat > 0 ? it->remRepeat : 0);
        ctl_text(c->hRemRepeat, nb);
        wb_init(&bb, nb, 24);
        wb_int(&bb, it->remTimes > 0 ? it->remTimes : 1);
        ctl_text(c->hRemTimes, nb);
    }
    edit_ring_info(c);

    c->curColor = -1;
    for (i = 0; i < 8; i++)
        if (g_palette[i] == it->color) { c->curColor = i; break; }
    for (i = 0; i < 8; i++) InvalidateRect(c->hColor[i], NULL, TRUE);

    c->pathCount = it->attachCount;
    for (i = 0; i < it->attachCount; i++)
        memcpy(c->paths[i], it->attach[i], sizeof(c->paths[i]));
    att_refresh(c);

    EnableWindow(c->hDateE, it->hasEnd);
    EnableWindow(c->hTimeE, it->hasEnd);
    c->loading = 0;
    edit_sync_kind(c);
}

/* “无时间待办”与“纪念日”互斥；待办时时间与循环控件置灰 */
static void edit_sync_kind(EditCtx *c)
{
    int todo = (SendMessageW(c->hTodo, BM_GETCHECK, 0, 0) == BST_CHECKED);
    HWND grp[16];
    int i, n = 0;

    if (todo) SendMessageW(c->hAnniv, BM_SETCHECK, BST_UNCHECKED, 0);
    EnableWindow(c->hAnniv, !todo);
    {   /* \u8bfe\u8868\u53ea\u5bf9\u666e\u901a\u65e5\u7a0b\u6709\u610f\u4e49\uff1b\u201c\u663e\u793a\u4e0b\u4e00\u8282\u8bfe\u201d\u53ea\u5bf9\u8bfe\u8868\u6709\u610f\u4e49 */
        int anniv = (SendMessageW(c->hAnniv, BM_GETCHECK, 0, 0) == BST_CHECKED);
        int tt = (SendMessageW(c->hTt, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (c->hTt) EnableWindow(c->hTt, (!todo && !anniv) ? TRUE : FALSE);
        if (c->hTtNext)
            EnableWindow(c->hTtNext, (tt && !todo && !anniv) ? TRUE : FALSE);
    }
    {   /* 铃声只在勾了“提示音”时才有意义 */
        int snd = (SendMessageW(c->hRemSound, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (c->hRemRing) EnableWindow(c->hRemRing, snd ? TRUE : FALSE);
    }
    grp[n++] = c->hDateS;
    grp[n++] = c->hTimeS;
    grp[n++] = c->hHasEnd;
    grp[n++] = c->hRep;
    grp[n++] = c->hInterval;
    grp[n++] = c->hUnit;
    for (i = 0; i < 7; i++) grp[n++] = c->hWeek[i];
    for (i = 0; i < n; i++)
        if (grp[i]) EnableWindow(grp[i], todo ? FALSE : TRUE);
    if (todo) {
        EnableWindow(c->hDateE, FALSE);
        EnableWindow(c->hTimeE, FALSE);
    } else {
        int we = (SendMessageW(c->hHasEnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
        EnableWindow(c->hDateE, we);
        EnableWindow(c->hTimeE, we);
    }
}

/* 从控件收集数据；返回 0 表示校验失败 */
static int edit_store(EditCtx *c)
{
    Schedule *it = &c->item;
    SYSTEMTIME s, e;
    int i, sel;

    GetWindowTextW(c->hTitle, it->title, MAX_TITLE);
    {
        wchar_t t[MAX_TITLE];
        size_t a = 0, b;
        wcscpy(t, it->title);
        b = wcslen(t);
        while (a < b && (t[a] == L' ' || t[a] == L'\t')) a++;
        while (b > a && (t[b - 1] == L' ' || t[b - 1] == L'\t')) b--;
        memmove(it->title, t + a, (b - a) * sizeof(wchar_t));
        it->title[b - a] = 0;
    }
    if (!it->title[0]) {
        MessageBoxW(c->hSelf, L"请填写日程标题。", L"无法保存", MB_ICONWARNING);
        SetFocus(c->hTitle);
        return 0;
    }

    DateTime_GetSystemtime(c->hDateS, &s);
    {
        SYSTEMTIME t;
        DateTime_GetSystemtime(c->hTimeS, &t);
        s.wHour = t.wHour; s.wMinute = t.wMinute; s.wSecond = t.wSecond;
        s.wMilliseconds = 0;
    }
    it->start = s;
    it->hasEnd = (SendMessageW(c->hHasEnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
    if (it->hasEnd) {
        struct tm ta, te;
        time_t va, vb;
        DateTime_GetSystemtime(c->hDateE, &e);
        {
            SYSTEMTIME t;
            DateTime_GetSystemtime(c->hTimeE, &t);
            e.wHour = t.wHour; e.wMinute = t.wMinute; e.wSecond = t.wSecond;
            e.wMilliseconds = 0;
        }
        it->end = e;
        memset(&ta, 0, sizeof(ta)); memset(&te, 0, sizeof(te));
        ta.tm_year = it->start.wYear - 1900; ta.tm_mon = it->start.wMonth - 1;
        ta.tm_mday = it->start.wDay; ta.tm_hour = it->start.wHour;
        ta.tm_min = it->start.wMinute; ta.tm_sec = it->start.wSecond; ta.tm_isdst = -1;
        te.tm_year = it->end.wYear - 1900; te.tm_mon = it->end.wMonth - 1;
        te.tm_mday = it->end.wDay; te.tm_hour = it->end.wHour;
        te.tm_min = it->end.wMinute; te.tm_sec = it->end.wSecond; te.tm_isdst = -1;
        va = mktime(&ta); vb = mktime(&te);
        if (vb <= va) {
            MessageBoxW(c->hSelf, L"结束时间必须晚于开始时间。", L"无法保存", MB_ICONWARNING);
            return 0;
        }
        it->durSec = vb - va;
    } else {
        it->end = it->start;
        it->durSec = 0;
    }

    sel = (int)SendMessageW(c->hRep, CB_GETCURSEL, 0, 0);
    if (sel < 0) sel = 0;
    it->repInterval = ctl_int(c->hInterval, 1, 1, 999);
    switch (sel) {
    case 0: it->repKind = REP_NONE; break;
    case 1: it->repKind = REP_CUSTOM; it->repUnit = UNIT_DAY; break;
    case 2: it->repKind = REP_WEEKLY; it->repUnit = UNIT_WEEK; break;
    case 3: it->repKind = REP_CUSTOM; it->repUnit = UNIT_MONTH; break;
    case 4: it->repKind = REP_CUSTOM; it->repUnit = UNIT_YEAR; break;
    default:
        it->repKind = REP_CUSTOM;
        it->repUnit = (int)SendMessageW(c->hUnit, CB_GETCURSEL, 0, 0);
        if (it->repUnit < 0 || it->repUnit > 3) it->repUnit = UNIT_DAY;
        break;
    }
    it->weekMask = 0;
    for (i = 0; i < 7; i++)
        if (SendMessageW(c->hWeek[i], BM_GETCHECK, 0, 0) == BST_CHECKED)
            it->weekMask |= (1 << i);
    if (it->repKind == REP_WEEKLY && it->weekMask == 0) {
        MessageBoxW(c->hSelf, L"请至少选择一个星期。", L"无法保存", MB_ICONWARNING);
        return 0;
    }
    if (it->repKind == REP_CUSTOM && it->repUnit == UNIT_WEEK && it->weekMask == 0) {
        int wd;
        struct tm t;
        time_t v;
        memset(&t, 0, sizeof(t));
        t.tm_year = it->start.wYear - 1900; t.tm_mon = it->start.wMonth - 1;
        t.tm_mday = it->start.wDay; t.tm_isdst = -1;
        v = mktime(&t);
        wd = (v == (time_t)-1) ? 1 : localtime(&v)->tm_wday;
        it->weekMask = 1 << wd;
    }

    /* 正文：富文本存 RTF，同时提取纯文本摘要 */
    if (c->rich) {
        wchar_t rtf[MAX_RTF];
        if (rt_get_rtf(c->hNote, rtf, MAX_RTF)) {
            wcsncpy(it->noteRtf, rtf, MAX_RTF - 1);
            it->noteRtf[MAX_RTF - 1] = 0;
            rtf_to_plain(it->noteRtf, it->note, MAX_NOTE);
        } else {
            it->noteRtf[0] = 0;
            GetWindowTextW(c->hNote, it->note, MAX_NOTE);
        }
    } else {
        it->noteRtf[0] = 0;
        GetWindowTextW(c->hNote, it->note, MAX_NOTE);
    }

    GetWindowTextW(c->hLink, it->link, MAX_LINK);
    it->enabled = (SendMessageW(c->hEnabled, BM_GETCHECK, 0, 0) == BST_CHECKED);
    it->done = (SendMessageW(c->hDone, BM_GETCHECK, 0, 0) == BST_CHECKED);
    it->showImg = (SendMessageW(c->hShowImg, BM_GETCHECK, 0, 0) == BST_CHECKED);
    it->timetable = (SendMessageW(c->hTt, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
    it->ttNext = (SendMessageW(c->hTtNext, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
    {
        int dk;
        it->cdUnit = CDU_DEF;
        for (dk = 0; dk < 4; dk++)
            if (SendMessageW(c->hCdu[dk], BM_GETCHECK, 0, 0) == BST_CHECKED)
                it->cdUnit = CDU_DAY + dk;
    }
    {
        it->remind = 0;
        if (SendMessageW(c->hRemPopup, BM_GETCHECK, 0, 0) == BST_CHECKED) it->remind |= REM_POPUP;
        if (SendMessageW(c->hRemToast, BM_GETCHECK, 0, 0) == BST_CHECKED) it->remind |= REM_TOAST;
        if (SendMessageW(c->hRemSound, BM_GETCHECK, 0, 0) == BST_CHECKED) it->remind |= REM_SOUND;
        /* \u6ce8\u610f\uff1actl_text \u662f\u201c\u5199\u63a7\u4ef6\u201d\uff0c\u8bfb\u8981\u7528 ctl_int \u2014\u2014
           \u4ee5\u524d\u8fd9\u91cc\u628a\u4e09\u4e2a\u6570\u5b57\u6846\u90fd\u5f53\u8bfb\u7528\u4e86\uff0c\u7b49\u4e8e\u7528\u672a\u521d\u59cb\u5316\u7684\u7f13\u51b2\u8986\u76d6\u63a7\u4ef6\u518d\u89e3\u6790\uff0c
           \u6240\u4ee5\u201c\u63d0\u524d / \u91cd\u590d\u95f4\u9694 / \u6b21\u6570\u201d\u6539\u4e86\u786e\u5b9a\u540e\u4e0d\u751f\u6548 */
        it->remLead = ctl_int(c->hRemLead, 0, 0, 1440);
        it->remRepeat = ctl_int(c->hRemRepeat, 0, 0, 1440);
        it->remTimes = ctl_int(c->hRemTimes, 1, 1, 99);
    }
    it->kind = (SendMessageW(c->hTodo, BM_GETCHECK, 0, 0) == BST_CHECKED)
             ? KIND_TODO
             : ((SendMessageW(c->hAnniv, BM_GETCHECK, 0, 0) == BST_CHECKED)
                   ? KIND_ANNIV : KIND_TASK);
    if (c->curColor >= 0) it->color = g_palette[c->curColor];

    it->attachCount = c->pathCount;
    for (i = 0; i < c->pathCount; i++)
        memcpy(it->attach[i], c->paths[i], sizeof(it->attach[i]));

    model_normalize(it);
    return 1;
}

/* ------------------------------------------------------------------ 窗口过程 */
static LRESULT CALLBACK edit_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    EditCtx *c = g_edit;

    switch (msg) {
    case WM_CREATE:
        return 0;

    case WM_ERASEBKGND:
        {
            HDC hdc = (HDC)wp;
            RECT rc;
            GetClientRect(hwnd, &rc);
            {
                HBRUSH b = CreateSolidBrush(TH_BG);
                FillRect(hdc, &rc, b);
                DeleteObject(b);
            }
        }
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (c) draw_preview(c, hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        {
            static HBRUSH br;
            if (!br) br = CreateSolidBrush(TH_BG);
            return (LRESULT)br;
        }

    case WM_CTLCOLORBTN:
        return (LRESULT)(HBRUSH)GetStockObject(NULL_BRUSH);

    case WM_NOTIFY:
        if (c && c->rich && lp) {
            NMHDR *nh = (NMHDR *)lp;
            if (nh->idFrom == IDE_NOTE && nh->code == EN_SELCHANGE) {
                fmt_sync_from_caret(c);
            }
        }
        return 0;

    case WM_COMMAND:
        if (!c) break;
        switch (LOWORD(wp)) {
        case IDE_OK:
            if (edit_store(c)) { c->result = 1; DestroyWindow(hwnd); }
            return 0;
        case IDE_CANCEL:
            c->result = 0;
            DestroyWindow(hwnd);
            return 0;
        case IDE_DEL:
            if (c->isNew) {
                MessageBoxW(hwnd, L"这是尚未保存的新日程。", L"提示", MB_ICONINFORMATION);
            } else if (MessageBoxW(hwnd, L"确定删除这条日程吗？", L"删除确认",
                                   MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2) == IDYES) {
                c->result = -1;
                DestroyWindow(hwnd);
            }
            return 0;

        case IDE_HASEND:
            {
                int on = (SendMessageW(c->hHasEnd, BM_GETCHECK, 0, 0) == BST_CHECKED);
                EnableWindow(c->hDateE, on);
                EnableWindow(c->hTimeE, on);
            }
            return 0;

        case IDE_REP:
        case IDE_UNIT:
            if (HIWORD(wp) == CBN_SELCHANGE) rep_sync_ui(c, 0);
            return 0;

        case IDE_FONT:
            if (HIWORD(wp) == CBN_SELCHANGE && !c->loading) fmt_apply(c, 1, 0, -1, -1, -1, 0);
            return 0;
        case IDE_SIZE:
            if (HIWORD(wp) == CBN_SELCHANGE && !c->loading) fmt_apply(c, 0, 1, -1, -1, -1, 0);
            return 0;
        case IDE_BOLD:
            {
                wchar_t face[LF_FACESIZE];
                int pt = 0, b = 0, i = 0, u = 0;
                rt_get_format(c->hNote, face, LF_FACESIZE, &pt, &b, &i, &u, NULL, NULL);
                fmt_apply(c, 0, 0, b ? 0 : 1, -1, -1, 0);
                fmt_sync_from_caret(c);
            }
            return 0;
        case IDE_ITALIC:
            {
                wchar_t face[LF_FACESIZE];
                int pt = 0, b = 0, i = 0, u = 0;
                rt_get_format(c->hNote, face, LF_FACESIZE, &pt, &b, &i, &u, NULL, NULL);
                fmt_apply(c, 0, 0, -1, i ? 0 : 1, -1, 0);
                fmt_sync_from_caret(c);
            }
            return 0;
        case IDE_UNDERLINE:
            {
                wchar_t face[LF_FACESIZE];
                int pt = 0, b = 0, i = 0, u = 0;
                rt_get_format(c->hNote, face, LF_FACESIZE, &pt, &b, &i, &u, NULL, NULL);
                fmt_apply(c, 0, 0, -1, -1, u ? 0 : 1, 0);
                fmt_sync_from_caret(c);
            }
            return 0;
        case IDE_FMT_ALL:
            fmt_apply(c, 1, 1, -1, -1, -1, 1);
            return 0;

        case IDE_TXTCOLOR_MORE:
            {   /* 自定义文字颜色 */
                CHOOSECOLORW cc;
                static COLORREF cust[16];
                memset(&cc, 0, sizeof(cc));
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = c->hSelf;
                cc.lpCustColors = cust;
                cc.rgbResult = c->curTxtAuto ? RGB(0, 0, 0) : c->curTxtColor;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&cc)) {
                    c->curTxtColor = cc.rgbResult;
                    fmt_apply_color(c, -1);
                }
            }
            return 0;

        case IDE_TODO:
    case IDE_ANNIV:
    case IDE_TT:
    case IDE_REM_SOUND:
        edit_sync_kind(c);
        return 0;

        case IDE_REM_RING:
            {   /* 铃声来源：系统提示音 / 自选声音文件（提醒时自动打开这个文件） */
                HMENU m = CreatePopupMenu();
                POINT p;
                RECT r;
                int cmd;
                int hasFile = c->item.sndFile[0] ? 1 : 0;
                AppendMenuW(m, MF_STRING | (c->item.sndMode != SND_FILE ? MF_CHECKED : 0),
                            4300, L"系统提示音（Windows 默认）");
                AppendMenuW(m, MF_STRING | (c->item.sndMode == SND_FILE ? MF_CHECKED : 0) |
                               (hasFile ? 0 : MF_GRAYED),
                            4301, L"自选声音文件");
                AppendMenuW(m, MF_SEPARATOR, 0, NULL);
                AppendMenuW(m, MF_STRING, 4302, L"选择声音文件…");
                AppendMenuW(m, MF_STRING | (hasFile ? 0 : MF_GRAYED), 4303,
                            L"试听（在本程序里播放）");
                AppendMenuW(m, MF_STRING | (snd_is_active() ? 0 : MF_GRAYED), 4306,
                            L"停止铃声");
                AppendMenuW(m, MF_STRING | (hasFile ? 0 : MF_GRAYED), 4307,
                            L"用默认播放器打开");
                AppendMenuW(m, MF_STRING | (hasFile ? 0 : MF_GRAYED), 4304, L"清除自选文件");
                AppendMenuW(m, MF_SEPARATOR, 0, NULL);
                AppendMenuW(m, MF_STRING | MF_DISABLED, 4305,
                            L"提醒时在本程序里播放（wav / mp3 / flac）");
                GetWindowRect(c->hRemRing, &r);
                p.x = r.left;
                p.y = r.bottom;
                cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                          p.x, p.y, 0, c->hSelf, NULL);
                DestroyMenu(m);
                if (cmd == 4300) c->item.sndMode = SND_SYS;
                else if (cmd == 4301) c->item.sndMode = SND_FILE;
                else if (cmd == 4302) {
                    wchar_t got[MAX_PATH];
                    if (pick_sound_file(c, got)) {
                        wcsncpy(c->item.sndFile, got, MAX_PATH - 1);
                        c->item.sndFile[MAX_PATH - 1] = 0;
                        c->item.sndMode = SND_FILE;
                    }
                }
                else if (cmd == 4303) snd_play_file(c->item.sndFile);
                else if (cmd == 4306) snd_stop();
                else if (cmd == 4307) snd_open_file(c->item.sndFile);
                else if (cmd == 4304) { c->item.sndFile[0] = 0; c->item.sndMode = SND_SYS; }
                if (cmd) {
                    /* 选了铃声就顺带把“提示音”勾上：不然选了也不会响 */
                    SendMessageW(c->hRemSound, BM_SETCHECK, BST_CHECKED, 0);
                    edit_sync_kind(c);
                    edit_ring_info(c);
                }
            }
            return 0;
    case IDE_NOTECOLOR:
            {
                HMENU m = CreatePopupMenu();
                HMENU mc = CreatePopupMenu();
                POINT p;
                RECT r;
                int cmd, i, st = NOTE_STYLE_PAPER, ci = 0;
                int open = note_is_open(c->item.id);
                if (open) note_get_look(c->item.id, &st, &ci);
                for (i = 0; i < NOTE_COLOR_COUNT; i++)
                    AppendMenuW(mc, MF_STRING | (open && i == ci ? MF_CHECKED : 0),
                                4000 + i, g_noteColorNames[i]);
                AppendMenuW(m, MF_STRING | (open && st == NOTE_STYLE_PAPER ? MF_CHECKED : 0),
                            4100, L"便笺纸外观");
                AppendMenuW(m, MF_STRING | (open && st == NOTE_STYLE_SYSTEM ? MF_CHECKED : 0),
                            4101, L"系统窗口外观");
                AppendMenuW(m, MF_STRING | (open && st == NOTE_STYLE_BOOKMARK ? MF_CHECKED : 0),
                            4104, L"书签形态（吸附屏幕边缘）");
                AppendMenuW(m, MF_POPUP, (UINT_PTR)mc, L"便笺颜色");
                AppendMenuW(m, MF_SEPARATOR, 0, NULL);
                AppendMenuW(m, MF_STRING, 4102, L"打开便笺");
                AppendMenuW(m, MF_STRING, 4103, L"关闭便笺");
                GetWindowRect(c->hNoteBtn, &r);
                p.x = r.left;
                p.y = r.bottom;
                cmd = (int)TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                          p.x, p.y, 0, c->hSelf, NULL);
                DestroyMenu(m);
                if (cmd >= 4000 && cmd < 4000 + NOTE_COLOR_COUNT) {
                    if (open) { note_set_color(c->item.id, cmd - 4000); }
                    else note_open(c->item.id, NOTE_STYLE_PAPER, cmd - 4000, 1);
                } else if (cmd == 4100 || cmd == 4101) {
                    int want = (cmd == 4100) ? NOTE_STYLE_PAPER : NOTE_STYLE_SYSTEM;
                    if (open) note_set_style(c->item.id, want);
                    else note_open(c->item.id, want, ci, 1);
                } else if (cmd == 4104) {
                    if (open) note_set_bookmark(c->item.id, st != NOTE_STYLE_BOOKMARK);
                    else note_open(c->item.id, st, ci, 1);
                } else if (cmd == 4102) {
                    note_open(c->item.id, st, ci, 1);
                } else if (cmd == 4103) {
                    if (open) note_close_one(c->item.id);
                }
            }
            return 0;

        case IDE_LINKOPEN:
            {
                wchar_t url[MAX_LINK];
                GetWindowTextW(c->hLink, url, MAX_LINK);
                if (url[0]) ShellExecuteW(hwnd, L"open", url, NULL, NULL, SW_SHOWNORMAL);
            }
            return 0;

        case IDE_ATT_IMG:
        case IDE_ATT_FILE:
            {
                wchar_t got[16][MAX_PATH];
                int n = pick_files(c, LOWORD(wp) == IDE_ATT_IMG, got, 16);
                int i;
                for (i = 0; i < n; i++) att_add(c, got[i]);
                if (n) att_refresh(c);
            }
            return 0;

        case IDE_ATT_DEL:
            {
                int idx = att_sel_index(c);
                if (idx < 0) break;
                memmove(&c->paths[idx], &c->paths[idx + 1],
                        (size_t)(c->pathCount - idx - 1) * sizeof(c->paths[0]));
                c->pathCount--;
                att_refresh(c);
                if (c->pathCount) SendMessageW(c->hAtt, LB_SETCURSEL,
                                               (WPARAM)(idx ? idx - 1 : 0), 0);
            }
            return 0;

        case IDE_ATT_OPEN:
            {
                int idx = att_sel_index(c);
                if (idx < 0) break;
                if (GetFileAttributesW(c->paths[idx]) == INVALID_FILE_ATTRIBUTES) {
                    MessageBoxW(hwnd, L"文件不存在或已被移动。", L"提示", MB_ICONWARNING);
                } else {
                    ShellExecuteW(hwnd, L"open", c->paths[idx], NULL, NULL, SW_SHOWNORMAL);
                }
            }
            return 0;

        case IDE_ATT_VIEW:
            {
                const wchar_t *imgs[MAX_ATTACH];
                int i, n = 0, start = 0;
                int cur = att_sel_index(c);
                for (i = 0; i < c->pathCount; i++) {
                    if (!img_is_image(c->paths[i])) continue;
                    if (i == cur) start = n;
                    imgs[n++] = c->paths[i];
                }
                if (n == 0) {
                    MessageBoxW(hwnd, L"当前附件里没有图片。", L"提示", MB_ICONINFORMATION);
                    break;
                }
                iv_open(GetParent(hwnd), imgs, n, start);
            }
            return 0;

        case IDE_ATT_UP:
        case IDE_ATT_DOWN:
            {
                int idx = att_sel_index(c);
                int to = (LOWORD(wp) == IDE_ATT_UP) ? idx - 1 : idx + 1;
                wchar_t tmp[MAX_PATH];
                if (idx < 0 || to < 0 || to >= c->pathCount) break;
                memcpy(tmp, c->paths[idx], sizeof(tmp));
                memcpy(c->paths[idx], c->paths[to], sizeof(tmp));
                memcpy(c->paths[to], tmp, sizeof(tmp));
                att_refresh(c);
                SendMessageW(c->hAtt, LB_SETCURSEL, (WPARAM)to, 0);
            }
            return 0;

        case IDE_COLOR_MORE:
            {
                CHOOSECOLORW cc;
                static COLORREF custom[16];
                memset(&cc, 0, sizeof(cc));
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hwnd;
                cc.lpCustColors = custom;
                cc.rgbResult = (c->curColor >= 0) ? g_palette[c->curColor] : TH_ACCENT;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&cc)) {
                    c->item.color = cc.rgbResult;
                    c->curColor = -1;
                    {
                        int i;
                        for (i = 0; i < 8; i++) InvalidateRect(c->hColor[i], NULL, TRUE);
                    }
                }
            }
            return 0;

        case IDE_ATT_LIST:
            if (HIWORD(wp) == LBN_SELCHANGE) {
                if (c->prevRect.right > c->prevRect.left)
                    InvalidateRect(hwnd, &c->prevRect, TRUE);
            }
            if (HIWORD(wp) == LBN_DBLCLK) {
                int idx = att_sel_index(c);
                if (idx >= 0 && img_is_image(c->paths[idx])) {
                    SendMessageW(hwnd, WM_COMMAND, IDE_ATT_VIEW, 0);
                } else if (idx >= 0) {
                    SendMessageW(hwnd, WM_COMMAND, IDE_ATT_OPEN, 0);
                }
            }
            return 0;

        default:
            if (LOWORD(wp) >= IDE_COLOR_BASE && LOWORD(wp) < IDE_COLOR_BASE + 8) {
                int k;
                c->curColor = LOWORD(wp) - IDE_COLOR_BASE;
                for (k = 0; k < 8; k++) InvalidateRect(c->hColor[k], NULL, TRUE);
                return 0;
            }
            break;
        }
        if (LOWORD(wp) >= IDE_TXTCOLOR_BASE &&
            LOWORD(wp) < IDE_TXTCOLOR_BASE + TXT_COLOR_COUNT) {
            fmt_apply_color(c, LOWORD(wp) - IDE_TXTCOLOR_BASE);
            return 0;
        }
        break;

    case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lp;
            if (mis->CtlID == IDE_ATT_LIST) {
                mis->itemHeight = (UINT)SC(c, 44);
                return TRUE;
            }
        }
        break;

    case WM_DRAWITEM:
        if (c) {
            LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
            if (dis->CtlID == IDE_ATT_LIST) { draw_attach_item(c, dis); return TRUE; }
            if (dis->CtlID >= IDE_COLOR_BASE && dis->CtlID < IDE_COLOR_BASE + 8) {
                draw_color_btn(c, dis);
                return TRUE;
            }
            if (dis->CtlID >= IDE_TXTCOLOR_BASE &&
                dis->CtlID < IDE_TXTCOLOR_BASE + TXT_COLOR_COUNT) {
                draw_txtcolor_btn(c, dis);
                return TRUE;
            }
        }
        break;

    case WM_CLOSE:
        if (c) c->result = 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (c) {
            c->alive = 0;
            if (c->hFontUI) DeleteObject(c->hFontUI);
            if (c->hFontTitle) DeleteObject(c->hFontTitle);
            if (c->hFontSmall) DeleteObject(c->hFontSmall);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void edit_register_class(HINSTANCE hInst)
{
    static HBRUSH br;
    WNDCLASSEXW wc;
    if (!br) br = CreateSolidBrush(TH_BG);
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = edit_proc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = br;
    wc.lpszClassName = EDIT_CLASS;
    wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIconW(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);
}

/* ------------------------------------------------------------------ 对外接口 */
static int edit_run(HWND owner, Schedule *item, int isNew)
{
    EditCtx *c;
    HWND hwnd;
    MSG msg;
    int result, winW, winH;
    HDC hdc;
    int dpiY;

    c = (EditCtx *)calloc(1, sizeof(EditCtx));
    if (!c) return 0;
    c->item = *item;
    c->isNew = isNew;
    c->result = 0;
    c->alive = 1;
    c->rich = rt_available();
    g_edit = c;

    hdc = GetDC(NULL);
    dpiY = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    {
        int want = MulDiv(dpiY, 100, 96);
        int wa = GetSystemMetrics(SM_CYSCREEN) - 60;
        int maxPct = (int)((LONGLONG)wa * 100 / (EW_H + 48));
        if (want > maxPct) want = maxPct;
        if (want < 70) want = 70;
        c->scale = want;
    }
    winW = MulDiv(EW_W, c->scale, 100);
    winH = MulDiv(EW_H, c->scale, 100);
    {
        RECT rc = { 0, 0, winW, winH };
        AdjustWindowRectEx(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE,
                           WS_EX_DLGMODALFRAME);
        winW = rc.right - rc.left;
        winH = rc.bottom - rc.top;
    }

    hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT,
                           EDIT_CLASS, isNew ? L"新建日程" : L"编辑日程",
                           WS_POPUP | WS_CAPTION | WS_SYSMENU,
                           CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
                           owner, NULL, g_hInst, NULL);
    if (!hwnd) { free(c); g_edit = NULL; return 0; }
    c->hSelf = hwnd;

    edit_create_children(c);
    edit_load(c);

    /* 富文本工具条按钮需要按字体度量定位 */
    if (c->rich) {
        int bh = 28, by = 234;
        int x = CTLX + 176 + 70 + 8;
        SetWindowPos(c->hBold, NULL, MulDiv(x, c->scale, 100), MulDiv(by, c->scale, 100),
                     MulDiv(34, c->scale, 100), MulDiv(bh, c->scale, 100), SWP_NOZORDER);
        SetWindowPos(c->hItalic, NULL, MulDiv(x + 38, c->scale, 100), MulDiv(by, c->scale, 100),
                     MulDiv(34, c->scale, 100), MulDiv(bh, c->scale, 100), SWP_NOZORDER);
        SetWindowPos(c->hUnderline, NULL, MulDiv(x + 76, c->scale, 100), MulDiv(by, c->scale, 100),
                     MulDiv(34, c->scale, 100), MulDiv(bh, c->scale, 100), SWP_NOZORDER);
        SetWindowPos(c->hFmtAll, NULL, MulDiv(x + 118, c->scale, 100), MulDiv(by, c->scale, 100),
                     MulDiv(88, c->scale, 100), MulDiv(bh, c->scale, 100), SWP_NOZORDER);
        if (c->hTxtColorMore)
            SetWindowPos(c->hTxtColorMore, NULL, MulDiv(712, c->scale, 100), MulDiv(by + 2, c->scale, 100),
                         MulDiv(58, c->scale, 100), MulDiv(bh - 4, c->scale, 100), SWP_NOZORDER);
        fmt_sync_from_caret(c);
    }

    center_over(hwnd, owner, winW, winH);
    EnableWindow(owner, FALSE);
    ShowWindow(hwnd, SW_SHOW);
    SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(c->hTitle);
    UpdateWindow(hwnd);

    while (c->alive) {
        if (!PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            WaitMessage();
            continue;
        }
        if (msg.message == WM_QUIT) {
            PostQuitMessage((int)msg.wParam);
            break;
        }
        if (msg.message == WM_KEYDOWN) {
            int inNote = (msg.hwnd == c->hNote || (c->rich && msg.hwnd == c->hNote));
            if (!inNote) {
                if (msg.wParam == VK_ESCAPE) {
                    SendMessageW(hwnd, WM_COMMAND, IDE_CANCEL, 0);
                    continue;
                }
                if (msg.wParam == VK_RETURN) {
                    SendMessageW(hwnd, WM_COMMAND, IDE_OK, 0);
                    continue;
                }
            }
        }
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    result = c->result;
    if (result == 1) *item = c->item;
    g_edit = NULL;
    free(c);
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (result == 1) model_normalize(item);
    return result;
}

int edit_show(HWND owner, Schedule *item, int isNew)
{
    return edit_run(owner, item, isNew);
}

int edit_show_at(HWND owner, Schedule *item, const SYSTEMTIME *prefStart)
{
    if (prefStart) item->start = *prefStart;
    return edit_run(owner, item, 1);
}
