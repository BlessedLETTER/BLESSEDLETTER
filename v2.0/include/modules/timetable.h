#ifndef ZS_MOD_TIMETABLE_H
#define ZS_MOD_TIMETABLE_H

#include "core/types.h"
#include "core/period.h"

/* 课表：今天这节课的 [开始, 结束)、此刻正在上的课、下一节 */
int            sched_class_window(const Schedule *s, time_t *pStart, time_t *pEnd);
Schedule      *sched_class_now(void);
Schedule      *sched_class_now_ex(time_t *pStart, time_t *pEnd);   /* 含跨零点 */
Schedule      *sched_class_next(time_t after, time_t *pStart, time_t *pEnd);
Schedule      *sched_class_next_any(time_t after, time_t *pStart, time_t *pEnd);  /* 含明天/下周 */
const wchar_t *tt_when_text(time_t t, wchar_t *buf, size_t cch);    /* 下一节课的时间文字 */

#endif /* ZS_MOD_TIMETABLE_H */
