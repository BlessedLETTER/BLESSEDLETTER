/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/period.h"


time_t sched_next_occurrence(const Schedule *s, time_t now)
{
    time_t start;
    int N;
    long long startOrd, todayOrd, diff, k, kk, dbest;
    struct tm ntm;

    if (!s || !s->enabled || s->done) return 0;
    if (s->kind == KIND_TODO) return 0;      /* 无时间待办：没有“下次发生” */
    if (s->kind == KIND_ANNIV && s->repKind != REP_CUSTOM) return 0;

    start = zs_systime_to_t(&s->start);
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
        int wd0 = weekday_of_day(startOrd);
        long long anchorMon = startOrd - ((wd0 + 6) % 7);   /* 以周一为一周起点 */
        for (kk = 0; kk <= 7 * N + 7; kk++) {
            long long d = todayOrd + kk;
            int wd;
            long long mon, weeks;
            time_t cand;
            if (s->weekMask == 0) break;
            wd = weekday_of_day(d);
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
            cand = zs_time_from_ymd(y, m, d, s->start.wHour, s->start.wMinute, s->start.wSecond);
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
            cand = zs_time_from_ymd(y, m, d, s->start.wHour, s->start.wMinute, s->start.wSecond);
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
        int wd = weekday_of_day(dayOrd);
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
