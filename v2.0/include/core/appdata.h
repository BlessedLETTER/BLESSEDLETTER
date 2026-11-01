#ifndef ZS_CORE_APPDATA_H
#define ZS_CORE_APPDATA_H

#include "core/types.h"

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

#endif /* ZS_CORE_APPDATA_H */
