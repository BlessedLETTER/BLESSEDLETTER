/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/strbuf.h"
#include "core/period.h"
#include "core/fmt_time.h"
#include "modules/schedule.h"
#include "modules/timetable.h"


static int class_window_on(const Schedule *s, long dayOrd, time_t *pStart, time_t *pEnd)
{
    time_t st, en;
    if (!s || !s->timetable) return 0;
    if (!s->enabled || s->done) return 0;
    if (s->kind != KIND_TASK) return 0;
    st = sched_occurrence_on(s, dayOrd);
    if (st <= 0) return 0;
    en = sched_occurrence_end(s, st);
    if (en <= st) en = st + (time_t)TT_CLASS_MIN * 60;
    if (pStart) *pStart = st;
    if (pEnd) *pEnd = en;
    return 1;
}

/* 这节课的窗口：如果昨天开始的那节还没结束（跨零点），优先返回它；否则今天这节 */
int sched_class_window(const Schedule *s, time_t *pStart, time_t *pEnd)
{
    time_t now = time(NULL);
    time_t yst, yen;
    if (class_window_on(s, today_ord() - 1, &yst, &yen) && now >= yst && now < yen) {
        if (pStart) *pStart = yst;
        if (pEnd) *pEnd = yen;
        return 1;
    }
    return class_window_on(s, today_ord(), pStart, pEnd);
}

/* 此刻正在上的那节课（含跨零点；重叠时取最近开始的）；没有则 NULL */
Schedule *sched_class_now_ex(time_t *pStart, time_t *pEnd)
{
    time_t now = time(NULL), bestStart = 0, bs = 0, be = 0;
    Schedule *best = NULL;
    int i;
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        time_t st, en;
        int hit = 0;
        if (class_window_on(s, today_ord(), &st, &en) && now >= st && now < en)
            hit = 1;
        else if (class_window_on(s, today_ord() - 1, &st, &en) && now >= st && now < en)
            hit = 1;
        if (!hit) continue;
        if (!best || st >= bestStart) { best = s; bestStart = st; bs = st; be = en; }
    }
    if (best) {
        if (pStart) *pStart = bs;
        if (pEnd) *pEnd = be;
    }
    return best;
}

Schedule *sched_class_now(void)
{
    return sched_class_now_ex(NULL, NULL);
}

/* after 之后今天最近的一节课（只用于“今天”的旧逻辑，交接不再使用） */
Schedule *sched_class_next(time_t after, time_t *pStart, time_t *pEnd)
{
    Schedule *best = NULL;
    time_t bestStart = 0, bestEnd = 0;
    int i;
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        time_t st, en;
        if (!class_window_on(s, today_ord(), &st, &en)) continue;
        if (st <= after) continue;
        if (!best || st < bestStart) { best = s; bestStart = st; bestEnd = en; }
    }
    if (best) {
        if (pStart) *pStart = bestStart;
        if (pEnd) *pEnd = bestEnd;
    }
    return best;
}

/* after 之后的下一节课，不限今天：明天第一节 / 下周一第一节都算。
   用每节课的 nextOcc（每秒刷新、支持每周/自定义周期）；一次性课程过期后自然为 0。 */
Schedule *sched_class_next_any(time_t after, time_t *pStart, time_t *pEnd)
{
    Schedule *best = NULL;
    time_t bs = 0, be = 0;
    time_t now = time(NULL);
    int i;
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        time_t st, en;
        if (!s || !s->timetable) continue;
        if (!s->enabled || s->done || s->kind != KIND_TASK) continue;
        st = (after <= now + 2) ? s->nextOcc : sched_next_occurrence(s, after);
        if (st <= 0 || st < after) continue;
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
            wb_add(&b, wd[weekday_of_day(d)]);
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

