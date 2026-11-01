/* ============================================================================
 *  ZSchedule - model.c
 *  数据模型、周期（重复）推算、倒计时计算、文本格式化、文件读写
 * ==========================================================================*/
#include "zsched.h"
#ifdef RTF_TRACE
#  include <stdio.h>
#endif
#ifdef RTF_TRACE
#  include <stdio.h>
#endif

/* =======================================================================
 *  一、可增长的宽字符缓冲区
 * =====================================================================*/
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
long long civil_days(int y, int m, int d)
{
    long long yy = y;
    long long era;
    unsigned yoe, doy, doe;
    yy -= (m <= 2);
    era = (yy >= 0 ? yy : yy - 399) / 400;
    yoe = (unsigned)(yy - era * 400);
    doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (long long)doe - 719468;
}

void civil_from_days(long long z, int *py, int *pm, int *pd)
{
    long long era, y;
    unsigned doe, yoe, doy, mp, d, m;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = (long long)yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp + (mp < 10 ? 3 : -9);
    *py = (int)(y + (m <= 2));
    *pm = (int)m;
    *pd = (int)d;
}

static int is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int days_in_month(int y, int m)
{
    static const int t[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m < 1 || m > 12) return 30;
    if (m == 2 && is_leap(y)) return 29;
    return t[m - 1];
}

/* 0 = 周日, 1 = 周一 … 6 = 周六 */
static int weekday_of_ord(long long ord)
{
    return (int)(((ord % 7) + 7 + 4) % 7);
}

int weekday_of_day(long long ord) { return weekday_of_ord(ord); }

/* 所在周的周一（自然日序号） */
long long monday_of(long long ord)
{
    return ord - ((weekday_of_ord(ord) + 6) % 7);
}

long long today_ord(void)
{
    time_t t = time(NULL);
    struct tm *pt = localtime(&t);
    if (!pt) return 0;
    return civil_days(pt->tm_year + 1900, pt->tm_mon + 1, pt->tm_mday);
}

long long sched_start_day(const Schedule *s)
{
    return civil_days(s->start.wYear, s->start.wMonth, s->start.wDay);
}

static time_t systime_to_t(const SYSTEMTIME *st)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = st->wYear - 1900;
    t.tm_mon  = st->wMonth - 1;
    t.tm_mday = st->wDay;
    t.tm_hour = st->wHour;
    t.tm_min  = st->wMinute;
    t.tm_sec  = st->wSecond;
    t.tm_isdst = -1;
    return mktime(&t);
}

static time_t time_from_ymd(int y, int m, int d, int hh, int mi, int ss)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900;
    t.tm_mon  = m - 1;
    t.tm_mday = d;
    t.tm_hour = hh;
    t.tm_min  = mi;
    t.tm_sec  = ss;
    t.tm_isdst = -1;
    return mktime(&t);
}

time_t time_from_ord(long long ord, int hh, int mi, int ss)
{
    int y, m, d;
    civil_from_days(ord, &y, &m, &d);
    return time_from_ymd(y, m, d, hh, mi, ss);
}

int sched_day_count(time_t from, time_t to)
{
    struct tm a, b;
    struct tm *pt;
    long long da, db;
    pt = localtime(&from); if (!pt) return 0; a = *pt;
    pt = localtime(&to);   if (!pt) return 0; b = *pt;
    da = civil_days(a.tm_year + 1900, a.tm_mon + 1, a.tm_mday);
    db = civil_days(b.tm_year + 1900, b.tm_mon + 1, b.tm_mday);
    return (int)(db - da);
}

/* =======================================================================
 *  三、周期推算：求“下一次发生时刻”
 * =====================================================================*/
time_t sched_next_occurrence(const Schedule *s, time_t now)
{
    time_t start;
    int N;
    long long startOrd, todayOrd, diff, k, kk, dbest;
    struct tm ntm;

    if (!s || !s->enabled || s->done) return 0;
    if (s->kind == KIND_TODO) return 0;      /* 无时间待办：没有“下次发生” */
    if (s->kind == KIND_ANNIV && s->repKind != REP_CUSTOM) return 0;

    start = systime_to_t(&s->start);
    if (start == (time_t)-1) return 0;

    /* 尚未开始 → 就是开始时刻 */
    if (start >= now) return start;

    /* 一次性日程且已过 → 无下一次 */
    if (s->repKind == REP_NONE) return 0;

    N = s->repInterval;
    if (N < 1) N = 1;

    {
        struct tm *pt = localtime(&now);
        if (!pt) return 0;
        ntm = *pt;
    }
    todayOrd = civil_days(ntm.tm_year + 1900, ntm.tm_mon + 1, ntm.tm_mday);
    startOrd = civil_days(s->start.wYear, s->start.wMonth, s->start.wDay);

    /* ---- 每周（可多选星期几）---- */
    if (s->repKind == REP_WEEKLY) {
        int wd0 = weekday_of_ord(startOrd);
        long long anchorMon = startOrd - ((wd0 + 6) % 7);   /* 以周一为一周起点 */
        for (kk = 0; kk <= 7 * N + 7; kk++) {
            long long d = todayOrd + kk;
            int wd;
            long long mon, weeks;
            time_t cand;
            if (s->weekMask == 0) break;
            wd = weekday_of_ord(d);
            if (!(s->weekMask & (1 << wd))) continue;
            mon = d - ((wd + 6) % 7);
            weeks = (mon - anchorMon) / 7;
            if (weeks < 0) continue;
            if (N > 1 && (weeks % N) != 0) continue;
            cand = time_from_ord(d, s->start.wHour, s->start.wMinute, s->start.wSecond);
            if (cand >= now && cand >= start) return cand;
        }
        return 0;
    }

    /* ---- 自定义：每 N 天 / 周 ---- */
    if (s->repUnit == UNIT_DAY || s->repUnit == UNIT_WEEK) {
        long long step = (s->repUnit == UNIT_DAY) ? N : 7LL * N;
        diff = todayOrd - startOrd;
        if (diff < 0) diff = 0;
        k = diff / step;
        dbest = 0;
        for (kk = k; kk <= k + 2; kk++) {
            long long d = startOrd + kk * step;
            time_t cand;
            if (d < startOrd) continue;
            cand = time_from_ord(d, s->start.wHour, s->start.wMinute, s->start.wSecond);
            if (cand >= now && cand >= start) { dbest = d; return cand; }
        }
        (void)dbest;
        return 0;
    }

    /* ---- 自定义：每 N 月 ---- */
    if (s->repUnit == UNIT_MONTH) {
        long long totM = (long long)(ntm.tm_year - (s->start.wYear - 1900)) * 12
                       + (long long)(ntm.tm_mon - (s->start.wMonth - 1));
        if (totM < 0) totM = 0;
        k = totM / N;
        for (kk = k; kk <= k + 2; kk++) {
            long long mo = (long long)(s->start.wMonth - 1) + kk * N;
            int y = s->start.wYear + (int)(mo / 12);
            int m = (int)(mo % 12) + 1;
            int d = s->start.wDay;
            time_t cand;
            if (d > days_in_month(y, m)) d = days_in_month(y, m);
            cand = time_from_ymd(y, m, d, s->start.wHour, s->start.wMinute, s->start.wSecond);
            if (cand >= now && cand >= start) return cand;
        }
        return 0;
    }

    /* ---- 自定义：每 N 年 ---- */
    {
        long long totY = ntm.tm_year - (s->start.wYear - 1900);
        if (totY < 0) totY = 0;
        k = totY / N;
        for (kk = k; kk <= k + 2; kk++) {
            int y = s->start.wYear + (int)(kk * N);
            int m = s->start.wMonth;
            int d = s->start.wDay;
            time_t cand;
            if (d > days_in_month(y, m)) d = days_in_month(y, m);
            cand = time_from_ymd(y, m, d, s->start.wHour, s->start.wMinute, s->start.wSecond);
            if (cand >= now && cand >= start) return cand;
        }
        return 0;
    }
}

time_t sched_occurrence_end(const Schedule *s, time_t occ)
{
    if (!s || !s->hasEnd || s->durSec <= 0) return occ;
    return occ + s->durSec;
}

/* 判断某自然日（序号）该日程是否发生；发生则返回当天的时间点，否则 0。
   —— 供“按周×小时段”网格视图把重复日程展开到具体日期使用。 */
time_t sched_occurrence_on(const Schedule *s, long long dayOrd)
{
    long long startOrd;
    int N;

    if (!s || !s->enabled || s->done) return 0;
    if (s->kind == KIND_TODO) return 0;      /* 无时间待办不上周表 */
    startOrd = sched_start_day(s);
    if (dayOrd < startOrd) return 0;
    N = s->repInterval;
    if (N < 1) N = 1;

    switch (s->repKind) {
    case REP_NONE:
        return (dayOrd == startOrd)
             ? time_from_ord(dayOrd, s->start.wHour, s->start.wMinute, s->start.wSecond)
             : 0;

    case REP_WEEKLY: {
        int wd = weekday_of_ord(dayOrd);
        long long weeks;
        if (!(s->weekMask & (1 << wd))) return 0;
        weeks = (monday_of(dayOrd) - monday_of(startOrd)) / 7;
        if (weeks < 0 || (weeks % N) != 0) return 0;
        return time_from_ord(dayOrd, s->start.wHour, s->start.wMinute, s->start.wSecond);
    }

    default: /* REP_CUSTOM */
        switch (s->repUnit) {
        case UNIT_DAY:
            if (((dayOrd - startOrd) % N) != 0) return 0;
            return time_from_ord(dayOrd, s->start.wHour, s->start.wMinute, s->start.wSecond);
        case UNIT_WEEK:
            if (((dayOrd - startOrd) % (7LL * N)) != 0) return 0;
            return time_from_ord(dayOrd, s->start.wHour, s->start.wMinute, s->start.wSecond);
        case UNIT_MONTH: {
            int y, m, d, dim, sd, sm, sy;
            civil_from_days(dayOrd, &y, &m, &d);
            sy = s->start.wYear; sm = s->start.wMonth;
            if ((y - sy) * 12 + (m - sm) < 0) return 0;
            if ((((y - sy) * 12 + (m - sm)) % N) != 0) return 0;
            dim = days_in_month(y, m);
            sd = s->start.wDay;
            if (sd > dim) sd = dim;
            if (d != sd) return 0;
            return time_from_ord(dayOrd, s->start.wHour, s->start.wMinute, s->start.wSecond);
        }
        default: { /* UNIT_YEAR */
            int y, m, d, dim, sd;
            civil_from_days(dayOrd, &y, &m, &d);
            if (((y - s->start.wYear) % N) != 0) return 0;
            if (m != s->start.wMonth) return 0;
            dim = days_in_month(y, m);
            sd = s->start.wDay;
            if (sd > dim) sd = dim;
            if (d != sd) return 0;
            return time_from_ord(dayOrd, s->start.wHour, s->start.wMinute, s->start.wSecond);
        }
        }
    }
}

/* =======================================================================
 *  四、文本格式化
 * =====================================================================*/
/* ======================================================================
 *  纪念日：记录“已经过去的事”
 * =====================================================================*/

/* 已经过去多久（按日历差，自动处理大小月与闰年）；返回总天数 */
/* 倒数日：返回距离纪念日的天数绝对值；*pFuture = 1 表示将来（还有 N 天），
   0 表示已经过去（已过去 N 天）；当天返回 0 且 *pFuture = 2 */
/* 纪念日 / 倒数日的一句话文案：既算“还有多少天”也算“已过去多少天” */
const wchar_t *sched_anniv_text(const Schedule *s, wchar_t *buf, size_t cch)
{
    WBuf b;
    int future = 0, d;
    int roundNo = 0;
    time_t next;
    if (!s || !buf) return L"";
    d = sched_anniv_days(s, &future);
    wb_init(&b, buf, cch);
    if (d == 0)                      wb_add(&b, L"\u5c31\u662f\u4eca\u5929");
    else if (future) { wb_add(&b, L"\u8fd8\u6709 "); wb_int(&b, d); wb_add(&b, L" \u5929"); }
    else { wb_add(&b, L"\u5df2\u8fc7\u53bb "); wb_int(&b, d); wb_add(&b, L" \u5929"); }
    next = sched_anniv_next_round(s, &roundNo);
    /* 还没到的日子不算“距下次”（那会和“还有 N 天”重复） */
    if (!future && next > 0 && roundNo > 0 && s->repKind != REP_NONE) {
        struct tm *pt;
        time_t now = time(NULL);
        int y1, m1, d1, y2, m2, d2;
        pt = localtime(&now);
        if (pt) {
            y1 = pt->tm_year + 1900; m1 = pt->tm_mon + 1; d1 = pt->tm_mday;
            pt = localtime(&next);
            if (pt) {
                y2 = pt->tm_year + 1900; m2 = pt->tm_mon + 1; d2 = pt->tm_mday;
                wb_add(&b, L"\uff08\u8ddd\u4e0b\u6b21 ");
                wb_int(&b, (long long)(civil_days(y2, m2, d2) - civil_days(y1, m1, d1)));
                wb_add(&b, L" \u5929\uff09");
            }
        }
    }
    return buf;
}

int sched_anniv_days(const Schedule *s, int *pFuture)
{
    time_t t, now;
    int sign;
    if (!s) return 0;
    t = systime_to_t(&s->start);
    now = time(NULL);
    if (pFuture) *pFuture = 0;
    sign = sched_anniv_elapsed(s, NULL, NULL, NULL);
    if (t > now) {
        int d = -sign < 0 ? sign : -sign;
        (void)d;
        if (pFuture) *pFuture = 1;
        /* 还没到：用两个日期相减 */
        {
            struct tm *pt = localtime(&now);
            int y, m, dd, sy, sm, sd;
            if (!pt) return 0;
            y = pt->tm_year + 1900; m = pt->tm_mon + 1; dd = pt->tm_mday;
            sy = s->start.wYear; sm = s->start.wMonth; sd = s->start.wDay;
            return (int)(civil_days(sy, sm, sd) - civil_days(y, m, dd));
        }
    }
    if (pFuture) *pFuture = 0;
    return sign;
}

int sched_anniv_elapsed(const Schedule *s, int *years, int *months, int *days)
{
    SYSTEMTIME now;
    int y, m, d, tot;
    long long a, b;

    if (years) *years = 0;
    if (months) *months = 0;
    if (days) *days = 0;
    if (!s) return 0;

    GetLocalTime(&now);
    y = now.wYear - s->start.wYear;
    m = now.wMonth - s->start.wMonth;
    d = now.wDay - s->start.wDay;
    if (d < 0) {
        m--;
        d += days_in_month((now.wMonth == 1) ? now.wYear - 1 : now.wYear,
                           (now.wMonth == 1) ? 12 : now.wMonth - 1);
    }
    if (m < 0) { y--; m += 12; }
    if (y < 0) { y = 0; m = 0; d = 0; }

    if (years) *years = y;
    if (months) *months = m;
    if (days) *days = d;

    a = civil_days(s->start.wYear, s->start.wMonth, s->start.wDay);
    b = civil_days(now.wYear, now.wMonth, now.wDay);
    tot = (int)(b - a);
    return tot > 0 ? tot : 0;
}

/* 下一个周年时刻；roundNo 给出这是第几周年（1 起）。无年度重复时返回 0 */
time_t sched_anniv_next_round(const Schedule *s, int *roundNo)
{
    SYSTEMTIME now;
    int elapsed, n, y, m, d, dim;

    if (roundNo) *roundNo = 0;
    if (!s || s->kind != KIND_ANNIV) return 0;
    if (s->repKind != REP_CUSTOM || s->repUnit != UNIT_YEAR) return 0;

    GetLocalTime(&now);
    elapsed = now.wYear - s->start.wYear;
    n = elapsed + 1;
    if (n < 1) n = 1;
    y = s->start.wYear + n;
    m = s->start.wMonth;
    d = s->start.wDay;
    dim = days_in_month(y, m);
    if (d > dim) d = dim;
    if (roundNo) *roundNo = n;
    return time_from_ymd(y, m, d, s->start.wHour, s->start.wMinute, s->start.wSecond);
}

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
void countdown_split(time_t occ, long long *days, int *hh, int *mi, int *ss, int *state)
{
    time_t now, diff;
    if (days) *days = 0;
    if (hh) *hh = 0;
    if (mi) *mi = 0;
    if (ss) *ss = 0;
    if (occ <= 0) { if (state) *state = 2; return; }
    now = time(NULL);
    diff = occ - now;
    if (diff <= 0) { if (state) *state = 1; return; }
    if (state) *state = 0;
    if (days) *days = (long long)(diff / 86400);
    if (hh) *hh = (int)((diff % 86400) / 3600);
    if (mi) *mi = (int)((diff % 3600) / 60);
    if (ss) *ss = (int)(diff % 60);
}

/* ======================================================================
 *  倒计时显示精度：天 / 时 / 分 / 秒（可逐条选）
 * =====================================================================*/
int cd_unit_of(const Schedule *s)
{
    if (!s || s->cdUnit <= CDU_DEF || s->cdUnit > CDU_SEC) return CDU_SEC;
    return s->cdUnit;
}

/* 按精度拆解：最细一级向上取整（“还有”语义），秒级与旧版一致向下取整 */
static void cd_parts(time_t occ, int unit, long long *pd, int *ph, int *pm, int *ps)
{
    time_t diff = occ - time(NULL);
    long long d = 0;
    int h = 0, m = 0, ss = 0;
    if (diff < 0) diff = 0;
    if (unit == CDU_DAY) {
        d = (long long)((diff + 86399) / 86400);
    } else if (unit == CDU_HOUR) {
        long long th = (long long)((diff + 3599) / 3600);
        d = th / 24;
        h = (int)(th % 24);
    } else if (unit == CDU_MIN) {
        long long tm = (long long)((diff + 59) / 60);
        d = tm / 1440;
        h = (int)((tm % 1440) / 60);
        m = (int)(tm % 60);
    } else {
        d = (long long)(diff / 86400);
        h = (int)((diff % 86400) / 3600);
        m = (int)((diff % 3600) / 60);
        ss = (int)(diff % 60);
    }
    if (pd) *pd = d;
    if (ph) *ph = h;
    if (pm) *pm = m;
    if (ps) *ps = ss;
}

const wchar_t *cd_text_u(time_t occ, int unit, wchar_t *buf, size_t cch)
{
    WBuf b;
    long long d = 0;
    int h = 0, m = 0, ss = 0;
    wb_init(&b, buf, cch);
    if (occ <= 0) { wb_add(&b, L"—"); return buf; }
    if (occ - time(NULL) <= 0) { wb_add(&b, L"已过期"); return buf; }
    if (unit <= CDU_DEF || unit > CDU_SEC) unit = CDU_SEC;
    cd_parts(occ, unit, &d, &h, &m, &ss);
    if (d > 0) {
        wb_int(&b, d);
        wb_add(&b, unit == CDU_DAY ? L" 天" : L"天 ");
    }
    if (unit == CDU_DAY) return buf;
    if (unit == CDU_HOUR) {
        wb_u2(&b, (unsigned)h);
        wb_add(&b, L" 时");
        return buf;
    }
    wb_u2(&b, (unsigned)h);
    wb_ch(&b, L':');
    wb_u2(&b, (unsigned)m);
    if (unit == CDU_SEC) {
        wb_ch(&b, L':');
        wb_u2(&b, (unsigned)ss);
    }
    return buf;
}

const wchar_t *cd_text(const Schedule *s, time_t occ, wchar_t *buf, size_t cch)
{
    return cd_text_u(occ, cd_unit_of(s), buf, cch);
}

/* ======================================================================
 *  课表模式：今天这节课的时间窗口与上下节
 * =====================================================================*/
/* 今天这节课的 [开始, 结束)；1 = 今天有这节课。
   没填结束时间时按 TT_CLASS_MIN 分钟一节算。 */
int sched_class_window(const Schedule *s, time_t *pStart, time_t *pEnd)
{
    time_t st, en;
    if (!s || !s->timetable) return 0;
    if (!s->enabled || s->done) return 0;
    if (s->kind != KIND_TASK) return 0;
    st = sched_occurrence_on(s, today_ord());
    if (st <= 0) return 0;
    en = sched_occurrence_end(s, st);
    if (en <= st) en = st + (time_t)TT_CLASS_MIN * 60;
    if (pStart) *pStart = st;
    if (pEnd) *pEnd = en;
    return 1;
}

/* 此刻正在上的那节课（重叠时取最近开始的）；没有则 NULL */
Schedule *sched_class_now(void)
{
    time_t now = time(NULL), bestStart = 0;
    Schedule *best = NULL;
    int i;
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        time_t st, en;
        if (!sched_class_window(s, &st, &en)) continue;
        if (now < st || now >= en) continue;
        if (!best || st >= bestStart) { best = s; bestStart = st; }
    }
    return best;
}

/* after 之后今天最近的一节课 */
Schedule *sched_class_next(time_t after, time_t *pStart, time_t *pEnd)
{
    Schedule *best = NULL;
    time_t bestStart = 0, bestEnd = 0;
    int i;
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        time_t st, en;
        if (!sched_class_window(s, &st, &en)) continue;
        if (st <= after) continue;
        if (!best || st < bestStart) { best = s; bestStart = st; bestEnd = en; }
    }
    if (best) {
        if (pStart) *pStart = bestStart;
        if (pEnd) *pEnd = bestEnd;
    }
    return best;
}

/* after 之后的下一节课，不限今天：今天晚些的课优先，其次任何课表日程的下一场
   （明天 / 下周都算，用来在便笺上提示“下一节课是什么”） */
Schedule *sched_class_next_any(time_t after, time_t *pStart, time_t *pEnd)
{
    Schedule *best = NULL;
    time_t bs = 0, be = 0;
    int i;
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        time_t st, en;
        if (!s || !s->timetable) continue;
        if (!s->enabled || s->done || s->kind != KIND_TASK) continue;
        st = sched_occurrence_on(s, today_ord());
        if (st <= after) st = s->nextOcc;        /* 今天这场已经过去 → 看下一次发生 */
        if (st <= after) continue;
        en = sched_occurrence_end(s, st);
        if (en <= st) en = st + (time_t)TT_CLASS_MIN * 60;
        if (!best || st < bs) { best = s; bs = st; be = en; }
    }
    if (best) {
        if (pStart) *pStart = bs;
        if (pEnd) *pEnd = be;
    }
    return best;
}

/* 下一节课的时间文字：今天只看 时:分，明天/后天明确指出，更远带星期 */
const wchar_t *tt_when_text(time_t t, wchar_t *buf, size_t cch)
{
    static const wchar_t *wd[7] = { L"日", L"一", L"二", L"三", L"四", L"五", L"六" };
    struct tm *pt;
    long long d, d0;
    WBuf b;
    if (t <= 0) { wb_init(&b, buf, cch); wb_add(&b, L"—"); return buf; }
    pt = localtime(&t);
    if (!pt) { wb_init(&b, buf, cch); wb_add(&b, L"—"); return buf; }
    wb_init(&b, buf, cch);
    d = civil_days(pt->tm_year + 1900, pt->tm_mon + 1, pt->tm_mday);
    d0 = today_ord();
    if (d != d0) {
        if (d == d0 + 1)      wb_add(&b, L"明天 ");
        else if (d == d0 + 2) wb_add(&b, L"后天 ");
        else {
            wb_add(&b, L"周");
            wb_add(&b, wd[weekday_of_ord(d)]);
            wb_add(&b, L" ");
        }
    }
    wb_u2(&b, pt->tm_hour);
    wb_ch(&b, L':');
    wb_u2(&b, pt->tm_min);
    return buf;
}

/* ======================================================================
 *  RTF → 纯文本（用于列表摘要），只需覆盖 RichEdit 生成的字集
 * =====================================================================*/

void rtf_to_plain(const wchar_t *rtf, wchar_t *out, size_t cch)
{
    WBuf b;
    const wchar_t *p;
    int depth = 0, skipTo = -1;
    BYTE bytes[1024];          /* 连续 \'hh 累积到一起再按 CP_ACP 解码（中文一个字两字节） */
    int nb = 0;

    wb_init(&b, out, cch);
    if (!rtf || !rtf[0]) return;
    if (wcsncmp(rtf, L"{\\rtf", 5) != 0) {      /* 不是 RTF：原样返回 */
        wb_add(&b, rtf);
        return;
    }

#define RTF_FLUSH()                                                           \
    do {                                                                      \
        if (nb > 0) {                                                         \
            wchar_t tmp[1026];                                                \
            int cnt = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)bytes, nb,       \
                                          tmp, 1024);                         \
            int k;                                                            \
            if (cnt > 0) {                                                    \
                tmp[cnt] = 0;                                                 \
                for (k = 0; k < cnt; k++) wb_ch(&b, tmp[k]);                  \
            }                                                                 \
            nb = 0;                                                           \
        }                                                                     \
    } while (0)

#define RTF_IS_DEST(w)   (wcsncmp((w), L"fonttbl", 7) == 0 ||                 \
                          wcsncmp((w), L"colortbl", 8) == 0 ||                \
                          wcsncmp((w), L"stylesheet", 10) == 0 ||             \
                          wcsncmp((w), L"info", 4) == 0 ||                    \
                          wcsncmp((w), L"pict", 4) == 0 ||                    \
                          wcsncmp((w), L"object", 6) == 0 ||                  \
                          wcsncmp((w), L"header", 6) == 0 ||                  \
                          wcsncmp((w), L"footer", 6) == 0 ||                  \
                          wcsncmp((w), L"footnote", 8) == 0 ||                \
                          wcsncmp((w), L"field", 5) == 0)

    for (p = rtf; *p; ) {
        if (*p == L'{') {
            depth++;
            if (p[1] == L'\\') {
                const wchar_t *w = p + 2;
                if (*w == L'*' || RTF_IS_DEST(w)) skipTo = depth;
            }
            p++;
            continue;
        }
        if (*p == L'}') {
            if (skipTo >= 0 && depth == skipTo) skipTo = -1;
            depth--;
            if (depth < 0) depth = 0;
            p++;
            continue;
        }
        if (skipTo >= 0) { p++; continue; }

        if (*p == L'\\') {
            const wchar_t *q = p + 1;
            /* \\  \{  \} —— 字面量 */
            if (*q == L'\\' || *q == L'{' || *q == L'}') {
                RTF_FLUSH();
                wb_ch(&b, *q);
                p = q + 1;
                continue;
            }
            /* \'hh —— ANSI 字节，连续多个合并后按 CP_ACP 解码 */
            if (*q == L'\'') {
                int h1 = q[1], h2 = q[2], v = 0;
                if (h1 >= L'0' && h1 <= L'9') v = h1 - L'0';
                else if (h1 >= L'a' && h1 <= L'f') v = h1 - L'a' + 10;
                else if (h1 >= L'A' && h1 <= L'F') v = h1 - L'A' + 10;
                v <<= 4;
                if (h2 >= L'0' && h2 <= L'9') v |= h2 - L'0';
                else if (h2 >= L'a' && h2 <= L'f') v |= h2 - L'a' + 10;
                else if (h2 >= L'A' && h2 <= L'F') v |= h2 - L'A' + 10;
                if (nb >= (int)sizeof(bytes) - 4) RTF_FLUSH();
                bytes[nb++] = (BYTE)v;
                p = q + 3;
                continue;
            }
            if (*q == L'*') { p = q + 1; continue; }
            if (*q == L'~') { RTF_FLUSH(); wb_ch(&b, L' '); p = q + 1; continue; }
            if (*q == L'-' || *q == L'_') { p = q + 1; continue; }
            /* \uNNNN? —— 只有 u 后面紧跟数字或负号才是 Unicode 转义
               （\uc1 / \uc0 之类必须按普通控制字处理） */
            if ((*q == L'u' || *q == L'U') &&
                (q[1] == L'-' || (q[1] >= L'0' && q[1] <= L'9'))) {
                const wchar_t *d = q + 1;
                int sign = 1, v = 0;
                RTF_FLUSH();
                if (*d == L'-') { sign = -1; d++; }
                while (*d >= L'0' && *d <= L'9') { v = v * 10 + (*d - L'0'); d++; }
                if (*d == L'?') d++;
                if (v > 0) wb_ch(&b, (wchar_t)(short)(sign * v));
                p = d;
                continue;
            }
            /* 普通控制字：跳过词、可选负号、数字，以及分隔用的一个空格 */
            {
                const wchar_t *d = q;
                int n = 0, isPar = 0, isLine = 0, isTab = 0;
                while (d[n] && ((d[n] >= L'a' && d[n] <= L'z') ||
                                (d[n] >= L'A' && d[n] <= L'Z'))) n++;
                if (n == 3 && !wcsncmp(d, L"par", 3)) isPar = 1;
                else if (n == 4 && !wcsncmp(d, L"line", 4)) isLine = 1;
                else if (n == 3 && !wcsncmp(d, L"tab", 3)) isTab = 1;
                d += n;
                if (*d == L'-') d++;
                while (*d >= L'0' && *d <= L'9') d++;
                if (*d == L' ') d++;
                RTF_FLUSH();
                if (isPar || isLine) wb_ch(&b, L'\n');
                else if (isTab) wb_ch(&b, L'\t');
                p = d;
                continue;
            }
        }
        if (*p == L'\r' || *p == L'\n') { p++; continue; }   /* RTF 裸换行无意义 */
        RTF_FLUSH();
        wb_ch(&b, *p);
        p++;
    }
    RTF_FLUSH();
#undef RTF_FLUSH
#undef RTF_IS_DEST
}

const wchar_t *weekday_names_text(int mask, wchar_t *buf, size_t cch)
{
    static const wchar_t *nm[7] = { L"周日", L"周一", L"周二", L"周三",
                                    L"周四", L"周五", L"周六" };
    WBuf b;
    int i, first = 1;
    wb_init(&b, buf, cch);
    for (i = 0; i < 7; i++) {
        if (!(mask & (1 << i))) continue;
        if (!first) wb_ch(&b, L'/');
        wb_add(&b, nm[i]);
        first = 0;
    }
    if (first) wb_add(&b, L"—");
    return buf;
}

const wchar_t *repeat_text(const Schedule *s, wchar_t *buf, size_t cch)
{
    WBuf b;
    wb_init(&b, buf, cch);
    switch (s->repKind) {
    case REP_WEEKLY:
        wb_add(&b, s->repInterval > 1 ? L"每" : L"每");
        if (s->repInterval > 1) { wb_int(&b, s->repInterval); }
        wb_add(&b, L"周 ");
        {
            wchar_t tmp[64];
            wb_add(&b, weekday_names_text(s->weekMask, tmp, 64));
        }
        break;
    case REP_CUSTOM:
        wb_add(&b, L"每");
        if (s->repInterval > 1) wb_int(&b, s->repInterval);
        switch (s->repUnit) {
        case UNIT_DAY:   wb_add(&b, L"天"); break;
        case UNIT_WEEK:  wb_add(&b, L"周"); break;
        case UNIT_MONTH: wb_add(&b, L"个月"); break;
        default:         wb_add(&b, L"年"); break;
        }
        if (s->repUnit == UNIT_WEEK) {
            wchar_t tmp[64];
            wb_ch(&b, L' ');
            wb_add(&b, weekday_names_text(s->weekMask, tmp, 64));
        }
        break;
    default:
        wb_add(&b, L"不重复");
        break;
    }
    return buf;
}

const wchar_t *status_text(const Schedule *s, wchar_t *buf, size_t cch)
{
    WBuf b;
    wb_init(&b, buf, cch);
    if (s->done)             wb_add(&b, L"已完成");
    else if (!s->enabled)    wb_add(&b, L"已暂停");
    else                     wb_add(&b, L"进行中");
    return buf;
}

const wchar_t *file_name_of(const wchar_t *path, wchar_t *buf, size_t cch)
{
    const wchar_t *p = path, *q;
    WBuf b;
    for (q = path; *q; q++) {
        if (*q == L'\\' || *q == L'/') p = q + 1;
    }
    wb_init(&b, buf, cch);
    wb_add(&b, p);
    return buf;
}

/* =======================================================================
 *  五、数据模型
 * =====================================================================*/
static SchedList g_store;
static wchar_t   g_path[MAX_PATH];
static int       g_nextId = 1;
static int       g_legacy = 0;      /* 载入的是旧格式（颜色字段不可信），需要回写 */

/* 数据文件格式版本
 *   v1: color= 字段为“每通道取后两位十进制”的历史写法，信息有损，已废弃
 *   v2: color=RRGGBB（标准 16 进制），当前格式
 */
#define DATA_VERSION 4

void model_init(void)
{
    memset(&g_store, 0, sizeof(g_store));
    g_nextId = 1;
    g_path[0] = 0;
}

void model_free(void)
{
    if (g_store.items) free(g_store.items);
    memset(&g_store, 0, sizeof(g_store));
}

int model_count(void) { return g_store.count; }

Schedule *model_at(int idx)
{
    if (idx < 0 || idx >= g_store.count) return NULL;
    return &g_store.items[idx];
}

Schedule *model_by_id(int id)
{
    int i;
    for (i = 0; i < g_store.count; i++)
        if (g_store.items[i].id == id) return &g_store.items[i];
    return NULL;
}

int model_index_by_id(int id)
{
    int i;
    for (i = 0; i < g_store.count; i++)
        if (g_store.items[i].id == id) return i;
    return -1;
}

static int ensure_cap(void)
{
    int nc;
    Schedule *np;
    if (g_store.count < g_store.cap) return 1;
    nc = g_store.cap ? g_store.cap * 2 : 16;
    np = (Schedule *)realloc(g_store.items, (size_t)nc * sizeof(Schedule));
    if (!np) return 0;
    g_store.items = np;
    g_store.cap = nc;
    return 1;
}

void model_normalize(Schedule *s)
{
    if (!s) return;
    s->title[MAX_TITLE - 1] = 0;
    s->note[MAX_NOTE - 1] = 0;
    s->noteRtf[MAX_RTF - 1] = 0;
    s->link[MAX_LINK - 1] = 0;
    /* 有富文本时，摘要一律从富文本重新提取，保证两者一致 */
    if (s->noteRtf[0] && wcsncmp(s->noteRtf, L"{\\rtf", 5) == 0)
        rtf_to_plain(s->noteRtf, s->note, MAX_NOTE);
    if (s->repKind < REP_NONE || s->repKind > REP_CUSTOM) s->repKind = REP_NONE;
    if (s->repInterval < 1) s->repInterval = 1;
    if (s->repInterval > 999) s->repInterval = 999;
    if (s->repUnit < UNIT_DAY || s->repUnit > UNIT_YEAR) s->repUnit = UNIT_DAY;
    s->weekMask &= 0x7F;
    if (s->repKind == REP_WEEKLY && s->weekMask == 0) {
        int wd = weekday_of_ord(civil_days(s->start.wYear, s->start.wMonth, s->start.wDay));
        s->weekMask = 1 << wd;
    }
    if (s->attachCount < 0) s->attachCount = 0;
    if (s->attachCount > MAX_ATTACH) s->attachCount = MAX_ATTACH;
    s->enabled = s->enabled ? 1 : 0;
    s->done = s->done ? 1 : 0;
    if (s->kind != KIND_ANNIV && s->kind != KIND_TODO) s->kind = KIND_TASK;
    if (s->kind == KIND_ANNIV) s->done = 0;   /* 纪念日始终是“仍在记录”的状态 */
    /* 无时间待办：不删用户填的时间，只是运行时不参与排程（取消待办后能原样回来） */
    s->showImg = s->showImg ? 1 : 0;
    s->timetable = (s->timetable && s->kind == KIND_TASK) ? 1 : 0;
    s->ttNext = s->ttNext ? 1 : 0;
    s->ttNext = s->ttNext ? 1 : 0;   /* 只有普通日程能当课表 */
    if (s->cdUnit < CDU_DEF || s->cdUnit > CDU_SEC) s->cdUnit = CDU_DEF;
    s->remind &= (REM_POPUP | REM_TOAST | REM_SOUND);
    if (s->remLead < 0) s->remLead = 0;
    if (s->remLead > 1440) s->remLead = 1440;
    if (s->remRepeat < 0) s->remRepeat = 0;
    if (s->remRepeat > 1440) s->remRepeat = 0;
    if (s->remTimes < 1) s->remTimes = 1;
    if (s->remTimes > 99) s->remTimes = 99;
    /* 提示音来源：默认系统提示音；自选文件为空时退回系统提示音（老数据没这两行，行为不变） */
    s->sndFile[MAX_PATH - 1] = 0;
    s->sndMode = (s->sndMode == SND_FILE) ? SND_FILE : SND_SYS;
    if (s->sndMode == SND_FILE && !s->sndFile[0]) s->sndMode = SND_SYS;
    if (s->imgN < 0) s->imgN = 0;
    if (s->imgN > MAX_IMGS) s->imgN = MAX_IMGS;
    if (s->imgMode != IMGM_INLINE && s->imgMode != IMGM_END) s->imgMode = IMGM_FLOAT;
    s->imgLayer = (s->imgLayer == IMGL_UNDER) ? IMGL_UNDER : IMGL_OVER;
    {   /* 旧版共用框（imgX/Y/W/H）并入第一张 */
        int k;
        if (s->imgW > 0 && s->imgH > 0 && s->imgB[0][2] <= 0) {
            s->imgB[0][0] = s->imgX;
            s->imgB[0][1] = s->imgY;
            s->imgB[0][2] = s->imgW;
            s->imgB[0][3] = s->imgH;
        }
        for (k = 0; k < MAX_IMGS; k++) {
            if (s->imgB[k][2] < 60 || s->imgB[k][2] > 1000 ||
                s->imgB[k][3] < 60 || s->imgB[k][3] > 1000) {
                s->imgB[k][0] = s->imgB[k][1] = s->imgB[k][2] = s->imgB[k][3] = 0;
            }
        }
    }
    if (s->imgPct < 25) s->imgPct = s->imgPct ? 25 : 0;
    if (s->imgPct > 100) s->imgPct = 100;
    /* 自定义图片框：千分比，超界就当自动 */
    if (s->imgX < 0) s->imgX = 0;
    if (s->imgY < 0) s->imgY = 0;
    if (s->imgX > 1000) s->imgX = 1000;
    if (s->imgY > 1000) s->imgY = 1000;
    if (s->imgW < 60 || s->imgW > 1000) s->imgW = 0;
    if (s->imgH < 60 || s->imgH > 1000) s->imgH = 0;
    if (!s->imgW || !s->imgH) { s->imgW = 0; s->imgH = 0; s->imgX = 0; s->imgY = 0; }
    s->hasEnd = s->hasEnd ? 1 : 0;
    if (!s->color) s->color = RGB(47, 111, 235);

    if (s->hasEnd) {
        time_t a = systime_to_t(&s->start), b = systime_to_t(&s->end);
        s->durSec = (b > a) ? (b - a) : 0;
        if (b <= a) s->hasEnd = 0;
    } else {
        s->durSec = 0;
    }
}

/* 第一张图片附件（不是图片就跳过），没有则返回 NULL */
const wchar_t *sched_first_image(const Schedule *s, wchar_t *buf, size_t cch)
{
    int i;
    if (!s || !buf || cch == 0) return NULL;
    for (i = 0; i < s->attachCount && i < MAX_ATTACH; i++) {
        if (s->attach[i][0] && img_is_image(s->attach[i])) {
            wcsncpy(buf, s->attach[i], cch - 1);
            buf[cch - 1] = 0;
            return buf;
        }
    }
    return NULL;
}

int sched_image_count(const Schedule *s)
{
    int i, n = 0;
    if (!s) return 0;
    for (i = 0; i < s->attachCount && i < MAX_ATTACH; i++)
        if (s->attach[i][0] && img_is_image(s->attach[i])) n++;
    return n;
}

const wchar_t *sched_image_at(const Schedule *s, int k, wchar_t *buf, size_t cch)
{
    int i, n = 0;
    if (!s || !buf || cch == 0 || k < 0) return NULL;
    for (i = 0; i < s->attachCount && i < MAX_ATTACH; i++) {
        if (s->attach[i][0] && img_is_image(s->attach[i])) {
            if (n == k) {
                wcsncpy(buf, s->attach[i], cch - 1);
                buf[cch - 1] = 0;
                return buf;
            }
            n++;
        }
    }
    return NULL;
}

Schedule *model_add(const Schedule *src)
{
    Schedule *dst;
    if (!ensure_cap()) return NULL;
    dst = &g_store.items[g_store.count];
    memset(dst, 0, sizeof(*dst));
    if (src) *dst = *src;
    if (dst->id <= 0) dst->id = g_nextId;
    if (dst->id >= g_nextId) g_nextId = dst->id + 1;
    g_store.count++;
    model_normalize(dst);
    dst->nextOcc = sched_next_occurrence(dst, time(NULL));
    return dst;
}

Schedule *model_dup(int id)
{
    Schedule *src = model_by_id(id);
    Schedule tmp;
    if (!src) return NULL;
    tmp = *src;
    tmp.id = 0;
    tmp.title[0] = 0;
    {
        wchar_t t[MAX_TITLE];
        WBuf b;
        wb_init(&b, t, MAX_TITLE);
        wb_add(&b, src->title);
        wb_add(&b, L"（副本）");
        memcpy(tmp.title, t, sizeof(t));
    }
    return model_add(&tmp);
}

void model_remove(int id)
{
    int i = model_index_by_id(id);
    if (i < 0) return;
    if (i + 1 < g_store.count) {
        memmove(&g_store.items[i], &g_store.items[i + 1],
                (size_t)(g_store.count - i - 1) * sizeof(Schedule));
    }
    g_store.count--;
}

void model_refresh_next(void)
{
    time_t now = time(NULL);
    int i;
    for (i = 0; i < g_store.count; i++) {
        Schedule *s = &g_store.items[i];
        s->nextOcc = sched_next_occurrence(s, now);
        if (s->hasEnd) {
            time_t a = systime_to_t(&s->start), b = systime_to_t(&s->end);
            s->durSec = (b > a) ? (b - a) : 0;
        }
    }
}

/* =======================================================================
 *  六、持久化（UTF-16LE 文本，带 BOM，纯手工解析，不依赖 CRT 的宽字符 IO）
 * =====================================================================*/
const wchar_t *model_path(void) { return g_path; }

void model_set_path(const wchar_t *path)
{
    size_t n;
    if (!path) { g_path[0] = 0; return; }
    n = wcslen(path);
    if (n >= MAX_PATH) n = MAX_PATH - 1;
    memcpy(g_path, path, n * sizeof(wchar_t));
    g_path[n] = 0;
}

static void esc_append(WStr *out, const wchar_t *s)
{
    if (!s) return;
    for (; *s; s++) {
        if (*s == L'\\')      ws_add(out, L"\\\\");
        else if (*s == L'\n') ws_add(out, L"\\n");
        else if (*s == L'\r') ws_add(out, L"\\r");
        else                  ws_ch(out, *s);
    }
}

static void unesc_into(wchar_t *dst, size_t cch, const wchar_t *src)
{
    size_t i = 0;
    if (!cch) return;
    while (*src && i + 1 < cch) {
        if (src[0] == L'\\' && src[1]) {
            src++;
            if (*src == L'n')      dst[i++] = L'\n';
            else if (*src == L'r') dst[i++] = L'\r';
            else                   dst[i++] = *src;
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = 0;
}

static int write_all(const wchar_t *path, const wchar_t *data, size_t wchars)
{
    HANDLE h;
    DWORD wrote = 0;
    unsigned short bom = 0xFEFF;
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    WriteFile(h, &bom, 2, &wrote, NULL);
    WriteFile(h, data, (DWORD)(wchars * sizeof(wchar_t)), &wrote, NULL);
    CloseHandle(h);
    return 1;
}

static wchar_t *read_all(const wchar_t *path, size_t *outLen)
{
    HANDLE h;
    DWORD size, rd = 0;
    wchar_t *buf;
    size_t n;
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size < 2 || size > 64u * 1024u * 1024u) {
        CloseHandle(h);
        return NULL;
    }
    buf = (wchar_t *)malloc(size + sizeof(wchar_t) * 2);
    if (!buf) { CloseHandle(h); return NULL; }
    ReadFile(h, buf, size, &rd, NULL);
    CloseHandle(h);
    n = rd / 2;
    if (n && buf[0] == 0xFEFF) {          /* 跳过 BOM */
        memmove(buf, buf + 1, (n - 1) * sizeof(wchar_t));
        n--;
    }
    buf[n] = 0;
    *outLen = n;
    return buf;
}

/* ==========================================================================
 *  历史版本：每次写盘前把旧文件备一份到 zsched_history\
 *  （至少间隔 2 分钟才存一份，最多保留 60 份）
 * ========================================================================*/
#define MODEL_HIST_MAX   60
#define MODEL_HIST_GAP   120

static wchar_t g_histDir[MAX_PATH];
static wchar_t g_histSrc[MAX_PATH];   /* g_histDir 是按哪个数据文件算出来的 */
static DWORD   g_histLastTick;

static void hist_init(void)
{
    const wchar_t *p = model_path();
    const wchar_t *slash = NULL, *q;
    size_t n;
    if (!p || !p[0]) return;
    if (g_histDir[0] && wcscmp(g_histSrc, p) == 0) return;   /* 路径没变就沿用 */
    for (q = p; *q; q++)
        if (*q == L'\\' || *q == L'/') slash = q;
    n = slash ? (size_t)(slash - p + 1) : 0;
    if (n > MAX_PATH - 40) n = MAX_PATH - 40;
    memcpy(g_histDir, p, n * sizeof(wchar_t));
    g_histDir[n] = 0;
    wcscat(g_histDir, DATA_HIST_NAME);
    wcsncpy(g_histSrc, p, MAX_PATH - 1);
    g_histSrc[MAX_PATH - 1] = 0;
}

const wchar_t *model_history_dir(void)
{
    hist_init();
    return g_histDir;
}

static void hist_trim(void)
{
    /* 文件名是时间戳，排序即年龄：超过上限就删最旧的 */
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH];
    wchar_t list[MODEL_HIST_MAX + 8][64];
    int n = 0, i, j;
    HANDLE h;
    if (!g_histDir[0]) return;
    wcscpy(pat, g_histDir);
    wcscat(pat, L"\\*.txt");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < MODEL_HIST_MAX + 8) {
            wcsncpy(list[n], fd.cFileName, 63);
            list[n][63] = 0;
            n++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (n <= MODEL_HIST_MAX) return;
    for (i = 1; i < n; i++) {           /* 按名字冒泡排序 */
        wchar_t key[64];
        wcscpy(key, list[i]);
        j = i - 1;
        while (j >= 0 && wcscmp(list[j], key) > 0) {
            wcscpy(list[j + 1], list[j]);
            j--;
        }
        wcscpy(list[j + 1], key);
    }
    for (i = 0; i < n - MODEL_HIST_MAX; i++) {
        wchar_t full[MAX_PATH];
        wcscpy(full, g_histDir);
        wcscat(full, L"\\");
        wcscat(full, list[i]);
        DeleteFileW(full);
    }
}

int model_archive(const wchar_t *why)
{
    wchar_t dst[MAX_PATH], name[80];
    SYSTEMTIME st;
    HANDLE hs, hd;
    DWORD rd = 0, wr = 0, size;
    BYTE *buf;
    int ok = 0;

    (void)why;
    hist_init();
    if (!g_histDir[0]) return 0;
    hs = CreateFileW(model_path(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hs == INVALID_HANDLE_VALUE) return 0;
    size = GetFileSize(hs, NULL);
    if (size == 0 || size > 64u * 1024 * 1024) { CloseHandle(hs); return 0; }
    CreateDirectoryW(g_histDir, NULL);
    GetLocalTime(&st);
    wsprintfW(name, L"\\zsched_%04d%02d%02d_%02d%02d%02d.txt",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    wcscpy(dst, g_histDir);
    wcscat(dst, name);
    buf = (BYTE *)malloc(size);
    if (!buf) { CloseHandle(hs); return 0; }
    if (ReadFile(hs, buf, size, &rd, NULL) && rd > 0) {
        hd = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
        if (hd != INVALID_HANDLE_VALUE) {
            ok = WriteFile(hd, buf, rd, &wr, NULL) ? 1 : 0;
            CloseHandle(hd);
        }
    }
    free(buf);
    CloseHandle(hs);
    if (ok) {
        g_histLastTick = GetTickCount();
        hist_trim();
    }
    return ok;
}

int model_history_count(void)
{
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH];
    HANDLE h;
    int n = 0;
    hist_init();
    if (!g_histDir[0]) return 0;
    wcscpy(pat, g_histDir);
    wcscat(pat, L"\\*.txt");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

int model_history_latest(wchar_t *out, size_t cch)
{
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH], best[64];
    HANDLE h;
    int found = 0;
    hist_init();
    if (!g_histDir[0] || !out || cch == 0) return 0;
    wcscpy(pat, g_histDir);
    wcscat(pat, L"\\*.txt");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!found || wcscmp(fd.cFileName, best) > 0) {
            wcsncpy(best, fd.cFileName, 63);
            best[63] = 0;
            found = 1;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!found) return 0;
    wcsncpy(out, g_histDir, cch - 1);
    out[cch - 1] = 0;
    if (wcslen(out) + 1 + wcslen(best) >= cch) return 0;
    wcscat(out, L"\\");
    wcscat(out, best);
    return 1;
}

int model_history_restore_latest(void)
{
    wchar_t src[MAX_PATH];
    HANDLE hs, hd;
    DWORD rd = 0, wr = 0, size;
    BYTE *buf;
    int ok = 0;
    if (!model_history_latest(src, MAX_PATH)) return 0;
    model_archive(L"before-restore");            /* 先把当前版本也存一份 */
    hs = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (hs == INVALID_HANDLE_VALUE) return 0;
    size = GetFileSize(hs, NULL);
    if (size == 0 || size > 64u * 1024 * 1024) { CloseHandle(hs); return 0; }
    buf = (BYTE *)malloc(size);
    if (!buf) { CloseHandle(hs); return 0; }
    if (ReadFile(hs, buf, size, &rd, NULL) && rd > 0) {
        hd = CreateFileW(model_path(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
        if (hd != INVALID_HANDLE_VALUE) {
            ok = WriteFile(hd, buf, rd, &wr, NULL) ? 1 : 0;
            CloseHandle(hd);
        }
    }
    free(buf);
    CloseHandle(hs);
    return ok;
}

int model_save(const wchar_t *path)
{
    {   /* 写盘前把旧版本存一份（至少间隔 2 分钟） */
        DWORD now = GetTickCount();
        if (g_histLastTick == 0 || now - g_histLastTick > MODEL_HIST_GAP * 1000)
            model_archive(L"auto");
    }
    WStr out;
    int i, k;
    if (!path || !path[0]) return 0;
    ws_init(&out);
    ws_add(&out, L"ZScheduleData/4\r\n");
    for (i = 0; i < g_store.count; i++) {
        const Schedule *s = &g_store.items[i];
        ws_add(&out, L"[item]\r\n");
        ws_add(&out, L"id=");      ws_int(&out, s->id);               ws_add(&out, L"\r\n");
        ws_add(&out, L"title=");   esc_append(&out, s->title);        ws_add(&out, L"\r\n");
        ws_add(&out, L"note=");    esc_append(&out, s->note);         ws_add(&out, L"\r\n");
        ws_add(&out, L"rtf=");     esc_append(&out, s->noteRtf);      ws_add(&out, L"\r\n");
        ws_add(&out, L"link=");    esc_append(&out, s->link);         ws_add(&out, L"\r\n");
        {
            wchar_t t[40];
            ws_add(&out, L"start=");
            ws_add(&out, dt_text(&s->start, t, 40));
            ws_add(&out, L"\r\n");
            ws_add(&out, L"end=");
            ws_add(&out, dt_text(&s->end, t, 40));
            ws_add(&out, L"\r\n");
        }
        ws_add(&out, L"hasend=");  ws_int(&out, s->hasEnd);           ws_add(&out, L"\r\n");
        ws_add(&out, L"rep=");     ws_int(&out, s->repKind);          ws_add(&out, L"\r\n");
        ws_add(&out, L"interval=");ws_int(&out, s->repInterval);      ws_add(&out, L"\r\n");
        ws_add(&out, L"unit=");    ws_int(&out, s->repUnit);          ws_add(&out, L"\r\n");
        ws_add(&out, L"week=");    ws_int(&out, s->weekMask);         ws_add(&out, L"\r\n");
        ws_add(&out, L"enabled="); ws_int(&out, s->enabled);          ws_add(&out, L"\r\n");
        ws_add(&out, L"done=");    ws_int(&out, s->done);             ws_add(&out, L"\r\n");
        ws_add(&out, L"kind=");    ws_int(&out, s->kind);             ws_add(&out, L"\r\n");
        ws_add(&out, L"showimg="); ws_int(&out, s->showImg);          ws_add(&out, L"\r\n");
        ws_add(&out, L"imgpct=");  ws_int(&out, s->imgPct);           ws_add(&out, L"\r\n");
        ws_add(&out, L"timetable=");ws_int(&out, s->timetable);       ws_add(&out, L"\r\n");
        ws_add(&out, L"ttnext=");  ws_int(&out, s->ttNext);           ws_add(&out, L"\r\n");
        ws_add(&out, L"cdunit=");  ws_int(&out, s->cdUnit);           ws_add(&out, L"\r\n");
        ws_add(&out, L"remind=");  ws_int(&out, s->remind);           ws_add(&out, L"\r\n");
        ws_add(&out, L"remlead="); ws_int(&out, s->remLead);          ws_add(&out, L"\r\n");
        ws_add(&out, L"remrep=");  ws_int(&out, s->remRepeat);        ws_add(&out, L"\r\n");
        ws_add(&out, L"remtimes=");ws_int(&out, s->remTimes);         ws_add(&out, L"\r\n");
        ws_add(&out, L"sndmode=");  ws_int(&out, s->sndMode);          ws_add(&out, L"\r\n");
        if (s->sndFile[0]) {
            ws_add(&out, L"sndfile="); esc_append(&out, s->sndFile);   ws_add(&out, L"\r\n");
        }
        ws_add(&out, L"imgn=");    ws_int(&out, s->imgN);             ws_add(&out, L"\r\n");
        ws_add(&out, L"imgmode=");  ws_int(&out, s->imgMode);          ws_add(&out, L"\r\n");
        ws_add(&out, L"imglayer="); ws_int(&out, s->imgLayer);         ws_add(&out, L"\r\n");
        {   /* 每张图片各自的框 */
            int kk;
            for (kk = 0; kk < MAX_IMGS; kk++) {
                wchar_t kb[16];
                WBuf bb;
                if (s->imgB[kk][2] <= 0 || s->imgB[kk][3] <= 0) continue;
                wb_init(&bb, kb, 16);
                wb_add(&bb, L"imgb");
                wb_int(&bb, kk);
                wb_add(&bb, L"=");
                ws_add(&out, kb);
                ws_int(&out, s->imgB[kk][0]); ws_add(&out, L",");
                ws_int(&out, s->imgB[kk][1]); ws_add(&out, L",");
                ws_int(&out, s->imgB[kk][2]); ws_add(&out, L",");
                ws_int(&out, s->imgB[kk][3]); ws_add(&out, L"\r\n");
            }
        }
        ws_add(&out, L"imgx=");    ws_int(&out, s->imgX);             ws_add(&out, L"\r\n");
        ws_add(&out, L"imgy=");    ws_int(&out, s->imgY);             ws_add(&out, L"\r\n");
        ws_add(&out, L"imgw=");    ws_int(&out, s->imgW);             ws_add(&out, L"\r\n");
        ws_add(&out, L"imgh=");    ws_int(&out, s->imgH);             ws_add(&out, L"\r\n");
        ws_add(&out, L"color=");
        {
            static const wchar_t *hexd = L"0123456789ABCDEF";
            wchar_t t[8];
            int n = 0, ci;
            BYTE chv[3];
            chv[0] = GetRValue(s->color);
            chv[1] = GetGValue(s->color);
            chv[2] = GetBValue(s->color);
            for (ci = 0; ci < 3; ci++) {
                t[n++] = hexd[(chv[ci] >> 4) & 0x0F];
                t[n++] = hexd[chv[ci] & 0x0F];
            }
            t[n] = 0;
            ws_add(&out, t);
        }
        ws_add(&out, L"\r\n");
        for (k = 0; k < s->attachCount; k++) {
            ws_add(&out, L"attach=");
            esc_append(&out, s->attach[k]);
            ws_add(&out, L"\r\n");
        }
        ws_add(&out, L"[/item]\r\n");
    }
    if (!out.p) { ws_free(&out); return 0; }
    {
        int ok = write_all(path, out.p, out.len);
        ws_free(&out);
        return ok;
    }
}

static int parse_hex2(const wchar_t *p)
{
    int v = 0, i;
    for (i = 0; i < 2 && p[i]; i++) {
        wchar_t c = p[i];
        v *= 16;
        if (c >= L'0' && c <= L'9') v += c - L'0';
        else if (c >= L'a' && c <= L'f') v += c - L'a' + 10;
        else if (c >= L'A' && c <= L'F') v += c - L'A' + 10;
    }
    return v;
}

int model_loaded_legacy(void) { return g_legacy; }

int model_load(const wchar_t *path)
{
    wchar_t *buf;
    size_t len = 0, pos = 0;
    Schedule cur;
    int inItem = 0, got = 0, ver = 1;

    if (!path) return 0;
    buf = read_all(path, &len);
    if (!buf) return 0;

    /* 清空现有数据 */
    g_store.count = 0;
    g_nextId = 1;
    g_legacy = 0;

    memset(&cur, 0, sizeof(cur));

    while (pos <= len) {
        wchar_t *line = buf + pos;
        wchar_t *nl = wcschr(line, L'\n');
        size_t llen;
        if (nl) { *nl = 0; pos = (size_t)(nl - buf) + 1; }
        else    { pos = len + 1; }
        llen = wcslen(line);
        if (llen && line[llen - 1] == L'\r') line[llen - 1] = 0;
        /* 去首尾空白 */
        while (*line == L' ' || *line == L'\t') line++;
        if (!*line) continue;

        if (!wcsncmp(line, L"ZScheduleData/", 14)) {
            int v = (int)wcstol(line + 14, NULL, 10);
            ver = (v >= 1 && v <= 99) ? v : 1;
            continue;
        }

        if (!wcscmp(line, L"[item]")) {
            memset(&cur, 0, sizeof(cur));
            cur.enabled = 1;
            cur.repInterval = 1;
            cur.ttNext = 1;                    /* 旧数据没有这个开关时按“开”算 */
            cur.color = RGB(47, 111, 235);
            GetLocalTime(&cur.start);
            cur.start.wHour = 9; cur.start.wMinute = 0; cur.start.wSecond = 0;
            cur.end = cur.start;
            inItem = 1;
            continue;
        }
        if (!wcscmp(line, L"[/item]")) {
            if (inItem) {
                Schedule *added = model_add(&cur);
                if (added && ver < 2) {
                    /* v1 的颜色字段用了有损写法，已被污染，按调色板重新分配 */
                    added->color = g_palette[got % 8];
                }
                got++;
            }
            inItem = 0;
            continue;
        }
        if (!inItem) continue;
        {
            wchar_t *eq = wcschr(line, L'=');
            wchar_t *k, *v;
            if (!eq) continue;
            *eq = 0;
            k = line;
            v = eq + 1;
            if (!wcscmp(k, L"id"))            cur.id = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"title"))    unesc_into(cur.title, MAX_TITLE, v);
            else if (!wcscmp(k, L"note"))     unesc_into(cur.note, MAX_NOTE, v);
            else if (!wcscmp(k, L"rtf"))      unesc_into(cur.noteRtf, MAX_RTF, v);
            else if (!wcscmp(k, L"link"))     unesc_into(cur.link, MAX_LINK, v);
            else if (!wcscmp(k, L"start")) {
                int y = 0, mo = 1, d = 1, hh = 0, mi = 0, ss = 0;
                if (swscanf(v, L"%d-%d-%d %d:%d:%d", &y, &mo, &d, &hh, &mi, &ss) == 6) {
                    cur.start.wYear = (WORD)y;   cur.start.wMonth = (WORD)mo;
                    cur.start.wDay = (WORD)d;    cur.start.wHour = (WORD)hh;
                    cur.start.wMinute = (WORD)mi;cur.start.wSecond = (WORD)ss;
                    cur.start.wMilliseconds = 0;
                }
            }
            else if (!wcscmp(k, L"end")) {
                int y = 0, mo = 1, d = 1, hh = 0, mi = 0, ss = 0;
                if (swscanf(v, L"%d-%d-%d %d:%d:%d", &y, &mo, &d, &hh, &mi, &ss) == 6) {
                    cur.end.wYear = (WORD)y;   cur.end.wMonth = (WORD)mo;
                    cur.end.wDay = (WORD)d;    cur.end.wHour = (WORD)hh;
                    cur.end.wMinute = (WORD)mi;cur.end.wSecond = (WORD)ss;
                    cur.end.wMilliseconds = 0;
                }
            }
            else if (!wcscmp(k, L"hasend"))   cur.hasEnd = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"rep"))      cur.repKind = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"interval")) cur.repInterval = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"unit"))     cur.repUnit = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"week"))     cur.weekMask = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"enabled"))  cur.enabled = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"done"))     cur.done = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"kind"))     cur.kind = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"showimg"))  cur.showImg = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgpct"))   cur.imgPct = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"timetable")) cur.timetable = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"ttnext"))   cur.ttNext = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"cdunit"))   cur.cdUnit = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"remind"))   cur.remind = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"remlead"))  cur.remLead = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"remrep"))   cur.remRepeat = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"remtimes")) cur.remTimes = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"sndmode"))  cur.sndMode = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"sndfile"))  unesc_into(cur.sndFile, MAX_PATH, v);
            else if (!wcscmp(k, L"imgn"))     cur.imgN = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgmode"))  cur.imgMode = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imglayer")) cur.imgLayer = (int)wcstol(v, NULL, 10);
            else if (!wcsncmp(k, L"imgb", 4) && k[4] >= L'0' && k[4] <= L'9') {
                int bi = k[4] - L'0';
                if (bi >= 0 && bi < MAX_IMGS) {
                    const wchar_t *q = v;
                    int kk;
                    for (kk = 0; kk < 4 && *q; kk++) {
                        cur.imgB[bi][kk] = (int)wcstol(q, NULL, 10);
                        while (*q && *q != L',') q++;
                        if (*q == L',') q++;
                    }
                }
            }
            else if (!wcscmp(k, L"imgx"))     cur.imgX = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgy"))     cur.imgY = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgw"))     cur.imgW = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgh"))     cur.imgH = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"color")) {
                if (wcslen(v) >= 6)
                    cur.color = RGB(parse_hex2(v), parse_hex2(v + 2), parse_hex2(v + 4));
            }
            else if (!wcscmp(k, L"attach")) {
                if (cur.attachCount < MAX_ATTACH)
                    unesc_into(cur.attach[cur.attachCount++], MAX_PATH, v);
            }
        }
    }
    free(buf);
    model_refresh_next();
    g_legacy = (ver < DATA_VERSION) ? 1 : 0;
    return got;
}

/* 生成演示用富文本：第一段常规，第二段加粗放大（非 ASCII 用 \uNNNN? 转义） */
static void rtf_esc_append(WStr *out, const wchar_t *s)
{
    for (; *s; s++) {
        wchar_t c = *s;
        if (c == L'\\' || c == L'{' || c == L'}') { ws_ch(out, L'\\'); ws_ch(out, c); }
        else if (c == L'\n') ws_add(out, L"\\par ");
        else if (c < 128)    ws_ch(out, c);
        else { ws_add(out, L"\\u"); ws_int(out, (short)c); ws_ch(out, L'?'); }
    }
}

static void make_demo_rtf(WStr *out, const wchar_t *p1, const wchar_t *p2,
                          int pt1, int pt2)
{
    ws_add(out, L"{\\rtf1\\ansi\\ansicpg936\\deff0{\\fonttbl{\\f0\\fnil\\fcharset134 Microsoft YaHei UI;}}\n");
    ws_add(out, L"\\viewkind4\\uc1\\pard\\f0\\fs");
    ws_int(out, pt1 * 2);
    ws_ch(out, L' ');
    rtf_esc_append(out, p1);
    ws_add(out, L"\\par\\b\\fs");
    ws_int(out, pt2 * 2);
    ws_ch(out, L' ');
    rtf_esc_append(out, p2);
    ws_add(out, L"\\b0\\fs");
    ws_int(out, pt1 * 2);
    ws_add(out, L"\\par}");
}

/* 首次运行的演示数据 */
void model_seed_sample(void)
{
    Schedule s;
    SYSTEMTIME st;
    GetLocalTime(&st);
    st.wMinute = 0; st.wSecond = 0; st.wMilliseconds = 0;

    /* 1. 一次性：3 天后的预约 */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(47, 111, 235);
    s.repKind = REP_NONE;
    s.repInterval = 1;
    GetLocalTime(&s.start);
    {
        time_t t = time(NULL) + 3 * 86400;
        struct tm *pt = localtime(&t);
        if (pt) {
            s.start.wYear = (WORD)(pt->tm_year + 1900);
            s.start.wMonth = (WORD)(pt->tm_mon + 1);
            s.start.wDay = (WORD)pt->tm_mday;
        }
    }
    s.start.wHour = 14; s.start.wMinute = 30; s.start.wSecond = 0;
    s.start.wMilliseconds = 0;
    s.end = s.start; s.end.wHour = 15; s.end.wMinute = 30;
    s.hasEnd = 1;
    wcscpy(s.title, L"体检预约 · 市第一医院");
    wcscpy(s.note, L"带上身份证和医保卡，提前 15 分钟到前台取号。\n"
                   L"检查前一天晚上 22:00 后禁食。");
    {
        WStr r;
        ws_init(&r);
        make_demo_rtf(&r,
                      L"带上身份证和医保卡，提前 15 分钟到前台取号。",
                      L"检查前一天晚上 22:00 后禁食。", 10, 14);
        if (r.p) {
            wcsncpy(s.noteRtf, r.p, MAX_RTF - 1);
            s.noteRtf[MAX_RTF - 1] = 0;
        }
        ws_free(&r);
    }
    wcscpy(s.link, L"https://www.example.com/appointment");
    model_add(&s);

    /* 2. 每周多选：周一 / 周三 / 周五 */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(16, 152, 96);
    s.repKind = REP_WEEKLY;
    s.repInterval = 1;
    s.weekMask = (1 << 1) | (1 << 3) | (1 << 5);
    s.start = st;
    s.start.wHour = 9; s.start.wMinute = 30;
    s.end = s.start; s.end.wHour = 10; s.end.wMinute = 0;
    s.hasEnd = 1;
    wcscpy(s.title, L"晨会 · 项目进度同步");
    wcscpy(s.note, L"每人 2 分钟，同步昨天进展、今天计划、遇到的风险。");
    model_add(&s);

    /* 3. 自定义周期：每 3 天 */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(232, 125, 30);
    s.repKind = REP_CUSTOM;
    s.repInterval = 3;
    s.repUnit = UNIT_DAY;
    s.start = st;
    s.start.wHour = 20; s.start.wMinute = 0;
    wcscpy(s.title, L"健身房 · 力量训练");
    wcscpy(s.note, L"每 3 天一次，避免连续两天训练同一肌群。");
    model_add(&s);

    /* 4. 每月：交房租 */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(214, 45, 79);
    s.repKind = REP_CUSTOM;
    s.repInterval = 1;
    s.repUnit = UNIT_MONTH;
    s.start = st;
    s.start.wDay = 1;
    s.start.wHour = 9; s.start.wMinute = 0;
    wcscpy(s.title, L"交房租");
    wcscpy(s.note, L"每月 1 号，转账后截图存到云盘。");
    model_add(&s);

    /* 5. 纪念日：记录已经过去的事（每年还会过一次周年） */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(150, 62, 54);
    s.kind = KIND_ANNIV;
    s.repKind = REP_CUSTOM;
    s.repInterval = 1;
    s.repUnit = UNIT_YEAR;
    GetLocalTime(&s.start);
    s.start.wYear = (WORD)(s.start.wYear - 3);
    s.start.wMonth = 5;
    s.start.wDay = 20;
    s.start.wHour = 0; s.start.wMinute = 0; s.start.wSecond = 0;
    wcscpy(s.title, L"入职纪念日");
    wcscpy(s.note, L"那天第一次走进这间办公室，桌上只有一台旧显示器。");
    model_add(&s);

    model_refresh_next();
}

/* ======================================================================
 *  \u6570\u636e\u4fdd\u5b58\u4f4d\u7f6e\uff1a\u7edf\u4e00\u76ee\u5f55 + \u81ea\u5b9a\u4e49\u8def\u5f84 + \u65e7\u6570\u636e\u5bfc\u5165
 *
 *  \u2460 \u4ee5\u524d\u4e09\u4efd\u72b6\u6001\u6587\u4ef6\uff08\u4e3b\u6570\u636e / \u4fbf\u7b3a\u72b6\u6001 / \u4fbf\u7b3a\u5899\u5e03\u5c40\uff09\u90fd\u8ddf\u7740 exe \u8d70\uff0cexe \u4e00\u6362\u4f4d\u7f6e\u6570\u636e\u5c31\u201c\u4e22\u201d\u4e86\uff1b
 *     \u73b0\u5728\u53ea\u8ba4\u4e00\u4e2a\u201c\u6570\u636e\u76ee\u5f55\u201d\uff08`HKCU\Software\Jian\DataDir`\uff09\uff0c\u9ed8\u8ba4\u4ecd\u662f exe \u6240\u5728\u76ee\u5f55\u3002
 *  \u2461 \u56db\u4efd\u4e1c\u897f\u540c\u8fdb\u540c\u51fa\uff1a`zsched_data.txt`\u3001`zsched_notes.txt`\u3001`zsched_board.txt`\u3001`zsched_history\\`\u3002
 *  \u2462 \u65e7\u6570\u636e\u53ef\u4ee5\u81ea\u52a8\u641c\u5230\uff08exe \u76ee\u5f55 / \u684c\u9762 / \u6587\u6863 / \u4e0b\u8f7d / %APPDATA%\Jian\uff09\u5e76\u5207\u8fc7\u53bb\uff0c
 *     \u5207\u6362\u524d\u4e00\u5b9a\u5148\u628a\u5f53\u524d\u6570\u636e\u5b58\u4e00\u4efd\u5230\u5386\u53f2\u91cc\uff0c\u4e14\u4e0d\u4f1a\u8986\u76d6\u4efb\u4f55\u5df2\u6709\u6587\u4ef6\u3002
 * =====================================================================*/
static const wchar_t *kAppSetKey = L"Software\\Jian";
static const wchar_t *kAppSetDir = L"DataDir";
static wchar_t g_dataDir[MAX_PATH];

/* exe 所在目录（带尾 \\） */
static void exe_dir_get(wchar_t *out, size_t cch)
{
    wchar_t exe[MAX_PATH];
    wchar_t *p;
    size_t n;
    out[0] = 0;
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) return;
    p = exe + wcslen(exe);
    while (p > exe && p[-1] != L'\\' && p[-1] != L'/') p--;
    *p = 0;
    n = wcslen(exe);
    wcsncpy(out, exe, cch - 1);
    out[cch - 1] = 0;
    if (n && out[n - 1] != L'\\') wcsncat(out, L"\\", cch - wcslen(out) - 1);
}

/* "\u76ee\u5f55 + \u6587\u4ef6\u540d" \u62fc\u8def\u5f84\uff08\u81ea\u52a8\u8865\u4e0a\u4e2d\u95f4\u7684\u53cd\u659c\u6760\uff09 */
static void path_join(wchar_t *out, size_t cch, const wchar_t *dir, const wchar_t *name)
{
    size_t n;
    wcsncpy(out, dir ? dir : L"", cch - 1);
    out[cch - 1] = 0;
    n = wcslen(out);
    if (n && out[n - 1] != L'\\' && out[n - 1] != L'/') {
        wcsncat(out, L"\\", cch - wcslen(out) - 1);
    }
    wcsncat(out, name, cch - wcslen(out) - 1);
}

/* \u53d6\u6587\u4ef6\u6240\u5728\u76ee\u5f55\uff08\u5e26\u5c3e \\\uff09 */
static void dir_of_path(const wchar_t *file, wchar_t *out, size_t cch)
{
    const wchar_t *slash = NULL, *q;
    size_t n;
    out[0] = 0;
    if (!file || !file[0]) return;
    for (q = file; *q; q++)
        if (*q == L'\\' || *q == L'/') slash = q;
    n = slash ? (size_t)(slash - file + 1) : 0;
    if (n >= cch) n = cch - 1;
    memcpy(out, file, n * sizeof(wchar_t));
    out[n] = 0;
}

static int file_exists(const wchar_t *path)
{
    DWORD a;
    if (!path || !path[0]) return 0;
    a = GetFileAttributesW(path);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

static int dir_exists(const wchar_t *path)
{
    DWORD a;
    if (!path || !path[0]) return 0;
    a = GetFileAttributesW(path);
    return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

static int copy_file_to(const wchar_t *src, const wchar_t *dst)
{
    if (!file_exists(src)) return 0;
    return CopyFileW(src, dst, FALSE) ? 1 : 0;
}

/* \u6570\u4e00\u4e2a UTF-16 \u6587\u4ef6\u91cc\u51fa\u73b0\u4e86\u591a\u5c11\u6b21\u67d0\u4e2a\u6807\u8bb0\uff08\u7528\u6765\u7ed9\u5019\u9009\u6570\u636e\u201c\u6570\u6761\u6570\u201d\uff09 */
static int count_token(const wchar_t *path, const wchar_t *token)
{
    size_t len = 0;
    wchar_t *buf = read_all(path, &len);
    wchar_t *p;
    int n = 0;
    size_t tl;
    if (!buf) return 0;
    tl = wcslen(token);
    for (p = buf; (p = wcsstr(p, token)) != NULL; p += tl) n++;
    free(buf);
    return n;
}

static void setting_dir_get(wchar_t *out, size_t cch)
{
    HKEY k;
    DWORD type = 0, cb;
    out[0] = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAppSetKey, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return;
    cb = (DWORD)(cch * sizeof(wchar_t));
    if (RegQueryValueExW(k, kAppSetDir, NULL, &type, (BYTE *)out, &cb) != ERROR_SUCCESS ||
        type != REG_SZ) {
        out[0] = 0;
    }
    out[cch - 1] = 0;
    RegCloseKey(k);
}

static void setting_dir_set(const wchar_t *dir)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kAppSetKey, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &k, NULL) != ERROR_SUCCESS) return;
    if (dir && dir[0]) {
        RegSetValueExW(k, kAppSetDir, 0, REG_SZ, (const BYTE *)dir,
                       (DWORD)((wcslen(dir) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, kAppSetDir);
    }
    RegCloseKey(k);
}

void app_data_init(void)
{
    wchar_t dir[MAX_PATH];
    setting_dir_get(dir, MAX_PATH);
    if (dir[0] && !dir_exists(dir)) dir[0] = 0;      /* \u8bbe\u7f6e\u7684\u76ee\u5f55\u4e0d\u5728\uff08\u6bd4\u5982 U \u76d8\u62d4\u4e86\uff09\u5c31\u56de\u5230 exe \u65c1 */
    if (!dir[0]) exe_dir_get(dir, MAX_PATH);
    {
        size_t n = wcslen(dir);
        if (n && dir[n - 1] != L'\\' && dir[n - 1] != L'/') wcsncat(dir, L"\\", MAX_PATH - n - 1);
    }
    wcsncpy(g_dataDir, dir, MAX_PATH - 1);
    g_dataDir[MAX_PATH - 1] = 0;
}

const wchar_t *app_data_dir(void)
{
    if (!g_dataDir[0]) app_data_init();
    return g_dataDir;
}

/* \u628a\u5f53\u524d\u56db\u4efd\u4e1c\u897f\u590d\u5236\u5230 dstDir\uff08\u4e0d\u5220\u4e0d\u79fb\uff09 */
int app_data_copy_to(const wchar_t *dstDir)
{
    wchar_t srcFile[MAX_PATH], dstFile[MAX_PATH], srcHist[MAX_PATH], dstHist[MAX_PATH];
    int ok = 0;
    if (!dstDir || !dstDir[0]) return 0;
    CreateDirectoryW(dstDir, NULL);
    srcFile[0] = 0;
    if (model_path()[0]) wcsncpy(srcFile, model_path(), MAX_PATH - 1);
    else path_join(srcFile, MAX_PATH, app_data_dir(), DATA_FILE_NAME);
    if (srcFile[0]) {
        dir_of_path(srcFile, srcHist, MAX_PATH);
        path_join(dstFile, MAX_PATH, dstDir, DATA_FILE_NAME);
        if (copy_file_to(srcFile, dstFile)) ok = 1;
    }
    {
        wchar_t s[MAX_PATH], d[MAX_PATH], histDir[MAX_PATH];
        const wchar_t *old = srcFile[0] ? srcHist : NULL;
        if (old) {
            path_join(s, MAX_PATH, old, DATA_NOTES_NAME);
            path_join(d, MAX_PATH, dstDir, DATA_NOTES_NAME);
            copy_file_to(s, d);
            path_join(s, MAX_PATH, old, DATA_BOARD_NAME);
            path_join(d, MAX_PATH, dstDir, DATA_BOARD_NAME);
            copy_file_to(s, d);
            /* 历史目录：注意源路径要用 histDir（以前错用了数据目录 srcHist，
               结果历史文件永远找不到、历史目录拷过去是空的） */
            path_join(histDir, MAX_PATH, old, DATA_HIST_NAME);
            path_join(dstHist, MAX_PATH, dstDir, DATA_HIST_NAME);
            CreateDirectoryW(dstHist, NULL);
            {   /* \u5386\u53f2\u76ee\u5f55\u91cc\u7684 *.txt \u9010\u4e2a\u590d\u5236 */
                WIN32_FIND_DATAW fd;
                wchar_t pat[MAX_PATH];
                HANDLE h;
                path_join(pat, MAX_PATH, histDir, L"*.txt");
                h = FindFirstFileW(pat, &fd);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        path_join(s, MAX_PATH, histDir, fd.cFileName);
                        path_join(d, MAX_PATH, dstHist, fd.cFileName);
                        if (!file_exists(d)) copy_file_to(s, d);
                    } while (FindNextFileW(h, &fd));
                    FindClose(h);
                }
            }
        }
    }
    return ok;
}

/* \u6362\u6570\u636e\u76ee\u5f55\uff1bmigrate=1 \u65f6\u5148\u628a\u73b0\u6709\u6587\u4ef6\u590d\u5236\u8fc7\u53bb */
int app_data_dir_set(const wchar_t *dir, int migrate)
{
    wchar_t d[MAX_PATH];
    size_t n;
    if (!dir || !dir[0]) return 0;
    wcsncpy(d, dir, MAX_PATH - 1);
    d[MAX_PATH - 1] = 0;
    n = wcslen(d);
    if (n && d[n - 1] != L'\\' && d[n - 1] != L'/') wcsncat(d, L"\\", MAX_PATH - n - 1);
    if (!dir_exists(d)) {
        if (!CreateDirectoryW(d, NULL)) return 0;
    }
    if (migrate) app_data_copy_to(d);
    setting_dir_set(d);
    wcsncpy(g_dataDir, d, MAX_PATH - 1);
    g_dataDir[MAX_PATH - 1] = 0;
    return 1;
}

void app_data_reload(void)
{
    wchar_t f[MAX_PATH];
    path_join(f, MAX_PATH, app_data_dir(), DATA_FILE_NAME);
    model_free();                     /* 先清空当前数据，别和新目录里的叠在一起 */
    model_init();
    model_set_path(f);
    if (!model_load(f)) {
        model_seed_sample();
        model_save(f);
    } else if (model_loaded_legacy()) {
        model_save(f);
    }
    model_refresh_next();
}

/* \u627e\u672c\u673a\u5df2\u6709\u7684\u6570\u636e\uff1a\u770b\u8fd9\u51e0\u4e2a\u76ee\u5f55\u91cc\u6709\u6ca1\u6709 zsched_data.txt\uff0c\u6309\u4fee\u6539\u65f6\u95f4\u65b0\u2192\u65e7\u6392 */
int app_data_find(AppDataCand *out, int max)
{
    wchar_t cands[10][MAX_PATH];
    int nc = 0, i, n = 0;
    wchar_t *env;

#define ADD_DIR(p)                                                            \
    do {                                                                      \
        if ((p)[0]) {                                                         \
            int k, dup = 0;                                                   \
            for (k = 0; k < nc; k++)                                          \
                if (_wcsicmp(cands[k], (p)) == 0) { dup = 1; break; }          \
            if (!dup && nc < 10) { wcsncpy(cands[nc], (p), MAX_PATH - 1);     \
                                   cands[nc][MAX_PATH - 1] = 0; nc++; }       \
        }                                                                     \
    } while (0)

    ADD_DIR(app_data_dir());
    {   /* exe \u65c1 */
        wchar_t d[MAX_PATH];
        exe_dir_get(d, MAX_PATH);
        ADD_DIR(d);
    }
    {   /* \u684c\u9762 / \u6587\u6863 / \u4e0b\u8f7d / \u4e00\u9a71\u684c\u9762 */
        wchar_t d[MAX_PATH];
        env = _wgetenv(L"USERPROFILE");
        if (env && env[0]) {
            path_join(d, MAX_PATH, env, L"Desktop");        ADD_DIR(d);
            path_join(d, MAX_PATH, env, L"Documents");      ADD_DIR(d);
            path_join(d, MAX_PATH, env, L"Downloads");      ADD_DIR(d);
            path_join(d, MAX_PATH, env, L"OneDrive\\Desktop");   ADD_DIR(d);
            path_join(d, MAX_PATH, env, L"OneDrive\\Documents"); ADD_DIR(d);
        }
        env = _wgetenv(L"APPDATA");
        if (env && env[0]) { path_join(d, MAX_PATH, env, L"Jian"); ADD_DIR(d); }
        env = _wgetenv(L"LOCALAPPDATA");
        if (env && env[0]) { path_join(d, MAX_PATH, env, L"Jian"); ADD_DIR(d); }
    }
#undef ADD_DIR

    for (i = 0; i < nc && n < max; i++) {
        wchar_t f[MAX_PATH], nf[MAX_PATH];
        WIN32_FIND_DATAW fd;
        HANDLE h;
        path_join(f, MAX_PATH, cands[i], DATA_FILE_NAME);
        if (!file_exists(f)) continue;
        memset(&out[n], 0, sizeof(out[n]));
        wcsncpy(out[n].dir, cands[i], MAX_PATH - 1);
        wcsncpy(out[n].dataFile, f, MAX_PATH - 1);
        out[n].items = count_token(f, L"[item]");
        path_join(nf, MAX_PATH, cands[i], DATA_NOTES_NAME);
        if (file_exists(nf)) {
            out[n].hasNotes = 1;
            out[n].notes = count_token(nf, L"id=");
        }
        h = FindFirstFileW(f, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            out[n].mtime = ((long long)fd.ftLastWriteTime.dwHighDateTime << 32) |
                           fd.ftLastWriteTime.dwLowDateTime;
            FindClose(h);
        }
        n++;
    }
    {   /* \u6309\u4fee\u6539\u65f6\u95f4\u4ece\u65b0\u5230\u65e7\u6392\u5e8f\uff08\u63d2\u5165\u6392\u5e8f\uff0c\u6700\u591a 10 \u4efd\uff09 */
        int a, b;
        for (a = 1; a < n; a++) {
            AppDataCand key = out[a];
            b = a - 1;
            while (b >= 0 && out[b].mtime < key.mtime) { out[b + 1] = out[b]; b--; }
            out[b + 1] = key;
        }
    }
    return n;
}

/* \u5148\u628a\u5f53\u524d\u6570\u636e\u5b58\u4e00\u4efd\u5230\u5386\u53f2\uff0c\u518d\u5207\u5230\u5019\u9009\u76ee\u5f55\uff08\u4e0d\u590d\u5236\u3001\u4e0d\u8986\u76d6\u4efb\u4f55\u6587\u4ef6\uff09 */
int app_data_import(const AppDataCand *c)
{
    if (!c || !c->dir[0]) return 0;
    model_archive(L"import");                 /* \u5f53\u524d\u6570\u636e\u5148\u5b58\u6863\uff0c\u4e0d\u4f1a\u4e22 */
    if (!app_data_dir_set(c->dir, 0)) return 0;
    app_data_reload();
    return 1;
}

