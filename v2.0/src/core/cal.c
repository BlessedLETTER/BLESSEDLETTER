/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/period.h"


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

time_t zs_systime_to_t(const SYSTEMTIME *st)
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

time_t zs_time_from_ymd(int y, int m, int d, int hh, int mi, int ss)
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
    return zs_time_from_ymd(y, m, d, hh, mi, ss);
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
