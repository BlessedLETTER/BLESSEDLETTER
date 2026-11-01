/* ============================================================================
 *  ZSchedule · 桌面日程管家   (纯 C / Win32 API)
 *  zsched.h - 公共类型、主题常量、模块接口
 * ==========================================================================*/
#ifndef ZSCHED_H
#define ZSCHED_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601        /* Windows 7 及以上 */
#endif

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <wchar.h>

/* ------------------------------------------------------------------ 常量 */
#define APP_TITLE       L"笺"          /* 任务栏 / 窗口标题显示的名字 */
#define APP_VERSION     L"1.9.4"      /* 当前版本号（关于框显示） */
#define APP_AUTHOR      L"祝筱笺"
#define APP_URL         L"https://github.com/BlessedLETTER/BLESSEDLETTER/tree/BlessedMemo"
#define APP_ICON_ID     1                 /* zsched.rc 里的 ICON 资源 id */
#define MAIN_CLASS      L"ZScheduleMainWnd"
#define EDIT_CLASS      L"ZScheduleEditWnd"
#define PANEL_CLASS     L"ZSchedulePanelWnd"
#define WEEK_CLASS      L"ZScheduleWeekWnd"
#define IMGV_CLASS      L"ZScheduleImageViewWnd"
#define FLATBTN_CLASS   L"ZScheduleFlatButton"
#define NOTE_CLASS      L"ZScheduleStickyNote"
#define DATA_FILE_NAME  L"zsched_data.txt"
#define DATA_NOTES_NAME L"zsched_notes.txt"
#define DATA_BOARD_NAME L"zsched_board.txt"
#define DATA_HIST_NAME  L"zsched_history"

#define MAX_ATTACH      12
#define MAX_IMGS        6                     /* 一张便笺最多贴几张照片 */
enum { IMGM_FLOAT = 0, IMGM_INLINE = 1, IMGM_END = 2 };   /* 浮动 / 随文（文中）/ 文末 */
enum { IMGL_OVER = 0, IMGL_UNDER = 1 };
enum { REM_POPUP = 1, REM_TOAST = 2, REM_SOUND = 4 };      /* 提醒方式位组合 */                   /* 浮于文字上方 / 衬于文字下方 */
/* 提示音来源：系统提示音（MessageBeep）/ 自选声音文件（提醒时用系统默认程序打开它） */
enum { SND_SYS = 0, SND_FILE = 1 };
#define MAX_TITLE       128
#define MAX_NOTE        4000       /* 正文纯文本（摘要用） */
#define MAX_RTF         16000      /* 正文富文本 RTF 源码 */
#define MAX_LINK        512

/* 重复方式 */
enum { REP_NONE = 0, REP_WEEKLY = 1, REP_CUSTOM = 2 };
/* 自定义重复单位 */
enum { UNIT_DAY = 0, UNIT_WEEK = 1, UNIT_MONTH = 2, UNIT_YEAR = 3 };
/* 视图模式（commctrl.h 里已有 VIEW_LIST 宏，故加前缀） */
enum { ZS_VIEW_LIST = 0, ZS_VIEW_TABLE = 1, ZS_VIEW_WEEK = 2 };
/* 排序模式 */
enum { SORT_TIME = 0, SORT_TITLE = 1, SORT_START = 2 };
/* 日程类型：普通日程 / 纪念日（记录已经过去的事） */
enum { KIND_TASK = 0, KIND_ANNIV = 1, KIND_TODO = 2 };   /* 普通日程 / 纪念日 / 无时间待办 */

/* 课表模式：标记为课表的日程到点自动开关便笺；
   没填结束时间时，一节课按 45 分钟算 */
#define TT_CLASS_MIN    45

/* 倒计时显示精度：显示到哪一级（CDU_DEF = 未设置，同“秒”） */
enum { CDU_DEF = 0, CDU_DAY = 1, CDU_HOUR = 2, CDU_MIN = 3, CDU_SEC = 4 };
/* 便笺外观：跟随系统窗口 / 便笺纸贴在桌面 */
enum { NOTE_STYLE_SYSTEM = 0, NOTE_STYLE_PAPER = 1, NOTE_STYLE_BOOKMARK = 2 };
/* 书签吸附的边 */
enum { NOTE_EDGE_LEFT = 0, NOTE_EDGE_RIGHT = 1, NOTE_EDGE_TOP = 2, NOTE_EDGE_BOTTOM = 3 };
#define NOTE_BM_THICK   34     /* 书签厚度（逻辑像素） */
#define NOTE_BM_LONG    240    /* 书签长度（逻辑像素） */
#define NOTE_COLOR_COUNT 8

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

/* --------------------------------------------------------------- 数据结构 */
typedef struct {
    int        id;                       /* 唯一编号，>0 */
    wchar_t    title[MAX_TITLE];         /* 标题 */
    wchar_t    note[MAX_NOTE];           /* 正文纯文本（由富文本提取，用于摘要） */
    wchar_t    noteRtf[MAX_RTF];         /* 正文富文本 RTF，空串表示纯文本 */
    wchar_t    link[MAX_LINK];           /* 关联链接 */
    SYSTEMTIME start;                    /* 开始日期时间（含秒） */
    int        hasEnd;                   /* 是否设置结束时间 */
    SYSTEMTIME end;                      /* 结束日期时间 */
    int        repKind;                  /* REP_* */
    int        repInterval;              /* 每 N ...（>=1） */
    int        repUnit;                  /* UNIT_*，仅 REP_CUSTOM 有效 */
    int        weekMask;                 /* bit0=周日 … bit6=周六，仅 REP_WEEKLY */
    wchar_t    attach[MAX_ATTACH][MAX_PATH]; /* 附件 / 图片路径 */
    int        attachCount;
    int        enabled;                  /* 是否启用（参与倒计时） */
    int        done;                     /* 是否已完成 */
    COLORREF   color;                    /* 类别颜色 */
    int        kind;                     /* KIND_*：普通日程 / 纪念日 / 无时间待办 */
    int        showImg;                  /* 便笺上直接显示插入的图片 */
    int        imgPct;                   /* 自动排布时图片占内容区高度的百分比，0 = 默认 55 */
    int        imgX, imgY, imgW, imgH;    /* 旧版共用框（千分比），读旧数据时并入 imgB[0] */
    int        imgN;                      /* 显示几张图片（0 = 全部，最多 MAX_IMGS） */
    int        imgMode;                   /* IMGM_* 图片版式：浮动 / 随文 / 文末 */
    int        imgLayer;                  /* IMGL_* 图片层次：浮于文字上方 / 衬于文字下方 */
    /* 提醒：方式可多选，提前/间隔/次数可自定义 */
    int        remind;                    /* 0 = 关；REM_POPUP|REM_TOAST|REM_SOUND 位组合 */
    int        remLead;                   /* 提前多少分钟提醒 */
    int        remRepeat;                 /* 重复间隔（分钟，0 = 不重复） */
    int        remTimes;                  /* 最多提醒几次 */
    int        sndMode;                   /* SND_SYS = 系统提示音 / SND_FILE = 自选声音文件 */
    wchar_t    sndFile[MAX_PATH];         /* 自选铃声文件：提醒时自动用系统默认程序打开它 */
    int        imgB[MAX_IMGS][4];         /* 每张图片各自的框：{x,y,w,h} 千分比（相对内容区），w/h<=0 = 自动 */
    /* 课表与倒计时显示 */
    int        timetable;                 /* 1 = 课表：到点自动开关便笺、内容跟随上课时间 */
    int        cdUnit;                    /* CDU_*：倒计时显示到哪一级（0 = 默认秒） */
    int        ttNext;                    /* 1 = \u8bfe\u8868\u4fbf\u7b3a\u5728\u5012\u8ba1\u65f6\u4e0b\u65b9\u518d\u663e\u793a\u201c\u4e0b\u4e00\u8282\u8bfe\u201d\u540d\u79f0\uff08\u53ef\u70b9\u51fb\uff09\uff1b0 = \u4e0d\u663e\u793a */

    /* 运行期缓存（不参与存储） */
    time_t     durSec;                   /* 起止时长（秒） */
    time_t     nextOcc;                  /* 下一次发生时间，0 表示无 */
} Schedule;

typedef struct {
    Schedule *items;
    int       count;
    int       cap;
} SchedList;

/* ------------------------------------------------- 可变宽字符缓冲区 / 串 */
typedef struct {
    wchar_t *p;
    size_t   len;
    size_t   cap;
} WStr;

void  ws_init(WStr *s);
void  ws_free(WStr *s);
int   ws_reserve(WStr *s, size_t need);   /* 0 = 成功 */
void  ws_add(WStr *s, const wchar_t *txt);
void  ws_ch(WStr *s, wchar_t c);
void  ws_int(WStr *s, long long v);
void  ws_u2(WStr *s, unsigned v);

/* 固定长度缓冲写入器（不会溢出） */
typedef struct {
    wchar_t *buf;
    size_t   cap;
    size_t   len;
} WBuf;

void  wb_init(WBuf *b, wchar_t *buf, size_t cap);
void  wb_add(WBuf *b, const wchar_t *txt);
void  wb_ch(WBuf *b, wchar_t c);
void  wb_int(WBuf *b, long long v);
void  wb_u2(WBuf *b, unsigned v);

/* ------------------------------------------------------------ model.c 接口 */
void        model_init(void);
void        model_free(void);
int         model_count(void);
Schedule   *model_at(int idx);
Schedule   *model_by_id(int id);
int         model_index_by_id(int id);
Schedule   *model_add(const Schedule *src);
Schedule   *model_dup(int id);
void        model_remove(int id);
void        model_normalize(Schedule *s);
void        model_refresh_next(void);

int         model_load(const wchar_t *path);
int         model_save(const wchar_t *path);
int         model_loaded_legacy(void);
const wchar_t *model_path(void);
/* 历史版本存档 */
const wchar_t *model_history_dir(void);
int   model_archive(const wchar_t *why);      /* 把当前数据存一份到历史目录 */
int   model_history_count(void);
int   model_history_latest(wchar_t *out, size_t cch);   /* 最近一份的路径 */
int   model_history_restore_latest(void);     /* 恢复最近一份历史版本 */
void        model_set_path(const wchar_t *path);

/* ---- 数据保存位置：统一目录 + 自定义路径 + 旧数据导入 ---- */
typedef struct {
    wchar_t   dir[MAX_PATH];        /* 数据所在目录（末尾带 \） */
    wchar_t   dataFile[MAX_PATH];   /* 主数据文件全路径 */
    int       items;                /* 主数据里的日程条数 */
    int       notes;                /* 便笺状态文件里的条数 */
    int       hasNotes;             /* 有没有便笺状态文件 */
    long long mtime;                /* 主数据修改时间（FILETIME，用来排序） */
} AppDataCand;

void           app_data_init(void);                        /* 启动时读设置，定下数据目录 */
const wchar_t *app_data_dir(void);                         /* 当前数据目录 */
int            app_data_dir_set(const wchar_t *dir, int migrate);  /* 换目录；migrate=1 先把现有文件复制过去 */
int            app_data_copy_to(const wchar_t *dstDir);    /* 把当前三份文件（+历史）复制到目标目录 */
int            app_data_find(AppDataCand *out, int max);   /* 找本机已有的数据（按修改时间新→旧） */
void           app_data_reload(void);                      /* 按新的 model_path 重新载入主数据 */
int            app_data_import(const AppDataCand *c);      /* 先存档当前，再切到这一份（不覆盖任何文件） */
void        model_seed_sample(void);

/* 纪念日：推算已过去的时间 */
int         sched_anniv_elapsed(const Schedule *s, int *years, int *months, int *days);
int         sched_anniv_days(const Schedule *s, int *pFuture);
const wchar_t *sched_anniv_text(const Schedule *s, wchar_t *buf, size_t cch);   /* 倒数日：过去/将来都算 */
time_t      sched_anniv_next_round(const Schedule *s, int *roundNo);   /* 下一个周年 */

/* 周期与时间计算 */
time_t      sched_next_occurrence(const Schedule *s, time_t now);
time_t      sched_occurrence_end(const Schedule *s, time_t occ);
time_t      sched_occurrence_on(const Schedule *s, long long dayOrd);
long long   sched_start_day(const Schedule *s);
int         sched_day_count(time_t from, time_t to);
long long   civil_days(int y, int m, int d);
void        civil_from_days(long long z, int *py, int *pm, int *pd);
int         days_in_month(int y, int m);
int         weekday_of_day(long long ord);     /* 0=周日 */
long long   monday_of(long long ord);          /* 所在周的周一序号 */
long long   today_ord(void);
time_t      time_from_ord(long long ord, int hh, int mi, int ss);

/* 文本格式化 */
const wchar_t *repeat_text(const Schedule *s, wchar_t *buf, size_t cch);
const wchar_t *sched_first_image(const Schedule *s, wchar_t *buf, size_t cch);  /* 首张图片附件，无则 NULL */
const wchar_t *dt_text(const SYSTEMTIME *st, wchar_t *buf, size_t cch);
const wchar_t *timet_text(time_t t, wchar_t *buf, size_t cch);
const wchar_t *date_text(time_t t, wchar_t *buf, size_t cch);
const wchar_t *clock_text(time_t t, wchar_t *buf, size_t cch);
const wchar_t *countdown_text(time_t occ, wchar_t *buf, size_t cch);
void           countdown_split(time_t occ, long long *days,
                               int *hh, int *mi, int *ss, int *state);
int            cd_unit_of(const Schedule *s);                 /* 有效倒计时精度 */
const wchar_t *cd_text(const Schedule *s, time_t occ, wchar_t *buf, size_t cch);
const wchar_t *cd_text_u(time_t occ, int unit, wchar_t *buf, size_t cch);
/* 课表：今天这节课的 [开始, 结束)、此刻正在上的课、下一节 */
int            sched_class_window(const Schedule *s, time_t *pStart, time_t *pEnd);
Schedule      *sched_class_now(void);
Schedule      *sched_class_next(time_t after, time_t *pStart, time_t *pEnd);
Schedule      *sched_class_next_any(time_t after, time_t *pStart, time_t *pEnd);  /* 含明天/下周 */
const wchar_t *tt_when_text(time_t t, wchar_t *buf, size_t cch);    /* 下一节课的时间文字 */
const wchar_t *weekday_names_text(int mask, wchar_t *buf, size_t cch);
const wchar_t *file_name_of(const wchar_t *path, wchar_t *buf, size_t cch);
const wchar_t *status_text(const Schedule *s, wchar_t *buf, size_t cch);
void           rtf_to_plain(const wchar_t *rtf, wchar_t *out, size_t cch);
int            path_is_image(const wchar_t *path);

/* ------------------------------------------------------------ image.c 接口 */
int   img_init(void);
void  img_shutdown(void);
int   img_is_image(const wchar_t *path);
int   img_size(const wchar_t *path, int *pw, int *ph);
void  img_draw(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);
void  img_draw_cover(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);
void  img_draw_cover_full(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);
/* v1.9：等比“完整显示”（contain，允许放大、不裁剪）——桌面便笺的照片用这个 */
void  img_draw_fit(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);
int   sched_image_count(const Schedule *s);
const wchar_t *sched_image_at(const Schedule *s, int k, wchar_t *buf, size_t cch);

/* 便笺上图片框的几何（便笺墙与桌面便笺共用） */
#define IMG_MIN_W     72          /* 图片最小尺寸（逻辑像素） */
#define IMG_MIN_H     54
#define IMG_HDL       11          /* 手柄边长（逻辑像素） */
enum { IMG_HDL_NONE = 0, IMG_HDL_L = 1, IMG_HDL_R = 2, IMG_HDL_T = 4, IMG_HDL_B = 8 };

int   img_box_in(const RECT *content, const Schedule *s, int minTextH, int minImgH,
                 int minImgW, RECT *out);   /* 返回 1 = 有图片框 */
void  img_box_store(const RECT *content, const RECT *box, Schedule *s);
int   img_handle_at(const RECT *box, int x, int y, int hdl);   /* 命中哪个手柄 */
void  img_draw_zoom(HDC hdc, const wchar_t *path, int x, int y, int w, int h,
                    int zoomIn);
void  img_draw_file_icon(HDC hdc, const wchar_t *path, int x, int y, int cx, int cy);

/* --------------------------------------------------------- richtext.c 接口 */
int    rt_init(void);
const wchar_t *rt_class(void);
int    rt_available(void);
void   rt_set_text(HWND h, const wchar_t *rtf, const wchar_t *plain);
int    rt_get_rtf(HWND h, wchar_t *out, int cch);
void   rt_apply(HWND h, const wchar_t *face, int pt,
                int bold, int italic, int underline,
                int colorSet, COLORREF color, int toAll);
void   rt_get_format(HWND h, wchar_t *face, int facecch,
                     int *pt, int *bold, int *italic, int *underline,
                     COLORREF *color, int *autoColor);
int    rt_enum_fonts(wchar_t fonts[][LF_FACESIZE], int maxFonts);

/* ------------------------------------------- 富文本分段（便笺逐段字号/字体） */
#define MAX_RUNS       320
#define MAX_RUN_FONTS  16
#define MAX_RUN_COLORS 24      /* RTF 颜色表上限（\cfN 用） */

typedef struct {
    int off, len;                 /* 在 RichDoc.text 中的范围 */
    int sizeHalf;                 /* 字号（半磅），0 = 用便笺默认 */
    int bold, italic, underline;
    int font;                     /* 字体表下标，-1 = 默认 */
    int color;                    /* 颜色表下标，0 = 自动（用便笺默认字色） */
} RichRun;

typedef struct {
    wchar_t  text[MAX_NOTE + 8];
    int      len;
    RichRun  runs[MAX_RUNS];
    int      runCount;
    wchar_t  faces[MAX_RUN_FONTS][LF_FACESIZE];
    int      faceCount;
    COLORREF colors[MAX_RUN_COLORS];   /* RTF \colortbl（下标 0 = 自动） */
    int      colorCount;
} RichDoc;

RichDoc *rich_doc(void);                 /* 静态实例（画制单线程） */
void rich_parse(const wchar_t *rtf, const wchar_t *plain, RichDoc *doc);
int  rich_draw(HDC hdc, const RichDoc *doc, const RECT *box,
               int baseHalfPt, int scalePct, COLORREF color, int draw);
void rich_fonts_free(void);

/* ---------------------------------------------------------- imgview.c 接口 */
void   iv_register_class(HINSTANCE hInst);
HWND   iv_open(HWND owner, const wchar_t *const *paths, int count, int startIndex);

/* --------------------------------------------------------- weekview.c 接口 */
void   week_register_class(HINSTANCE hInst);
HWND   week_create(HWND parent, HINSTANCE hInst, int id);
void   week_refresh(HWND h);
void   week_shift(HWND h, int weeks);
void   week_goto_today(HWND h);
void   week_set_sel(HWND h, int id);
int    week_sel_id(HWND h);
void   week_set_focus_time(HWND h, const SYSTEMTIME *st);
void   week_goto_time(HWND h, const SYSTEMTIME *st);
long long week_monday(HWND h);
void   week_zoom(HWND h, int delta);          /* 每档 +1，0 为恢复默认 */

/* ----------------------------------------------------------- stickies.c */
void   stickies_register_class(HINSTANCE hInst);
HWND   stickies_create(HWND parent, HINSTANCE hInst, int id);
void   stickies_set_items(HWND h, const int *ids, int count);
int    stickies_sel(HWND h);
void   stickies_set_sel(HWND h, int id);
void   stickies_save_pos(void);
void   stickies_refresh(HWND h);
int    stickies_is_pinned(int id);
void   stickies_set_pinned(int id, int on);
int    stickies_has_size(int id);
void   stickies_reset_size(int id);
void   stickies_reset_size_all(void);
void   stickies_reset_pos(int id);
void   stickies_reset_all(void);
int    stickies_is_hidden(int id);
void   stickies_set_hidden(int id, int on);
int    stickies_hidden_count(void);
void   stickies_show_all(void);
void   stickies_hide_all(int on);
void   stickies_path_forget(void);          /* 换数据目录后失效便笺墙状态文件路径 */
/* 便笺墙翻页（v1.6）：一页放不下就新开一页 */
int    stickies_page_get(HWND h);           /* 当前第几页（1 起） */
int    stickies_page_count(HWND h);         /* 一共几页 */
int    stickies_page_full(HWND h);          /* 1 = 这一页装满了 */
int    stickies_page_of(int id);            /* 这张便笺在第几页 */
void   stickies_page_set(HWND h, int page);
void   stickies_page_new(HWND h);           /* 新开一页并翻过去 */
void   stickies_move_page(int id, int delta);  /* 把便笺移到上/下一页（-1/+1） */
void   note_set_main(HWND h);          /* 桌面便笺把消息回到主窗口 */

/* -------------------------------------------------------------- paper.c */
void      paper_init(void);
void      paper_free(void);
HBRUSH    paper_brush(COLORREF base);           /* 带纤维纹理的纸纹刷 */
void      paper_fill(HDC hdc, const RECT *rc, COLORREF base, int alignOrg);
/* 纸张卡片：带柔和投影 + 细边 + 纹理 */
void      paper_card(HDC hdc, const RECT *rc, COLORREF base, int radius,
                     int lifted, COLORREF border);
void      paper_shadow_only(HDC hdc, const RECT *rc, int radius, int depth);
void      paper_fold_corner(HDC hdc, const RECT *rc, COLORREF base, int size);
void      paper_rules(HDC hdc, const RECT *rc, int top, int step, COLORREF col);
COLORREF  paper_shade(COLORREF c, int delta);
void      paper_notebook(HDC hdc, const RECT *client, int scale,
                         RECT *pageOut, int *contentLeftOut);   /* delta>0 变暗 */

/* ----------------------------------------------------------- noteview.c */
void      note_register_class(HINSTANCE hInst);
void      note_open(int schedId, int style, int colorIndex, int activate);
void      note_open_batch(const int *ids, int count, int style, int colorIndex);
void      note_close_all(void);
void      note_refresh_all(void);
void      note_save_state(void);
void      note_load_state(void);
int       note_is_open(int id);
int       note_count(void);
void      note_set_style(int id, int style);
void      note_toggle_top(int id);
void      note_set_color(int id, int colorIndex);
void      note_close_one(int id);
void      note_get_look(int id, int *style, int *colorIndex);
int       note_list(int *ids, int max);                /* 当前打开的便笺 id 列表 */
int       note_get_geom(int id, int *style, int *colorIndex, RECT *rc);
void      note_open_at(int id, int style, int colorIndex, const RECT *rc, int activate);
int       note_look_remembered(int id, int *style, int *colorIndex);
int       note_tt_auto_get(void);                      /* 课表便笺自动跟随 */
void      note_tt_auto_set(int on);
void      note_path_forget(void);                      /* 换数据目录后失效便笺状态文件路径 */
void      note_set_bookmark(int id, int on);           /* 书签形态：吸附屏幕边缘 */
int       note_is_bookmark(int id);
extern const COLORREF g_noteColors[NOTE_COLOR_COUNT];
extern const wchar_t *g_noteColorNames[NOTE_COLOR_COUNT];

/* ------------------------------------------------------------- 扁平按钮 */
void   flat_register_class(HINSTANCE hInst);
void   flat_set_kind(HWND h, int primary, int toggle);
HWND   flat_create(HWND parent, const wchar_t *text, int id, int primary, int toggle);
void   flat_check(HWND h, int checked);

/* ---------------------------------------------------------- ui_edit.c 接口 */
int    edit_show(HWND owner, Schedule *item, int isNew);
int    edit_show_at(HWND owner, Schedule *item, const SYSTEMTIME *prefStart);
void   edit_register_class(HINSTANCE hInst);

/* -------------------------------------------------------- csvimport.c 接口 */
/* 导入 CSV 课表：正则识别表头/时间，其余信息写进正文，逐行生成课表便笺 */
void   csv_import_show(HWND owner);

/* ---------------------------------------------------------- ui_main.c 接口 */
extern HINSTANCE g_hInst;

HWND   main_create(void);
void   main_register_classes(HINSTANCE hInst);
void   main_rebuild_views(void);
void   main_tick(void);
int    main_selected_id(void);
int    main_handle_shortcut(MSG *msg, int isDialogMsg);
void   main_set_status(const wchar_t *msg);
/* 提醒与开机自启动 */
void   remind_tick(void);
void   remind_show_popup(const Schedule *s);
/* 自选铃声：用系统默认程序打开这个声音文件（“自动打开所选声音文件”） */
void   snd_open_file(const wchar_t *path);
/* 提示音来源的文字：系统提示音 / 自选文件名 */
const wchar_t *snd_desc(const Schedule *s, wchar_t *buf, size_t cch);
/* 声音播放（sound.c）：返回实际走的通道 */
enum { SNDV_NONE = 0, SNDV_MF = 1, SNDV_MCI = 2, SNDV_SHELL = 3 };
int    snd_play_file(const wchar_t *path);   /* 1=Media Foundation 2=MCI 3=交给系统程序 0=失败 */
void   snd_stop(void);
int    snd_is_active(void);
void   snd_shutdown(void);
int    app_autostart_get(void);
void   app_autostart_set(int on);
COLORREF palette_color(int idx);
extern const COLORREF g_palette[8];
void   main_refresh_all(void);      /* 数据变更后刷新全部界面（含便笺窗） */
int    main_pick_ids(int *out, int maxOut);   /* 取当前选中的日程（支持多选） */

#endif /* ZSCHED_H */
