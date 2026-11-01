#ifndef ZS_CORE_PATHS_H
#define ZS_CORE_PATHS_H

#include "core/types.h"

/* 路径原子：v2.0 重写中，接口见 core 层实现 */

void zs_exe_dir(wchar_t *out, size_t cch);
void zs_path_join(wchar_t *out, size_t cch, const wchar_t *dir, const wchar_t *name);
void zs_dir_of_path(const wchar_t *file, wchar_t *out, size_t cch);
int  zs_file_exists(const wchar_t *path);
int  zs_dir_exists(const wchar_t *path);
int  zs_copy_file(const wchar_t *src, const wchar_t *dst);

#endif /* ZS_CORE_PATHS_H */
