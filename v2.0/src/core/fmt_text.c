/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/strbuf.h"
#include "core/fmt_text.h"


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
