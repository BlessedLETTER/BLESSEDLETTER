#ifndef ZS_CORE_THEME_H
#define ZS_CORE_THEME_H

#include "core/types.h"

/* ======================================================================
 *  视觉主题 —— 现代简约：浅灰底 + 白卡片 + 单一强调色 + 大留白
 * ====================================================================*/
#define TH_BG          RGB(243, 238, 227)   /* 窗口底色：暖米纸 */
#define TH_SURFACE     RGB(252, 250, 244)   /* 卡片：本色纸 */
#define TH_SURFACE_2   RGB(247, 243, 234)   /* 次级面板 */
#define TH_LINE        RGB(219, 210, 193)   /* 分隔线：纸纤维灰 */
#define TH_LINE_SOFT   RGB(232, 225, 211)
#define TH_TEXT        RGB(43, 38, 32)      /* 主文字：墨色 */
#define TH_TEXT_2      RGB(112, 102, 88)    /* 次要文字 */
#define TH_TEXT_3      RGB(152, 142, 126)   /* 弱化文字 */
#define TH_ACCENT      RGB(150, 62, 54)     /* 强调色：朱印 */
#define TH_ACCENT_SOFT RGB(244, 231, 225)
#define TH_HOVER       RGB(236, 229, 214)
#define TH_PRESS       RGB(226, 217, 199)
#define TH_DANGER      RGB(178, 58, 48)
#define TH_WARN        RGB(186, 122, 40)
#define TH_OK          RGB(74, 122, 78)
#define TH_SELECT      RGB(241, 231, 219)

#define TH_RADIUS      8                    /* 卡片圆角 */
#define TH_TOOLBAR_H   56
#define TH_PANEL_H     104
#define TH_GAP         10

COLORREF palette_color(int idx);
extern const COLORREF g_palette[8];

#endif /* ZS_CORE_THEME_H */
