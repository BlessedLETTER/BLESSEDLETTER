#ifndef ZS_CORE_FMT_TIME_H
#define ZS_CORE_FMT_TIME_H

#include "core/types.h"

/* 文本格式化 */
const wchar_t *repeat_text(const Schedule *s, wchar_t *buf, size_t cch);
const wchar_t *sched_first_image(const Schedule *s, wchar_t *buf, size_t cch);  /* 首张图片附件，无则 NULL */
const wchar_t *dt_text(const SYSTEMTIME *st, wchar_t *buf, size_t cch);
const wchar_t *timet_text(time_t t, wchar_t *buf, size_t cch);
const wchar_t *date_text(time_t t, wchar_t *buf, size_t cch);
const wchar_t *clock_text(time_t t, wchar_t *buf, size_t cch);
const wchar_t *countdown_text(time_t occ, wchar_t *buf, size_t cch);

#endif /* ZS_CORE_FMT_TIME_H */
