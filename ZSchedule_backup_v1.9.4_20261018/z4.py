# -*- coding: utf-8 -*-
import io

def load(p):
    try:
        return io.open(p, encoding='utf-8').read(), 'utf-8'
    except UnicodeDecodeError:
        return io.open(p, encoding='gbk').read(), 'gbk'

def save(p, txt, enc):
    io.open(p, 'w', encoding=enc, newline='').write(txt)

def rep(src, old, new, n=1):
    assert src.count(old) == n, ('match', src.count(old), repr(old[:60]))
    return src.replace(old, new)

# ---------- 1) 倒数日文案 helper ----------
p = 'model.c'
src, enc = load(p)
src = rep(src, """int sched_anniv_days(const Schedule *s, int *pFuture)""",
"""/* \u7eaa\u5ff5\u65e5 / \u5012\u6570\u65e5\u7684\u4e00\u53e5\u8bdd\u6587\u6848\uff1a\u65e2\u7b97\u201c\u8fd8\u6709\u591a\u5c11\u5929\u201d\u4e5f\u7b97\u201c\u5df2\u8fc7\u53bb\u591a\u5c11\u5929\u201d */
const wchar_t *sched_anniv_text(const Schedule *s, wchar_t *buf, size_t cch)
{
    WBuf b;
    int future = 0, d;
    int roundNo = 0;
    time_t next;
    if (!s || !buf) return L"";
    d = sched_anniv_days(s, &future);
    wb_init(&b, buf, cch);
    if (d == 0 && future != 1)       wb_add(&b, L"\\u5c31\\u662f\\u4eca\\u5929");
    else if (future) { wb_add(&b, L"\\u8fd8\\u6709 "); wb_int(&b, d); wb_add(&b, L" \\u5929"); }
    else { wb_add(&b, L"\\u5df2\\u8fc7\\u53bb "); wb_int(&b, d); wb_add(&b, L" \\u5929"); }
    next = sched_anniv_next_round(s, &roundNo);
    if (next > 0 && roundNo > 0 && s->repKind != REP_NONE) {
        struct tm *pt;
        time_t now = time(NULL);
        int y1, m1, d1, y2, m2, d2;
        pt = localtime(&now);
        if (pt) {
            y1 = pt->tm_year + 1900; m1 = pt->tm_mon + 1; d1 = pt->tm_mday;
            pt = localtime(&next);
            if (pt) {
                y2 = pt->tm_year + 1900; m2 = pt->tm_mon + 1; d2 = pt->tm_mday;
                wb_add(&b, L"\\uff08\\u8ddd\\u4e0b\\u6b21 ");
                wb_int(&b, (long long)(civil_days(y2, m2, d2) - civil_days(y1, m1, d1)));
                wb_add(&b, L" \\u5929\\uff09");
            }
        }
    }
    return buf;
}

int sched_anniv_days(const Schedule *s, int *pFuture)""")
save(p, src, enc)
print('model.c ok')

p = 'zsched.h'
src, enc = load(p)
src = rep(src, "int         sched_anniv_days(const Schedule *s, int *pFuture);",
"""int         sched_anniv_days(const Schedule *s, int *pFuture);
const wchar_t *sched_anniv_text(const Schedule *s, wchar_t *buf, size_t cch);""")
save(p, src, enc)
print('zsched.h ok')

# ---------- 2) 便笺墙：倒数日文案 + 多张图片 ----------
p = 'stickies.c'
src, enc = load(p)
src = rep(src, """            if (sc->kind == KIND_ANNIV) {
                wb_init(&b, buf, 96);
                wb_add(&b, L"\\u5df2\\u8fc7\\u53bb ");
                wb_int(&b, (long long)sched_anniv_elapsed(sc, NULL, NULL, NULL));
                wb_add(&b, L" \\u5929");
            } else if (sc->done) {""",
"""            if (sc->kind == KIND_ANNIV) {
                wb_init(&b, buf, 96);
                wb_add(&b, sched_anniv_text(sc, buf2, 96));
            } else if (sc->done) {""")
src = rep(src, """            wchar_t buf[96], b1[64];
            WBuf b;
            HFONT hf = st_cf_font(ff, L.cf);""",
"""            wchar_t buf[96], buf2[96], b1[64];
            WBuf b;
            HFONT hf = st_cf_font(ff, L.cf);""")

# 多张图片：在图片框里纵向排开
src = rep(src, """    if (L.img.right > L.img.left) {        /* \u63d2\u5165\u7684\u7167\u7247\uff1a\u4f4d\u7f6e\u4e0e\u5927\u5c0f\u90fd\u53ef\u81ea\u5df1\u62d6 */
        wchar_t ipath[MAX_PATH];
        if (sched_first_image(sc, ipath, MAX_PATH)) {
            RECT ib = L.img;
            img_draw_cover(hdc, ipath, ib.left, ib.top,
                           ib.right - ib.left, ib.bottom - ib.top);""",
"""    if (L.img.right > L.img.left) {        /* \u63d2\u5165\u7684\u7167\u7247\uff1a\u53ef\u4ee5\u591a\u5f20\uff0c\u5728\u7167\u7247\u6846\u91cc\u7ad6\u5411\u6392\u5f00 */
        wchar_t ipath[MAX_PATH];
        int nImg = 0, ii;
        for (ii = 0; ii < sc->attachCount && ii < MAX_ATTACH; ii++)
            if (sc->attach[ii][0] && img_is_image(sc->attach[ii])) nImg++;
        if (nImg > 0 && sched_first_image(sc, ipath, MAX_PATH)) {
            RECT ib = L.img;
            if (nImg > 1) {
                int slice = (ib.bottom - ib.top) / nImg;
                int k = 0;
                for (ii = 0; ii < sc->attachCount && ii < MAX_ATTACH; ii++) {
                    RECT ib2;
                    if (!sc->attach[ii][0] || !img_is_image(sc->attach[ii])) continue;
                    ib2 = ib;
                    ib2.top = ib.top + k * slice;
                    ib2.bottom = (k == nImg - 1) ? ib.bottom : ib2.top + slice - 2;
                    img_draw_cover(hdc, sc->attach[ii], ib2.left, ib2.top,
                                   ib2.right - ib2.left, ib2.bottom - ib2.top);
                    {
                        HPEN pen2 = CreatePen(PS_SOLID, 1, paper_shade(paper, -34));
                        HPEN op2 = (HPEN)SelectObject(hdc, pen2);
                        HBRUSH ob2 = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                        Rectangle(hdc, ib2.left, ib2.top, ib2.right - 1, ib2.bottom - 1);
                        SelectObject(hdc, ob2);
                        SelectObject(hdc, op2);
                        DeleteObject(pen2);
                    }
                    k++;
                }
            } else {
            img_draw_cover(hdc, ipath, ib.left, ib.top,
                           ib.right - ib.left, ib.bottom - ib.top);""")
src = rep(src, """            if (imgActive) {               /* \u60ac\u505c/\u62d6\u52a8\u65f6\u7ed9\u51fa\u660e\u786e\u7684\u201c\u53ef\u79fb\u52a8\u53ef\u7f29\u653e\u201d\u63d0\u793a */""",
"""            }
            if (imgActive) {               /* \u60ac\u505c/\u62d6\u52a8\u65f6\u7ed9\u51fa\u660e\u786e\u7684\u201c\u53ef\u79fb\u52a8\u53ef\u7f29\u653e\u201d\u63d0\u793a */""")
save(p, src, enc)
print('stickies.c ok')

# ---------- 3) 桌面便笺：倒数日文案 ----------
p = 'noteview.c'
src, enc = load(p)
src = rep(src, """                if (s->kind == KIND_ANNIV) {
                    int yy, mm, dd, tot;
                    WBuf b3;
                    tot = sched_anniv_elapsed(s, &yy, &mm, &dd);
                    wb_init(&b3, buf, 220);
                    wb_add(&b3, L"\\u5df2\\u8fc7\\u53bb ");
                    wb_int(&b3, tot);
                    wb_add(&b3, L" \\u5929");
                    if (yy > 0) {
                        wb_add(&b3, L"\\uff08");
                        wb_int(&b3, yy);
                        wb_add(&b3, L" \\u5e74 ");
                        wb_int(&b3, mm);
                        wb_add(&b3, L" \\u4e2a\\u6708\\uff09");
                    }
                    DrawTextW(hdc, buf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);""",
"""                if (s->kind == KIND_ANNIV) {
                    WBuf b3;
                    int yy, mm, dd;
                    wb_init(&b3, buf, 220);
                    wb_add(&b3, sched_anniv_text(s, buf2, 160));
                    if (sched_anniv_days(s, NULL) > 0) {
                        /* \u5c06\u6765\u7684\u5012\u6570\u65e5\u4e0d\u518d\u9644\u5e74\u6708 */
                    } else {
                        sched_anniv_elapsed(s, &yy, &mm, &dd);
                        if (yy > 0) {
                            wb_add(&b3, L"\\uff08");
                            wb_int(&b3, yy);
                            wb_add(&b3, L" \\u5e74 ");
                            wb_int(&b3, mm);
                            wb_add(&b3, L" \\u4e2a\\u6708\\uff09");
                        }
                    }
                    DrawTextW(hdc, buf, -1, &r2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);""")
src = rep(src, """            RECT box = cr;
            wchar_t b2[128], buf[220];""",
"""            RECT box = cr;
            wchar_t b2[128], buf[220], buf2[160];""")
save(p, src, enc)
print('noteview.c ok')

# ---------- 4) 表格与面板 ----------
p = 'ui_main.c'
src, enc = load(p)
src = rep(src, """        if (s->kind == KIND_TODO) {
            it.pszText = (LPWSTR)(s->done ? L"\\u5df2\\u5b8c\\u6210" : L"\\u5f85\\u529e");
        } else if (s->kind == KIND_ANNIV) {
            WBuf wb;
            wb_init(&wb, b1, 128);
            wb_add(&wb, L"\\u5df2\\u8fc7\\u53bb ");
            wb_int(&wb, (long long)sched_anniv_elapsed(s, NULL, NULL, NULL));
            wb_add(&wb, L" \\u5929");
            it.pszText = b1;
        }
        else if (s->done)     it.pszText = (LPWSTR)L"\\u5df2\\u5b8c\\u6210";""",
"""        if (s->kind == KIND_TODO) {
            it.pszText = (LPWSTR)(s->done ? L"\\u5df2\\u5b8c\\u6210" : L"\\u5f85\\u529e");
        } else if (s->kind == KIND_ANNIV) {
            it.pszText = (LPWSTR)sched_anniv_text(s, b1, 128);
        }
        else if (s->done)     it.pszText = (LPWSTR)L"\\u5df2\\u5b8c\\u6210";""")
src = rep(src, """            else if (s->kind == KIND_ANNIV) {
                WBuf wb;
                wb_init(&wb, b1, 128);
                wb_add(&wb, L"\\u5df2\\u8fc7\\u53bb ");
                wb_int(&wb, (long long)sched_anniv_elapsed(s, NULL, NULL, NULL));
                wb_add(&wb, L" \\u5929");
                it.pszText = b1;
            }""",
"""            else if (s->kind == KIND_ANNIV) {
                it.pszText = (LPWSTR)sched_anniv_text(s, b1, 128);
            }""")
src = rep(src, """                if (s && s->kind == KIND_ANNIV) {
                    WBuf b;
                    SelectObject(mem, g_fHuge);
                    SetTextColor(mem, accent);
                    wb_init(&b, buf, 160);
                    wb_add(&b, L"\\u5df2\\u8fc7\\u53bb ");
                    wb_int(&b, (long long)sched_anniv_elapsed(s, NULL, NULL, NULL));
                    wb_add(&b, L" \\u5929");""",
"""                if (s && s->kind == KIND_ANNIV) {
                    WBuf b;
                    SelectObject(mem, g_fHuge);
                    SetTextColor(mem, accent);
                    wb_init(&b, buf, 160);
                    wb_add(&b, sched_anniv_text(s, buf + 90, 60));""")
save(p, src, enc)
print('ui_main.c ok')
