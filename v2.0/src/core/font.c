/* ZSchedule v2.0 · core/font.c
   统一的 DPI 字体工厂：pt / 权重 / 缩放 / 字体名 / 斜体 / 旋转角。
   便笺与墙的字体缓存仍然保留（各自的量化键），但实际创建走这里。 */
#include "core/types.h"
#include "core/font.h"

HFONT zs_font_create(int pt, int weight, int scalePct, const wchar_t *face,
                     int italic, int escapement)
{
    HDC hdc;
    int dpi, h;
    HFONT f;
    if (pt < 1) pt = 1;
    if (scalePct <= 0) scalePct = 100;
    hdc = GetDC(NULL);
    dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(NULL, hdc);
    h = -MulDiv(pt, dpi * scalePct, 72 * 100);
    if (h == 0) h = -pt;
    f = CreateFontW(h, 0, escapement, escapement, weight, italic ? 1 : 0, 0, 0,
                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    (face && face[0]) ? face : L"Microsoft YaHei UI");
    return f;
}
