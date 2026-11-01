#ifndef ZS_MOD_SCHEDULE_H
#define ZS_MOD_SCHEDULE_H

#include "core/types.h"
#include "core/appdata.h"

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
/* 纪念日：推算已过去的时间 */
int         sched_anniv_elapsed(const Schedule *s, int *years, int *months, int *days);
int         sched_anniv_days(const Schedule *s, int *pFuture);
const wchar_t *sched_anniv_text(const Schedule *s, wchar_t *buf, size_t cch);   /* 倒数日：过去/将来都算 */
time_t      sched_anniv_next_round(const Schedule *s, int *roundNo);   /* 下一个周年 */

void        model_seed_sample(void);

void        zs_migration_backup_once(const wchar_t *dataFile);

#endif /* ZS_MOD_SCHEDULE_H */
