#ifndef ZS_CORE_FMT_TEXT_H
#define ZS_CORE_FMT_TEXT_H

#include "core/types.h"

const wchar_t *weekday_names_text(int mask, wchar_t *buf, size_t cch);
const wchar_t *file_name_of(const wchar_t *path, wchar_t *buf, size_t cch);
const wchar_t *status_text(const Schedule *s, wchar_t *buf, size_t cch);

#endif /* ZS_CORE_FMT_TEXT_H */
