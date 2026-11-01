#ifndef ZS_CORE_FONT_H
#define ZS_CORE_FONT_H

#include "core/types.h"

/* DPI 字体工厂：v2.0 重写中 */

HFONT zs_font_create(int pt, int weight, int scalePct, const wchar_t *face,
                     int italic, int escapement);

#endif /* ZS_CORE_FONT_H */
