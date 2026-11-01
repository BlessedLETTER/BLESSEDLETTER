#ifndef ZS_CORE_TYPES_H
#define ZS_CORE_TYPES_H


/* ============================================================================
 *  ZSchedule · 桌面日程管家   (纯 C / Win32 API)
 *  zsched.h - 公共类型、主题常量、模块接口
 * ==========================================================================*/
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
#define APP_VERSION     L"2.0.0"      /* 当前版本号（关于框显示） */
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
typedef struct {
    wchar_t *buf;
    size_t   cap;
    size_t   len;
} WBuf;

#endif /* ZS_CORE_TYPES_H */
