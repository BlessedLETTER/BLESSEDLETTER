/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/strbuf.h"
#include "core/fmt_time.h"


const wchar_t *dt_text(const SYSTEMTIME *st, wchar_t *buf, size_t cch)
{
    WBuf b;
    wb_init(&b, buf, cch);
    wb_int(&b, st->wYear); wb_ch(&b, L'-');
    wb_u2(&b, st->wMonth); wb_ch(&b, L'-');
    wb_u2(&b, st->wDay); wb_ch(&b, L' ');
    wb_u2(&b, st->wHour); wb_ch(&b, L':');
    wb_u2(&b, st->wMinute); wb_ch(&b, L':');
    wb_u2(&b, st->wSecond);
    return buf;
}

const wchar_t *timet_text(time_t t, wchar_t *buf, size_t cch)
{
    struct tm *pt;
    WBuf b;
    if (t <= 0) { wb_init(&b, buf, cch); wb_add(&b, L"—"); return buf; }
    pt = localtime(&t);
    if (!pt) { wb_init(&b, buf, cch); wb_add(&b, L"—"); return buf; }
    wb_init(&b, buf, cch);
    wb_int(&b, pt->tm_year + 1900); wb_ch(&b, L'-');
    wb_u2(&b, pt->tm_mon + 1); wb_ch(&b, L'-');
    wb_u2(&b, pt->tm_mday); wb_ch(&b, L' ');
    wb_u2(&b, pt->tm_hour); wb_ch(&b, L':');
    wb_u2(&b, pt->tm_min); wb_ch(&b, L':');
    wb_u2(&b, pt->tm_sec);
    return buf;
}

const wchar_t *countdown_text(time_t occ, wchar_t *buf, size_t cch)
{
    WBuf b;
    time_t now, diff;
    long long days, hh, mi, ss;
    wb_init(&b, buf, cch);
    if (occ <= 0) { wb_add(&b, L"—"); return buf; }
    now = time(NULL);
    diff = occ - now;
    if (diff <= 0) { wb_add(&b, L"已过期"); return buf; }
    days = (long long)(diff / 86400);
    hh = (long long)((diff % 86400) / 3600);
    mi = (long long)((diff % 3600) / 60);
    ss = (long long)(diff % 60);
    if (days > 0) {
        wb_int(&b, days);
        wb_add(&b, L"天 ");
    }
    wb_u2(&b, (unsigned)hh); wb_ch(&b, L':');
    wb_u2(&b, (unsigned)mi); wb_ch(&b, L':');
    wb_u2(&b, (unsigned)ss);
    return buf;
}

const wchar_t *date_text(time_t t, wchar_t *buf, size_t cch)
{
    struct tm *pt;
    WBuf b;
    if (t <= 0) { wb_init(&b, buf, cch); wb_add(&b, L"—"); return buf; }
    pt = localtime(&t);
    if (!pt) { wb_init(&b, buf, cch); wb_add(&b, L"—"); return buf; }
    wb_init(&b, buf, cch);
    wb_int(&b, pt->tm_year + 1900); wb_ch(&b, L'-');
    wb_u2(&b, pt->tm_mon + 1); wb_ch(&b, L'-');
    wb_u2(&b, pt->tm_mday);
    return buf;
}

const wchar_t *clock_text(time_t t, wchar_t *buf, size_t cch)
{
    struct tm *pt;
    WBuf b;
    if (t <= 0) { wb_init(&b, buf, cch); wb_add(&b, L"—"); return buf; }
    pt = localtime(&t);
    if (!pt) { wb_init(&b, buf, cch); wb_add(&b, L"—"); return buf; }
    wb_init(&b, buf, cch);
    wb_u2(&b, pt->tm_hour); wb_ch(&b, L':');
    wb_u2(&b, pt->tm_min);
    return buf;
}

/* 拆解倒计时，state: 0=正常 1=已过期 2=无 */
