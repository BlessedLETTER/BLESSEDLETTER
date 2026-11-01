/* ZSchedule v2.0 · core/style/theme.c
   主题色板——原本散落在 ui_main.c，移到 core 后 model/editor 不再反向依赖 app。 */
#include "core/types.h"
#include "core/theme.h"

const COLORREF g_palette[8] = {
    RGB(59, 111, 224),   /* 蓝 */
    RGB(22, 163, 106),   /* 绿 */
    RGB(235, 148, 40),   /* 橙 */
    RGB(219, 74, 74),    /* 红 */
    RGB(124, 92, 224),   /* 紫 */
    RGB(13, 148, 168),   /* 青 */
    RGB(214, 51, 132),   /* 粉 */
    RGB(112, 122, 136)   /* 灰 */
};

COLORREF palette_color(int idx)
{
    if (idx < 0 || idx > 7) return g_palette[0];
    return g_palette[idx];
}
