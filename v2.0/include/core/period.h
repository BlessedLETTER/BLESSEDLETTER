#ifndef ZS_CORE_PERIOD_H
#define ZS_CORE_PERIOD_H

#include "core/types.h"

/* 周期与时间计算 */
time_t      sched_next_occurrence(const Schedule *s, time_t now);
time_t      sched_occurrence_end(const Schedule *s, time_t occ);
time_t      sched_occurrence_on(const Schedule *s, long long dayOrd);
long long   sched_start_day(const Schedule *s);
int         sched_day_count(time_t from, time_t to);
long long   civil_days(int y, int m, int d);
void        civil_from_days(long long z, int *py, int *pm, int *pd);
int         days_in_month(int y, int m);
int         weekday_of_day(long long ord);     /* 0=周日 */
long long   monday_of(long long ord);          /* 所在周的周一序号 */
long long   today_ord(void);
time_t      time_from_ord(long long ord, int hh, int mi, int ss);

time_t      zs_systime_to_t(const SYSTEMTIME *st);
time_t      zs_time_from_ymd(int y, int m, int d, int hh, int mi, int ss);

#endif /* ZS_CORE_PERIOD_H */
