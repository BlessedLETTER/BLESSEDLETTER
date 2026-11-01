/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/strbuf.h"
#include "core/fmt_time.h"
#include "modules/countdown.h"


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
/* 某一天这节课的 [开始, 结束)；0 = 那天没有这节课。
   没填结束时间时按 TT_CLASS_MIN 分钟一节算。 */
