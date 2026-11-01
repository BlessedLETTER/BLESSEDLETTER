#ifndef ZS_CORE_TEXTFILE_H
#define ZS_CORE_TEXTFILE_H

#include "core/types.h"

/* UTF-16LE / ESC 文本读写原子：v2.0 重写中 */

void   zs_esc_append(WStr *out, const wchar_t *s);
void   zs_unesc_into(wchar_t *dst, size_t cch, const wchar_t *src);
int    zs_write_all(const wchar_t *path, const wchar_t *text, size_t len);
wchar_t *zs_read_all(const wchar_t *path, size_t *outLen);

#endif /* ZS_CORE_TEXTFILE_H */
