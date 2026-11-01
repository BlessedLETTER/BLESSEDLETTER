/* ZSchedule v2.0 · core atom */
#include "core/types.h"
#include "core/strbuf.h"
#include "core/textfile.h"


void zs_esc_append(WStr *out, const wchar_t *s)
{
    if (!s) return;
    for (; *s; s++) {
        if (*s == L'\\')      ws_add(out, L"\\\\");
        else if (*s == L'\n') ws_add(out, L"\\n");
        else if (*s == L'\r') ws_add(out, L"\\r");
        else                  ws_ch(out, *s);
    }
}

void zs_unesc_into(wchar_t *dst, size_t cch, const wchar_t *src)
{
    size_t i = 0;
    if (!cch) return;
    while (*src && i + 1 < cch) {
        if (src[0] == L'\\' && src[1]) {
            src++;
            if (*src == L'n')      dst[i++] = L'\n';
            else if (*src == L'r') dst[i++] = L'\r';
            else                   dst[i++] = *src;
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = 0;
}

int zs_write_all(const wchar_t *path, const wchar_t *data, size_t wchars)
{
    HANDLE h;
    DWORD wrote = 0;
    unsigned short bom = 0xFEFF;
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    WriteFile(h, &bom, 2, &wrote, NULL);
    WriteFile(h, data, (DWORD)(wchars * sizeof(wchar_t)), &wrote, NULL);
    CloseHandle(h);
    return 1;
}

wchar_t *zs_read_all(const wchar_t *path, size_t *outLen)
{
    HANDLE h;
    DWORD size, rd = 0;
    wchar_t *buf;
    size_t n;
    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size < 2 || size > 64u * 1024u * 1024u) {
        CloseHandle(h);
        return NULL;
    }
    buf = (wchar_t *)malloc(size + sizeof(wchar_t) * 2);
    if (!buf) { CloseHandle(h); return NULL; }
    ReadFile(h, buf, size, &rd, NULL);
    CloseHandle(h);
    n = rd / 2;
    if (n && buf[0] == 0xFEFF) {          /* 跳过 BOM */
        memmove(buf, buf + 1, (n - 1) * sizeof(wchar_t));
        n--;
    }
    buf[n] = 0;
    *outLen = n;
    return buf;
}

/* ==========================================================================
 *  历史版本：每次写盘前把旧文件备一份到 zsched_history\
 *  （至少间隔 2 分钟才存一份，最多保留 60 份）
 * ========================================================================*/
