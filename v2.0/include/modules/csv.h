#ifndef ZS_MOD_CSV_H
#define ZS_MOD_CSV_H

#include "core/types.h"

/* -------------------------------------------------------- csvimport.c 接口 */
/* 导入 CSV 课表：正则识别表头/时间，其余信息写进正文，逐行生成课表便笺 */
void   csv_import_show(HWND owner);

#endif /* ZS_MOD_CSV_H */
