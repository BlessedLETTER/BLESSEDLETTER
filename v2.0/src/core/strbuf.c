/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/strbuf.h"


void ws_init(WStr *s)
{
    s->p = NULL;
    s->len = 0;
    s->cap = 0;
}

void ws_free(WStr *s)
{
    if (s->p) free(s->p);
    ws_init(s);
}

int ws_reserve(WStr *s, size_t need)
{
    size_t nc;
    wchar_t *np;
    if (s->len + need + 1 <= s->cap) return 0;
    nc = s->cap ? s->cap : 256;
    while (nc < s->len + need + 1) nc *= 2;
    np = (wchar_t *)realloc(s->p, nc * sizeof(wchar_t));
    if (!np) return -1;
    s->p = np;
    s->cap = nc;
    s->p[s->len] = 0;
    return 0;
}

void ws_add(WStr *s, const wchar_t *txt)
{
    size_t n;
    if (!txt) return;
    n = wcslen(txt);
    if (ws_reserve(s, n) != 0) return;
    memcpy(s->p + s->len, txt, n * sizeof(wchar_t));
    s->len += n;
    s->p[s->len] = 0;
}

void ws_ch(WStr *s, wchar_t c)
{
    if (ws_reserve(s, 1) != 0) return;
    s->p[s->len++] = c;
    s->p[s->len] = 0;
}

void ws_int(WStr *s, long long v)
{
    wchar_t tmp[32];
    int i = 0, neg = 0;
    unsigned long long u;
    if (v < 0) { neg = 1; u = (unsigned long long)(-(v + 1)) + 1ULL; }
    else       { u = (unsigned long long)v; }
    do { tmp[i++] = (wchar_t)(L'0' + (u % 10)); u /= 10; } while (u && i < 30);
    if (neg) tmp[i++] = L'-';
    while (i > 0) ws_ch(s, tmp[--i]);
}

void ws_u2(WStr *s, unsigned v)
{
    ws_ch(s, (wchar_t)(L'0' + (v / 10) % 10));
    ws_ch(s, (wchar_t)(L'0' + v % 10));
}

void ws_u3(WStr *s, unsigned v)
{
    ws_ch(s, (wchar_t)(L'0' + (v / 100) % 10));
    ws_ch(s, (wchar_t)(L'0' + (v / 10) % 10));
    ws_ch(s, (wchar_t)(L'0' + v % 10));
}

/* ---- 定长缓冲写入器 ---- */
void wb_init(WBuf *b, wchar_t *buf, size_t cap)
{
    b->buf = buf;
    b->cap = cap;
    b->len = 0;
    if (cap) buf[0] = 0;
}

void wb_add(WBuf *b, const wchar_t *txt)
{
    size_t n;
    if (!b->buf || !txt) return;
    n = wcslen(txt);
    if (b->len + n >= b->cap) n = (b->cap > b->len + 1) ? (b->cap - b->len - 1) : 0;
    if (n) {
        memcpy(b->buf + b->len, txt, n * sizeof(wchar_t));
        b->len += n;
    }
    b->buf[b->len] = 0;
}

void wb_ch(WBuf *b, wchar_t c)
{
    if (!b->buf || b->len + 1 >= b->cap) return;
    b->buf[b->len++] = c;
    b->buf[b->len] = 0;
}

void wb_int(WBuf *b, long long v)
{
    wchar_t tmp[32];
    int i = 0, neg = 0;
    unsigned long long u;
    if (v < 0) { neg = 1; u = (unsigned long long)(-(v + 1)) + 1ULL; }
    else       { u = (unsigned long long)v; }
    do { tmp[i++] = (wchar_t)(L'0' + (u % 10)); u /= 10; } while (u && i < 30);
    if (neg) tmp[i++] = L'-';
    while (i > 0) wb_ch(b, tmp[--i]);
}

void wb_u2(WBuf *b, unsigned v)
{
    wb_ch(b, (wchar_t)(L'0' + (v / 10) % 10));
    wb_ch(b, (wchar_t)(L'0' + v % 10));
}

void wb_u3(WBuf *b, unsigned v)
{
    wb_ch(b, (wchar_t)(L'0' + (v / 100) % 10));
    wb_ch(b, (wchar_t)(L'0' + (v / 10) % 10));
    wb_ch(b, (wchar_t)(L'0' + v % 10));
}

/* =======================================================================
 *  二、日期基础算法（与本地时区无关的“自然日序号”，1970-01-01 = 0）
 * =====================================================================*/
