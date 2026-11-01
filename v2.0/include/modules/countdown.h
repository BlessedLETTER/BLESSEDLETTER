#ifndef ZS_MOD_COUNTDOWN_H
#define ZS_MOD_COUNTDOWN_H

#include "core/types.h"
#include "core/fmt_time.h"

void           countdown_split(time_t occ, long long *days,
                               int *hh, int *mi, int *ss, int *state);
int            cd_unit_of(const Schedule *s);                 /* 有效倒计时精度 */
const wchar_t *cd_text(const Schedule *s, time_t occ, wchar_t *buf, size_t cch);
const wchar_t *cd_text_u(time_t occ, int unit, wchar_t *buf, size_t cch);

#endif /* ZS_MOD_COUNTDOWN_H */
