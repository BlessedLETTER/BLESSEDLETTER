/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/paths.h"


void zs_exe_dir(wchar_t *out, size_t cch)
{
    wchar_t exe[MAX_PATH];
    wchar_t *p;
    size_t n;
    out[0] = 0;
    if (!GetModuleFileNameW(NULL, exe, MAX_PATH)) return;
    p = exe + wcslen(exe);
    while (p > exe && p[-1] != L'\\' && p[-1] != L'/') p--;
    *p = 0;
    n = wcslen(exe);
    wcsncpy(out, exe, cch - 1);
    out[cch - 1] = 0;
    if (n && out[n - 1] != L'\\') wcsncat(out, L"\\", cch - wcslen(out) - 1);
}

/* "\u76ee\u5f55 + \u6587\u4ef6\u540d" \u62fc\u8def\u5f84\uff08\u81ea\u52a8\u8865\u4e0a\u4e2d\u95f4\u7684\u53cd\u659c\u6760\uff09 */
void zs_path_join(wchar_t *out, size_t cch, const wchar_t *dir, const wchar_t *name)
{
    size_t n;
    wcsncpy(out, dir ? dir : L"", cch - 1);
    out[cch - 1] = 0;
    n = wcslen(out);
    if (n && out[n - 1] != L'\\' && out[n - 1] != L'/') {
        wcsncat(out, L"\\", cch - wcslen(out) - 1);
    }
    wcsncat(out, name, cch - wcslen(out) - 1);
}

/* \u53d6\u6587\u4ef6\u6240\u5728\u76ee\u5f55\uff08\u5e26\u5c3e \\\uff09 */
void zs_dir_of_path(const wchar_t *file, wchar_t *out, size_t cch)
{
    const wchar_t *slash = NULL, *q;
    size_t n;
    out[0] = 0;
    if (!file || !file[0]) return;
    for (q = file; *q; q++)
        if (*q == L'\\' || *q == L'/') slash = q;
    n = slash ? (size_t)(slash - file + 1) : 0;
    if (n >= cch) n = cch - 1;
    memcpy(out, file, n * sizeof(wchar_t));
    out[n] = 0;
}

int zs_file_exists(const wchar_t *path)
{
    DWORD a;
    if (!path || !path[0]) return 0;
    a = GetFileAttributesW(path);
    return (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

int zs_dir_exists(const wchar_t *path)
{
    DWORD a;
    if (!path || !path[0]) return 0;
    a = GetFileAttributesW(path);
    return (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0;
}

int zs_copy_file(const wchar_t *src, const wchar_t *dst)
{
    if (!zs_file_exists(src)) return 0;
    return CopyFileW(src, dst, FALSE) ? 1 : 0;
}

/* \u6570\u4e00\u4e2a UTF-16 \u6587\u4ef6\u91cc\u51fa\u73b0\u4e86\u591a\u5c11\u6b21\u67d0\u4e2a\u6807\u8bb0\uff08\u7528\u6765\u7ed9\u5019\u9009\u6570\u636e\u201c\u6570\u6761\u6570\u201d\uff09 */
