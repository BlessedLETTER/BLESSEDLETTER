#ifndef ZS_UI_IDS_H
#define ZS_UI_IDS_H

#include "core/types.h"

/* 主窗体控件 ID */
#define IDC_BTN_NEW      1001
#define IDC_BTN_EDIT     1002
#define IDC_BTN_DEL      1003
#define IDC_BTN_COPY     1004
#define IDC_BTN_LIST     1005
#define IDC_BTN_TABLE    1006
#define IDC_BTN_WEEK     1007
#define IDC_BTN_PIN      1008
#define IDC_BTN_REFRESH  1009
#define IDC_LISTBOX      1010
#define IDC_LISTVIEW     1011
#define IDC_PANEL        1012
#define IDC_STATUS       1013
#define IDC_WEEKVIEW     1014
#define IDC_BTN_NOTE     1015
#define IDC_SEARCH       1016      /* 表格视图的搜索框 */
#define IDC_BTN_PASTE    1017      /* 便笺墙：粘贴便笺 */

#define IDM_NOTE_ONE     2060
#define IDM_NOTE_BATCH   2061
#define IDM_NOTE_ALL     2062
#define IDM_NOTE_CLOSE   2063
#define IDM_NOTE_STYLE   2064
#define IDM_NOTE_COLOR   2065
#define IDM_NOTE_TOPMOST 2066
#define IDM_NOTE_SHOWIMG 2067
#define IDM_NOTE_BOOKMARK 2068     /* \u4e66\u7b7e\u5f62\u6001\uff08\u5438\u9644\u5c4f\u5e55\u8fb9\u7f18\uff09 */
#define IDM_NOTE_TTNEXT  2069      /* \u8bfe\u8868\u4fbf\u7b3a\uff1a\u5012\u8ba1\u65f6\u4e0b\u65b9\u663e\u793a\u4e0b\u4e00\u8282\u8bfe\u540d\u79f0\uff08\u53ef\u70b9\u51fb\uff09 */
#define IDM_EDIT_TODO    2032
#define IDM_BOARD_HIDE   2033      /* 隐藏这张便笺 */
#define IDM_BOARD_SHOWALL 2034     /* 显示所有隐藏的便笺 */
#define IDM_BOARD_HIDEALL 2035     /* 隐藏 / 显示全部便笺 */
#define IDM_BOARD_IMGRST 2036      /* 照片位置与大小复位 */
#define IDM_BOARD_TT     2086      /* 标记 / 取消课表模式 */
#define IDM_BOARD_TTAUTO 2087      /* 课表便笺自动跟随开关 */
#define IDM_BOARD_TTNOW  2088      /* 打开现在这节课的便笺 */
#define IDM_CDU_DAY      2090      /* 倒计时显示到：天 */
#define IDM_CDU_HOUR     2091      /* ……时 */
#define IDM_CDU_MIN      2092      /* ……分 */
#define IDM_CDU_SEC      2093      /* ……秒 */
#define IDM_TT_NEXT      2094      /* 课表便笺：倒计时下方显示下一节课名称（可点击进编辑） */
#define IDM_FILE_DIRSHOW  2017     /* 菜单标签：当前数据位置（只读） */
#define IDM_FILE_DIRSET   2018     /* 选择数据文件夹… */
#define IDM_FILE_IMPORT   2019     /* 迁移 / 导入已有数据… */
#define IDM_FILE_DIROPEN  2040     /* 打开数据文件夹 */
#define IDM_FILE_DIREXPORT 2041    /* 备份 / 导出数据到文件夹… */
#define IDM_FILE_MIGRATE  2045     /* 迁移：在桌面生成一份数据拷贝文件夹 */
#define IDM_FILE_CSV      2042     /* 导入 CSV 课表文件… */
#define IDM_FILE_BACK     2043     /* 后台运行（隐藏主窗口，不占任务栏） */
#define IDM_SEARCH_FOCUS  2044     /* 跳到搜索框（表格视图） */
/* 便笺墙翻页 */
#define IDM_PAGE_PREV     2095
#define IDM_PAGE_NEXT     2096
#define IDM_PAGE_NEW      2097
#define IDM_PAGE_MOVEPREV 2098
#define IDM_PAGE_MOVENEXT 2099
/* 托盘右键菜单（和主窗口命令区分开） */
#define IDM_TRAY_SHOW     2110
#define IDM_TRAY_BACK     2111
#define IDM_TRAY_NOTES    2112
#define IDM_TRAY_STOPSND  2113
#define IDM_TRAY_EXIT     2114
/* 旧数据选择对话框 */
#define IDP_LIST         3080
#define IDP_OK           3081
#define IDP_CANCEL       3082
#define IDM_BOARD_PIN    2080
#define IDM_BOARD_UNPIN  2081
#define IDM_BOARD_AUTO   2082
#define IDM_BOARD_AUTOALL 2083
#define IDM_BOARD_SIZE   2084
#define IDM_BOARD_SIZEALL 2085
#define IDM_WEEK_ZOOMIN  2070
#define IDM_WEEK_ZOOMOUT 2071
#define IDM_WEEK_ZOOMRST 2072

/* 菜单 ID */
#define IDM_FILE_NEW     2001
#define IDM_FILE_SAVE    2002
#define IDM_FILE_RELOAD  2003
#define IDM_FILE_DIR     2004
#define IDM_FILE_ARCH    2006
#define IDM_FILE_HIST    2007
#define IDM_FILE_HISTRST 2008
#define IDM_FILE_AUTORUN 2009
#define IDM_FILE_EXIT    2005
#define IDM_EDIT_EDIT    2010
#define IDM_EDIT_DEL     2011
#define IDM_EDIT_COPY    2012
#define IDM_EDIT_DONE    2013
#define IDM_EDIT_ENABLE  2014
#define IDM_EDIT_RENAME  2015
#define IDM_EDIT_VIEWIMG 2016
#define IDM_EDIT_PASTE   2046      /* 便笺墙：粘贴已复制的便笺 */
#define IDM_VIEW_LIST    2020
#define IDM_VIEW_TABLE   2021
#define IDM_VIEW_WEEK    2022
#define IDM_VIEW_SORTT   2023
#define IDM_VIEW_SORTN   2024
#define IDM_VIEW_SORTC   2025
#define IDM_VIEW_PIN     2026
#define IDM_VIEW_UNPIN   2027
#define IDM_VIEW_CLEAR   2028
#define IDM_VIEW_TODAY   2029
#define IDM_HELP_ABOUT   2030
#define IDM_HELP_HELP    2031

/* 编辑对话框控件 ID */
#define IDE_TITLE        3001
#define IDE_DATE_S       3002
#define IDE_TIME_S       3003
#define IDE_HASEND       3004
#define IDE_DATE_E       3005
#define IDE_TIME_E       3006
#define IDE_REP          3007
#define IDE_INTERVAL     3008
#define IDE_UNIT         3009
#define IDE_WD_BASE      3010      /* 3010..3016 周日..周六 */
#define IDE_NOTE         3020
#define IDE_LINK         3021
#define IDE_LINKOPEN     3022
#define IDE_ATT_LIST     3023
#define IDE_ATT_IMG      3024
#define IDE_ATT_FILE     3025
#define IDE_ATT_DEL      3026
#define IDE_ATT_OPEN     3027
#define IDE_ATT_UP       3028
#define IDE_ATT_DOWN     3029
#define IDE_DONE         3030
#define IDE_ENABLED      3031
#define IDE_FONT         3032      /* 字体系列 */
#define IDE_SIZE         3033      /* 字号 */
#define IDE_BOLD         3034
#define IDE_ITALIC       3035
#define IDE_UNDERLINE    3036
#define IDE_FMT_ALL      3037      /* 整篇应用 */
#define IDE_PREVIEW      3038      /* 附件预览区 */
#define IDE_ATT_VIEW     3039      /* 查看原图 */
#define IDE_COLOR_BASE   3040      /* 3040..3047 调色板 */
#define IDE_ANNIV        3051
#define IDE_TODO         3054      /* 无时间待办 */
#define IDE_SHOWIMG      3055      /* 便笺上直接显示图片 */
#define IDE_REM_POPUP    3060      /* 提醒：便笺弹窗 */
#define IDE_REM_TOAST    3061      /* 提醒：通知栏 */
#define IDE_REM_SOUND    3062      /* 提醒：提示音 */
#define IDE_REM_LEAD     3063      /* 提前分钟 */
#define IDE_REM_REPEAT   3064      /* 重复间隔分钟 */
#define IDE_REM_TIMES    3065      /* 次数 */
#define IDE_REM_RING     3066      /* 提示音来源：系统提示音 / 自选声音文件… */
#define IDE_REM_RING_INFO 3067     /* 提示音来源的文字（只读） */
#define IDE_TT           3070      /* 课表模式 */
#define IDE_CDU_DAY      3071      /* 倒计时显示到：天/时/分/秒 */
#define IDE_CDU_HOUR     3072
#define IDE_CDU_MIN      3073
#define IDE_CDU_SEC      3074
#define IDE_TTNEXT       3075      /* \u8bfe\u8868\uff1a\u5012\u8ba1\u65f6\u4e0b\u65b9\u663e\u793a\u4e0b\u4e00\u8282\u8bfe\u540d\u79f0 */
#define IDE_TTNEXT       3075      /* \u8bfe\u8868\uff1a\u5012\u8ba1\u65f6\u4e0b\u65b9\u663e\u793a\u4e0b\u4e00\u8282\u8bfe\u540d\u79f0 */
#define IDE_TT_NEXT      3075      /* 显示下一节课名称（独立开关） */
#define IDE_NOTESTYLE    3052
#define IDE_NOTECOLOR    3053
#define IDE_COLOR_MORE   3050
#define IDE_OK           1
#define IDE_CANCEL       2
#define IDE_DEL          3

#endif /* ZS_UI_IDS_H */
