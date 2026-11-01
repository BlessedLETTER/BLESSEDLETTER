/* ============================================================================
 *  ZSchedule - model.c
 *  数据模型、周期（重复）推算、倒计时计算、文本格式化、文件读写
 * ==========================================================================*/
#include "zsched.h"
#ifdef RTF_TRACE
#  include <stdio.h>
#endif
#ifdef RTF_TRACE
#  include <stdio.h>
#endif

/* =======================================================================
 *  一、可增长的宽字符缓冲区
 * =====================================================================*/
const wchar_t *sched_anniv_text(const Schedule *s, wchar_t *buf, size_t cch)
{
    WBuf b;
    int future = 0, d;
    int roundNo = 0;
    time_t next;
    if (!s || !buf) return L"";
    d = sched_anniv_days(s, &future);
    wb_init(&b, buf, cch);
    if (d == 0)                      wb_add(&b, L"\u5c31\u662f\u4eca\u5929");
    else if (future) { wb_add(&b, L"\u8fd8\u6709 "); wb_int(&b, d); wb_add(&b, L" \u5929"); }
    else { wb_add(&b, L"\u5df2\u8fc7\u53bb "); wb_int(&b, d); wb_add(&b, L" \u5929"); }
    next = sched_anniv_next_round(s, &roundNo);
    /* 还没到的日子不算“距下次”（那会和“还有 N 天”重复） */
    if (!future && next > 0 && roundNo > 0 && s->repKind != REP_NONE) {
        struct tm *pt;
        time_t now = time(NULL);
        int y1, m1, d1, y2, m2, d2;
        pt = localtime(&now);
        if (pt) {
            y1 = pt->tm_year + 1900; m1 = pt->tm_mon + 1; d1 = pt->tm_mday;
            pt = localtime(&next);
            if (pt) {
                y2 = pt->tm_year + 1900; m2 = pt->tm_mon + 1; d2 = pt->tm_mday;
                wb_add(&b, L"\uff08\u8ddd\u4e0b\u6b21 ");
                wb_int(&b, (long long)(civil_days(y2, m2, d2) - civil_days(y1, m1, d1)));
                wb_add(&b, L" \u5929\uff09");
            }
        }
    }
    return buf;
}

int sched_anniv_days(const Schedule *s, int *pFuture)
{
    time_t t, now;
    int sign;
    if (!s) return 0;
    t = zs_systime_to_t(&s->start);
    now = time(NULL);
    if (pFuture) *pFuture = 0;
    sign = sched_anniv_elapsed(s, NULL, NULL, NULL);
    if (t > now) {
        int d = -sign < 0 ? sign : -sign;
        (void)d;
        if (pFuture) *pFuture = 1;
        /* 还没到：用两个日期相减 */
        {
            struct tm *pt = localtime(&now);
            int y, m, dd, sy, sm, sd;
            if (!pt) return 0;
            y = pt->tm_year + 1900; m = pt->tm_mon + 1; dd = pt->tm_mday;
            sy = s->start.wYear; sm = s->start.wMonth; sd = s->start.wDay;
            return (int)(civil_days(sy, sm, sd) - civil_days(y, m, dd));
        }
    }
    if (pFuture) *pFuture = 0;
    return sign;
}

int sched_anniv_elapsed(const Schedule *s, int *years, int *months, int *days)
{
    SYSTEMTIME now;
    int y, m, d, tot;
    long long a, b;

    if (years) *years = 0;
    if (months) *months = 0;
    if (days) *days = 0;
    if (!s) return 0;

    GetLocalTime(&now);
    y = now.wYear - s->start.wYear;
    m = now.wMonth - s->start.wMonth;
    d = now.wDay - s->start.wDay;
    if (d < 0) {
        m--;
        d += days_in_month((now.wMonth == 1) ? now.wYear - 1 : now.wYear,
                           (now.wMonth == 1) ? 12 : now.wMonth - 1);
    }
    if (m < 0) { y--; m += 12; }
    if (y < 0) { y = 0; m = 0; d = 0; }

    if (years) *years = y;
    if (months) *months = m;
    if (days) *days = d;

    a = civil_days(s->start.wYear, s->start.wMonth, s->start.wDay);
    b = civil_days(now.wYear, now.wMonth, now.wDay);
    tot = (int)(b - a);
    return tot > 0 ? tot : 0;
}

/* 下一个周年时刻；roundNo 给出这是第几周年（1 起）。无年度重复时返回 0 */
time_t sched_anniv_next_round(const Schedule *s, int *roundNo)
{
    SYSTEMTIME now;
    int elapsed, n, y, m, d, dim;

    if (roundNo) *roundNo = 0;
    if (!s || s->kind != KIND_ANNIV) return 0;
    if (s->repKind != REP_CUSTOM || s->repUnit != UNIT_YEAR) return 0;

    GetLocalTime(&now);
    elapsed = now.wYear - s->start.wYear;
    n = elapsed + 1;
    if (n < 1) n = 1;
    y = s->start.wYear + n;
    m = s->start.wMonth;
    d = s->start.wDay;
    dim = days_in_month(y, m);
    if (d > dim) d = dim;
    if (roundNo) *roundNo = n;
    return zs_time_from_ymd(y, m, d, s->start.wHour, s->start.wMinute, s->start.wSecond);
}

static SchedList g_store;
static wchar_t   g_path[MAX_PATH];
static int       g_nextId = 1;
static int       g_legacy = 0;      /* 载入的是旧格式（颜色字段不可信），需要回写 */

/* 数据文件格式版本
 *   v1: color= 字段为“每通道取后两位十进制”的历史写法，信息有损，已废弃
 *   v2: color=RRGGBB（标准 16 进制），当前格式
 */
#define DATA_VERSION 4

void model_init(void)
{
    memset(&g_store, 0, sizeof(g_store));
    g_nextId = 1;
    g_path[0] = 0;
}

void model_free(void)
{
    if (g_store.items) free(g_store.items);
    memset(&g_store, 0, sizeof(g_store));
}

int model_count(void) { return g_store.count; }

Schedule *model_at(int idx)
{
    if (idx < 0 || idx >= g_store.count) return NULL;
    return &g_store.items[idx];
}

Schedule *model_by_id(int id)
{
    int i;
    for (i = 0; i < g_store.count; i++)
        if (g_store.items[i].id == id) return &g_store.items[i];
    return NULL;
}

int model_index_by_id(int id)
{
    int i;
    for (i = 0; i < g_store.count; i++)
        if (g_store.items[i].id == id) return i;
    return -1;
}

static int ensure_cap(void)
{
    int nc;
    Schedule *np;
    if (g_store.count < g_store.cap) return 1;
    nc = g_store.cap ? g_store.cap * 2 : 16;
    np = (Schedule *)realloc(g_store.items, (size_t)nc * sizeof(Schedule));
    if (!np) return 0;
    g_store.items = np;
    g_store.cap = nc;
    return 1;
}

void model_normalize(Schedule *s)
{
    if (!s) return;
    s->title[MAX_TITLE - 1] = 0;
    s->note[MAX_NOTE - 1] = 0;
    s->noteRtf[MAX_RTF - 1] = 0;
    s->link[MAX_LINK - 1] = 0;
    /* 有富文本时，摘要一律从富文本重新提取，保证两者一致 */
    if (s->noteRtf[0] && wcsncmp(s->noteRtf, L"{\\rtf", 5) == 0)
        rtf_to_plain(s->noteRtf, s->note, MAX_NOTE);
    if (s->repKind < REP_NONE || s->repKind > REP_CUSTOM) s->repKind = REP_NONE;
    if (s->repInterval < 1) s->repInterval = 1;
    if (s->repInterval > 999) s->repInterval = 999;
    if (s->repUnit < UNIT_DAY || s->repUnit > UNIT_YEAR) s->repUnit = UNIT_DAY;
    s->weekMask &= 0x7F;
    if (s->repKind == REP_WEEKLY && s->weekMask == 0) {
        int wd = weekday_of_day(civil_days(s->start.wYear, s->start.wMonth, s->start.wDay));
        s->weekMask = 1 << wd;
    }
    if (s->attachCount < 0) s->attachCount = 0;
    if (s->attachCount > MAX_ATTACH) s->attachCount = MAX_ATTACH;
    s->enabled = s->enabled ? 1 : 0;
    s->done = s->done ? 1 : 0;
    if (s->kind != KIND_ANNIV && s->kind != KIND_TODO) s->kind = KIND_TASK;
    if (s->kind == KIND_ANNIV) s->done = 0;   /* 纪念日始终是“仍在记录”的状态 */
    /* 无时间待办：不删用户填的时间，只是运行时不参与排程（取消待办后能原样回来） */
    s->showImg = s->showImg ? 1 : 0;
    s->timetable = (s->timetable && s->kind == KIND_TASK) ? 1 : 0;
    s->ttNext = s->ttNext ? 1 : 0;
    s->ttNext = s->ttNext ? 1 : 0;   /* 只有普通日程能当课表 */
    if (s->cdUnit < CDU_DEF || s->cdUnit > CDU_SEC) s->cdUnit = CDU_DEF;
    s->remind &= (REM_POPUP | REM_TOAST | REM_SOUND);
    if (s->remLead < 0) s->remLead = 0;
    if (s->remLead > 1440) s->remLead = 1440;
    if (s->remRepeat < 0) s->remRepeat = 0;
    if (s->remRepeat > 1440) s->remRepeat = 0;
    if (s->remTimes < 1) s->remTimes = 1;
    if (s->remTimes > 99) s->remTimes = 99;
    /* 提示音来源：默认系统提示音；自选文件为空时退回系统提示音（老数据没这两行，行为不变） */
    s->sndFile[MAX_PATH - 1] = 0;
    s->sndMode = (s->sndMode == SND_FILE) ? SND_FILE : SND_SYS;
    if (s->sndMode == SND_FILE && !s->sndFile[0]) s->sndMode = SND_SYS;
    if (s->imgN < 0) s->imgN = 0;
    if (s->imgN > MAX_IMGS) s->imgN = MAX_IMGS;
    if (s->imgMode != IMGM_INLINE && s->imgMode != IMGM_END) s->imgMode = IMGM_FLOAT;
    s->imgLayer = (s->imgLayer == IMGL_UNDER) ? IMGL_UNDER : IMGL_OVER;
    {   /* 旧版共用框（imgX/Y/W/H）并入第一张 */
        int k;
        if (s->imgW > 0 && s->imgH > 0 && s->imgB[0][2] <= 0) {
            s->imgB[0][0] = s->imgX;
            s->imgB[0][1] = s->imgY;
            s->imgB[0][2] = s->imgW;
            s->imgB[0][3] = s->imgH;
        }
        for (k = 0; k < MAX_IMGS; k++) {
            if (s->imgB[k][2] < 60 || s->imgB[k][2] > 1000 ||
                s->imgB[k][3] < 60 || s->imgB[k][3] > 1000) {
                s->imgB[k][0] = s->imgB[k][1] = s->imgB[k][2] = s->imgB[k][3] = 0;
            }
        }
    }
    if (s->imgPct < 25) s->imgPct = s->imgPct ? 25 : 0;
    if (s->imgPct > 100) s->imgPct = 100;
    /* 自定义图片框：千分比，超界就当自动 */
    if (s->imgX < 0) s->imgX = 0;
    if (s->imgY < 0) s->imgY = 0;
    if (s->imgX > 1000) s->imgX = 1000;
    if (s->imgY > 1000) s->imgY = 1000;
    if (s->imgW < 60 || s->imgW > 1000) s->imgW = 0;
    if (s->imgH < 60 || s->imgH > 1000) s->imgH = 0;
    if (!s->imgW || !s->imgH) { s->imgW = 0; s->imgH = 0; s->imgX = 0; s->imgY = 0; }
    s->hasEnd = s->hasEnd ? 1 : 0;
    if (!s->color) s->color = RGB(47, 111, 235);

    if (s->hasEnd) {
        time_t a = zs_systime_to_t(&s->start), b = zs_systime_to_t(&s->end);
        s->durSec = (b > a) ? (b - a) : 0;
        if (b <= a) s->hasEnd = 0;
    } else {
        s->durSec = 0;
    }
}

/* 第一张图片附件（不是图片就跳过），没有则返回 NULL */
const wchar_t *sched_first_image(const Schedule *s, wchar_t *buf, size_t cch)
{
    int i;
    if (!s || !buf || cch == 0) return NULL;
    for (i = 0; i < s->attachCount && i < MAX_ATTACH; i++) {
        if (s->attach[i][0] && img_is_image(s->attach[i])) {
            wcsncpy(buf, s->attach[i], cch - 1);
            buf[cch - 1] = 0;
            return buf;
        }
    }
    return NULL;
}

int sched_image_count(const Schedule *s)
{
    int i, n = 0;
    if (!s) return 0;
    for (i = 0; i < s->attachCount && i < MAX_ATTACH; i++)
        if (s->attach[i][0] && img_is_image(s->attach[i])) n++;
    return n;
}

const wchar_t *sched_image_at(const Schedule *s, int k, wchar_t *buf, size_t cch)
{
    int i, n = 0;
    if (!s || !buf || cch == 0 || k < 0) return NULL;
    for (i = 0; i < s->attachCount && i < MAX_ATTACH; i++) {
        if (s->attach[i][0] && img_is_image(s->attach[i])) {
            if (n == k) {
                wcsncpy(buf, s->attach[i], cch - 1);
                buf[cch - 1] = 0;
                return buf;
            }
            n++;
        }
    }
    return NULL;
}

Schedule *model_add(const Schedule *src)
{
    Schedule *dst;
    if (!ensure_cap()) return NULL;
    dst = &g_store.items[g_store.count];
    memset(dst, 0, sizeof(*dst));
    if (src) *dst = *src;
    if (dst->id <= 0) dst->id = g_nextId;
    if (dst->id >= g_nextId) g_nextId = dst->id + 1;
    g_store.count++;
    model_normalize(dst);
    dst->nextOcc = sched_next_occurrence(dst, time(NULL));
    return dst;
}

Schedule *model_dup(int id)
{
    Schedule *src = model_by_id(id);
    Schedule tmp;
    if (!src) return NULL;
    tmp = *src;
    tmp.id = 0;
    tmp.title[0] = 0;
    {
        wchar_t t[MAX_TITLE];
        WBuf b;
        wb_init(&b, t, MAX_TITLE);
        wb_add(&b, src->title);
        wb_add(&b, L"（副本）");
        memcpy(tmp.title, t, sizeof(t));
    }
    return model_add(&tmp);
}

void model_remove(int id)
{
    int i = model_index_by_id(id);
    if (i < 0) return;
    if (i + 1 < g_store.count) {
        memmove(&g_store.items[i], &g_store.items[i + 1],
                (size_t)(g_store.count - i - 1) * sizeof(Schedule));
    }
    g_store.count--;
}

void model_refresh_next(void)
{
    time_t now = time(NULL);
    int i;
    for (i = 0; i < g_store.count; i++) {
        Schedule *s = &g_store.items[i];
        s->nextOcc = sched_next_occurrence(s, now);
        if (s->hasEnd) {
            time_t a = zs_systime_to_t(&s->start), b = zs_systime_to_t(&s->end);
            s->durSec = (b > a) ? (b - a) : 0;
        }
    }
}

/* =======================================================================
 *  六、持久化（UTF-16LE 文本，带 BOM，纯手工解析，不依赖 CRT 的宽字符 IO）
 * =====================================================================*/
const wchar_t *model_path(void) { return g_path; }

void model_set_path(const wchar_t *path)
{
    size_t n;
    if (!path) { g_path[0] = 0; return; }
    n = wcslen(path);
    if (n >= MAX_PATH) n = MAX_PATH - 1;
    memcpy(g_path, path, n * sizeof(wchar_t));
    g_path[n] = 0;
}

#define MODEL_HIST_MAX   60
#define MODEL_HIST_GAP   120

static wchar_t g_histDir[MAX_PATH];
static wchar_t g_histSrc[MAX_PATH];   /* g_histDir 是按哪个数据文件算出来的 */
static DWORD   g_histLastTick;

static void hist_init(void)
{
    const wchar_t *p = model_path();
    const wchar_t *slash = NULL, *q;
    size_t n;
    if (!p || !p[0]) return;
    if (g_histDir[0] && wcscmp(g_histSrc, p) == 0) return;   /* 路径没变就沿用 */
    for (q = p; *q; q++)
        if (*q == L'\\' || *q == L'/') slash = q;
    n = slash ? (size_t)(slash - p + 1) : 0;
    if (n > MAX_PATH - 40) n = MAX_PATH - 40;
    memcpy(g_histDir, p, n * sizeof(wchar_t));
    g_histDir[n] = 0;
    wcscat(g_histDir, DATA_HIST_NAME);
    wcsncpy(g_histSrc, p, MAX_PATH - 1);
    g_histSrc[MAX_PATH - 1] = 0;
}

const wchar_t *model_history_dir(void)
{
    hist_init();
    return g_histDir;
}

static void hist_trim(void)
{
    /* 文件名是时间戳，排序即年龄：超过上限就删最旧的 */
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH];
    wchar_t list[MODEL_HIST_MAX + 8][64];
    int n = 0, i, j;
    HANDLE h;
    if (!g_histDir[0]) return;
    wcscpy(pat, g_histDir);
    wcscat(pat, L"\\*.txt");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && n < MODEL_HIST_MAX + 8) {
            wcsncpy(list[n], fd.cFileName, 63);
            list[n][63] = 0;
            n++;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (n <= MODEL_HIST_MAX) return;
    for (i = 1; i < n; i++) {           /* 按名字冒泡排序 */
        wchar_t key[64];
        wcscpy(key, list[i]);
        j = i - 1;
        while (j >= 0 && wcscmp(list[j], key) > 0) {
            wcscpy(list[j + 1], list[j]);
            j--;
        }
        wcscpy(list[j + 1], key);
    }
    for (i = 0; i < n - MODEL_HIST_MAX; i++) {
        wchar_t full[MAX_PATH];
        wcscpy(full, g_histDir);
        wcscat(full, L"\\");
        wcscat(full, list[i]);
        DeleteFileW(full);
    }
}

int model_archive(const wchar_t *why)
{
    wchar_t dst[MAX_PATH], name[80];
    SYSTEMTIME st;
    HANDLE hs, hd;
    DWORD rd = 0, wr = 0, size;
    BYTE *buf;
    int ok = 0;

    (void)why;
    hist_init();
    if (!g_histDir[0]) return 0;
    hs = CreateFileW(model_path(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hs == INVALID_HANDLE_VALUE) return 0;
    size = GetFileSize(hs, NULL);
    if (size == 0 || size > 64u * 1024 * 1024) { CloseHandle(hs); return 0; }
    CreateDirectoryW(g_histDir, NULL);
    GetLocalTime(&st);
    wsprintfW(name, L"\\zsched_%04d%02d%02d_%02d%02d%02d.txt",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    wcscpy(dst, g_histDir);
    wcscat(dst, name);
    buf = (BYTE *)malloc(size);
    if (!buf) { CloseHandle(hs); return 0; }
    if (ReadFile(hs, buf, size, &rd, NULL) && rd > 0) {
        hd = CreateFileW(dst, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
        if (hd != INVALID_HANDLE_VALUE) {
            ok = WriteFile(hd, buf, rd, &wr, NULL) ? 1 : 0;
            CloseHandle(hd);
        }
    }
    free(buf);
    CloseHandle(hs);
    if (ok) {
        g_histLastTick = GetTickCount();
        hist_trim();
    }
    return ok;
}

int model_history_count(void)
{
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH];
    HANDLE h;
    int n = 0;
    hist_init();
    if (!g_histDir[0]) return 0;
    wcscpy(pat, g_histDir);
    wcscat(pat, L"\\*.txt");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) n++;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}

int model_history_latest(wchar_t *out, size_t cch)
{
    WIN32_FIND_DATAW fd;
    wchar_t pat[MAX_PATH], best[64];
    HANDLE h;
    int found = 0;
    hist_init();
    if (!g_histDir[0] || !out || cch == 0) return 0;
    wcscpy(pat, g_histDir);
    wcscat(pat, L"\\*.txt");
    h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (!found || wcscmp(fd.cFileName, best) > 0) {
            wcsncpy(best, fd.cFileName, 63);
            best[63] = 0;
            found = 1;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!found) return 0;
    wcsncpy(out, g_histDir, cch - 1);
    out[cch - 1] = 0;
    if (wcslen(out) + 1 + wcslen(best) >= cch) return 0;
    wcscat(out, L"\\");
    wcscat(out, best);
    return 1;
}

int model_history_restore_latest(void)
{
    wchar_t src[MAX_PATH];
    HANDLE hs, hd;
    DWORD rd = 0, wr = 0, size;
    BYTE *buf;
    int ok = 0;
    if (!model_history_latest(src, MAX_PATH)) return 0;
    model_archive(L"before-restore");            /* 先把当前版本也存一份 */
    hs = CreateFileW(src, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL, NULL);
    if (hs == INVALID_HANDLE_VALUE) return 0;
    size = GetFileSize(hs, NULL);
    if (size == 0 || size > 64u * 1024 * 1024) { CloseHandle(hs); return 0; }
    buf = (BYTE *)malloc(size);
    if (!buf) { CloseHandle(hs); return 0; }
    if (ReadFile(hs, buf, size, &rd, NULL) && rd > 0) {
        hd = CreateFileW(model_path(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
        if (hd != INVALID_HANDLE_VALUE) {
            ok = WriteFile(hd, buf, rd, &wr, NULL) ? 1 : 0;
            CloseHandle(hd);
        }
    }
    free(buf);
    CloseHandle(hs);
    return ok;
}

int model_save(const wchar_t *path)
{
    {   /* 写盘前把旧版本存一份（至少间隔 2 分钟） */
        DWORD now = GetTickCount();
        if (g_histLastTick == 0 || now - g_histLastTick > MODEL_HIST_GAP * 1000)
            model_archive(L"auto");
    }
    WStr out;
    int i, k;
    if (!path || !path[0]) return 0;
    ws_init(&out);
    ws_add(&out, L"ZScheduleData/4\r\n");
    for (i = 0; i < g_store.count; i++) {
        const Schedule *s = &g_store.items[i];
        ws_add(&out, L"[item]\r\n");
        ws_add(&out, L"id=");      ws_int(&out, s->id);               ws_add(&out, L"\r\n");
        ws_add(&out, L"title=");   zs_esc_append(&out, s->title);        ws_add(&out, L"\r\n");
        ws_add(&out, L"note=");    zs_esc_append(&out, s->note);         ws_add(&out, L"\r\n");
        ws_add(&out, L"rtf=");     zs_esc_append(&out, s->noteRtf);      ws_add(&out, L"\r\n");
        ws_add(&out, L"link=");    zs_esc_append(&out, s->link);         ws_add(&out, L"\r\n");
        {
            wchar_t t[40];
            ws_add(&out, L"start=");
            ws_add(&out, dt_text(&s->start, t, 40));
            ws_add(&out, L"\r\n");
            ws_add(&out, L"end=");
            ws_add(&out, dt_text(&s->end, t, 40));
            ws_add(&out, L"\r\n");
        }
        ws_add(&out, L"hasend=");  ws_int(&out, s->hasEnd);           ws_add(&out, L"\r\n");
        ws_add(&out, L"rep=");     ws_int(&out, s->repKind);          ws_add(&out, L"\r\n");
        ws_add(&out, L"interval=");ws_int(&out, s->repInterval);      ws_add(&out, L"\r\n");
        ws_add(&out, L"unit=");    ws_int(&out, s->repUnit);          ws_add(&out, L"\r\n");
        ws_add(&out, L"week=");    ws_int(&out, s->weekMask);         ws_add(&out, L"\r\n");
        ws_add(&out, L"enabled="); ws_int(&out, s->enabled);          ws_add(&out, L"\r\n");
        ws_add(&out, L"done=");    ws_int(&out, s->done);             ws_add(&out, L"\r\n");
        ws_add(&out, L"kind=");    ws_int(&out, s->kind);             ws_add(&out, L"\r\n");
        ws_add(&out, L"showimg="); ws_int(&out, s->showImg);          ws_add(&out, L"\r\n");
        ws_add(&out, L"imgpct=");  ws_int(&out, s->imgPct);           ws_add(&out, L"\r\n");
        ws_add(&out, L"timetable=");ws_int(&out, s->timetable);       ws_add(&out, L"\r\n");
        ws_add(&out, L"ttnext=");  ws_int(&out, s->ttNext);           ws_add(&out, L"\r\n");
        ws_add(&out, L"cdunit=");  ws_int(&out, s->cdUnit);           ws_add(&out, L"\r\n");
        ws_add(&out, L"remind=");  ws_int(&out, s->remind);           ws_add(&out, L"\r\n");
        ws_add(&out, L"remlead="); ws_int(&out, s->remLead);          ws_add(&out, L"\r\n");
        ws_add(&out, L"remrep=");  ws_int(&out, s->remRepeat);        ws_add(&out, L"\r\n");
        ws_add(&out, L"remtimes=");ws_int(&out, s->remTimes);         ws_add(&out, L"\r\n");
        ws_add(&out, L"sndmode=");  ws_int(&out, s->sndMode);          ws_add(&out, L"\r\n");
        if (s->sndFile[0]) {
            ws_add(&out, L"sndfile="); zs_esc_append(&out, s->sndFile);   ws_add(&out, L"\r\n");
        }
        ws_add(&out, L"imgn=");    ws_int(&out, s->imgN);             ws_add(&out, L"\r\n");
        ws_add(&out, L"imgmode=");  ws_int(&out, s->imgMode);          ws_add(&out, L"\r\n");
        ws_add(&out, L"imglayer="); ws_int(&out, s->imgLayer);         ws_add(&out, L"\r\n");
        {   /* 每张图片各自的框 */
            int kk;
            for (kk = 0; kk < MAX_IMGS; kk++) {
                wchar_t kb[16];
                WBuf bb;
                if (s->imgB[kk][2] <= 0 || s->imgB[kk][3] <= 0) continue;
                wb_init(&bb, kb, 16);
                wb_add(&bb, L"imgb");
                wb_int(&bb, kk);
                wb_add(&bb, L"=");
                ws_add(&out, kb);
                ws_int(&out, s->imgB[kk][0]); ws_add(&out, L",");
                ws_int(&out, s->imgB[kk][1]); ws_add(&out, L",");
                ws_int(&out, s->imgB[kk][2]); ws_add(&out, L",");
                ws_int(&out, s->imgB[kk][3]); ws_add(&out, L"\r\n");
            }
        }
        ws_add(&out, L"imgx=");    ws_int(&out, s->imgX);             ws_add(&out, L"\r\n");
        ws_add(&out, L"imgy=");    ws_int(&out, s->imgY);             ws_add(&out, L"\r\n");
        ws_add(&out, L"imgw=");    ws_int(&out, s->imgW);             ws_add(&out, L"\r\n");
        ws_add(&out, L"imgh=");    ws_int(&out, s->imgH);             ws_add(&out, L"\r\n");
        ws_add(&out, L"color=");
        {
            static const wchar_t *hexd = L"0123456789ABCDEF";
            wchar_t t[8];
            int n = 0, ci;
            BYTE chv[3];
            chv[0] = GetRValue(s->color);
            chv[1] = GetGValue(s->color);
            chv[2] = GetBValue(s->color);
            for (ci = 0; ci < 3; ci++) {
                t[n++] = hexd[(chv[ci] >> 4) & 0x0F];
                t[n++] = hexd[chv[ci] & 0x0F];
            }
            t[n] = 0;
            ws_add(&out, t);
        }
        ws_add(&out, L"\r\n");
        for (k = 0; k < s->attachCount; k++) {
            ws_add(&out, L"attach=");
            zs_esc_append(&out, s->attach[k]);
            ws_add(&out, L"\r\n");
        }
        ws_add(&out, L"[/item]\r\n");
    }
    if (!out.p) { ws_free(&out); return 0; }
    {
        int ok = zs_write_all(path, out.p, out.len);
        ws_free(&out);
        return ok;
    }
}

static int parse_hex2(const wchar_t *p)
{
    int v = 0, i;
    for (i = 0; i < 2 && p[i]; i++) {
        wchar_t c = p[i];
        v *= 16;
        if (c >= L'0' && c <= L'9') v += c - L'0';
        else if (c >= L'a' && c <= L'f') v += c - L'a' + 10;
        else if (c >= L'A' && c <= L'F') v += c - L'A' + 10;
    }
    return v;
}

int model_loaded_legacy(void) { return g_legacy; }

int model_load(const wchar_t *path)
{
    wchar_t *buf;
    size_t len = 0, pos = 0;
    Schedule cur;
    int inItem = 0, got = 0, ver = 1;

    if (!path) return 0;
    buf = zs_read_all(path, &len);
    if (!buf) return 0;

    /* 清空现有数据 */
    g_store.count = 0;
    g_nextId = 1;
    g_legacy = 0;

    memset(&cur, 0, sizeof(cur));

    while (pos <= len) {
        wchar_t *line = buf + pos;
        wchar_t *nl = wcschr(line, L'\n');
        size_t llen;
        if (nl) { *nl = 0; pos = (size_t)(nl - buf) + 1; }
        else    { pos = len + 1; }
        llen = wcslen(line);
        if (llen && line[llen - 1] == L'\r') line[llen - 1] = 0;
        /* 去首尾空白 */
        while (*line == L' ' || *line == L'\t') line++;
        if (!*line) continue;

        if (!wcsncmp(line, L"ZScheduleData/", 14)) {
            int v = (int)wcstol(line + 14, NULL, 10);
            ver = (v >= 1 && v <= 99) ? v : 1;
            continue;
        }

        if (!wcscmp(line, L"[item]")) {
            memset(&cur, 0, sizeof(cur));
            cur.enabled = 1;
            cur.repInterval = 1;
            cur.ttNext = 1;                    /* 旧数据没有这个开关时按“开”算 */
            cur.color = RGB(47, 111, 235);
            GetLocalTime(&cur.start);
            cur.start.wHour = 9; cur.start.wMinute = 0; cur.start.wSecond = 0;
            cur.end = cur.start;
            inItem = 1;
            continue;
        }
        if (!wcscmp(line, L"[/item]")) {
            if (inItem) {
                Schedule *added = model_add(&cur);
                if (added && ver < 2) {
                    /* v1 的颜色字段用了有损写法，已被污染，按调色板重新分配 */
                    added->color = g_palette[got % 8];
                }
                got++;
            }
            inItem = 0;
            continue;
        }
        if (!inItem) continue;
        {
            wchar_t *eq = wcschr(line, L'=');
            wchar_t *k, *v;
            if (!eq) continue;
            *eq = 0;
            k = line;
            v = eq + 1;
            if (!wcscmp(k, L"id"))            cur.id = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"title"))    zs_unesc_into(cur.title, MAX_TITLE, v);
            else if (!wcscmp(k, L"note"))     zs_unesc_into(cur.note, MAX_NOTE, v);
            else if (!wcscmp(k, L"rtf"))      zs_unesc_into(cur.noteRtf, MAX_RTF, v);
            else if (!wcscmp(k, L"link"))     zs_unesc_into(cur.link, MAX_LINK, v);
            else if (!wcscmp(k, L"start")) {
                int y = 0, mo = 1, d = 1, hh = 0, mi = 0, ss = 0;
                if (swscanf(v, L"%d-%d-%d %d:%d:%d", &y, &mo, &d, &hh, &mi, &ss) == 6) {
                    cur.start.wYear = (WORD)y;   cur.start.wMonth = (WORD)mo;
                    cur.start.wDay = (WORD)d;    cur.start.wHour = (WORD)hh;
                    cur.start.wMinute = (WORD)mi;cur.start.wSecond = (WORD)ss;
                    cur.start.wMilliseconds = 0;
                }
            }
            else if (!wcscmp(k, L"end")) {
                int y = 0, mo = 1, d = 1, hh = 0, mi = 0, ss = 0;
                if (swscanf(v, L"%d-%d-%d %d:%d:%d", &y, &mo, &d, &hh, &mi, &ss) == 6) {
                    cur.end.wYear = (WORD)y;   cur.end.wMonth = (WORD)mo;
                    cur.end.wDay = (WORD)d;    cur.end.wHour = (WORD)hh;
                    cur.end.wMinute = (WORD)mi;cur.end.wSecond = (WORD)ss;
                    cur.end.wMilliseconds = 0;
                }
            }
            else if (!wcscmp(k, L"hasend"))   cur.hasEnd = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"rep"))      cur.repKind = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"interval")) cur.repInterval = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"unit"))     cur.repUnit = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"week"))     cur.weekMask = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"enabled"))  cur.enabled = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"done"))     cur.done = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"kind"))     cur.kind = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"showimg"))  cur.showImg = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgpct"))   cur.imgPct = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"timetable")) cur.timetable = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"ttnext"))   cur.ttNext = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"cdunit"))   cur.cdUnit = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"remind"))   cur.remind = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"remlead"))  cur.remLead = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"remrep"))   cur.remRepeat = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"remtimes")) cur.remTimes = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"sndmode"))  cur.sndMode = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"sndfile"))  zs_unesc_into(cur.sndFile, MAX_PATH, v);
            else if (!wcscmp(k, L"imgn"))     cur.imgN = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgmode"))  cur.imgMode = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imglayer")) cur.imgLayer = (int)wcstol(v, NULL, 10);
            else if (!wcsncmp(k, L"imgb", 4) && k[4] >= L'0' && k[4] <= L'9') {
                int bi = k[4] - L'0';
                if (bi >= 0 && bi < MAX_IMGS) {
                    const wchar_t *q = v;
                    int kk;
                    for (kk = 0; kk < 4 && *q; kk++) {
                        cur.imgB[bi][kk] = (int)wcstol(q, NULL, 10);
                        while (*q && *q != L',') q++;
                        if (*q == L',') q++;
                    }
                }
            }
            else if (!wcscmp(k, L"imgx"))     cur.imgX = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgy"))     cur.imgY = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgw"))     cur.imgW = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"imgh"))     cur.imgH = (int)wcstol(v, NULL, 10);
            else if (!wcscmp(k, L"color")) {
                if (wcslen(v) >= 6)
                    cur.color = RGB(parse_hex2(v), parse_hex2(v + 2), parse_hex2(v + 4));
            }
            else if (!wcscmp(k, L"attach")) {
                if (cur.attachCount < MAX_ATTACH)
                    zs_unesc_into(cur.attach[cur.attachCount++], MAX_PATH, v);
            }
        }
    }
    free(buf);
    model_refresh_next();
    g_legacy = (ver < DATA_VERSION) ? 1 : 0;
    return got;
}

/* 生成演示用富文本：第一段常规，第二段加粗放大（非 ASCII 用 \uNNNN? 转义） */
static void rtf_esc_append(WStr *out, const wchar_t *s)
{
    for (; *s; s++) {
        wchar_t c = *s;
        if (c == L'\\' || c == L'{' || c == L'}') { ws_ch(out, L'\\'); ws_ch(out, c); }
        else if (c == L'\n') ws_add(out, L"\\par ");
        else if (c < 128)    ws_ch(out, c);
        else { ws_add(out, L"\\u"); ws_int(out, (short)c); ws_ch(out, L'?'); }
    }
}

static void make_demo_rtf(WStr *out, const wchar_t *p1, const wchar_t *p2,
                          int pt1, int pt2)
{
    ws_add(out, L"{\\rtf1\\ansi\\ansicpg936\\deff0{\\fonttbl{\\f0\\fnil\\fcharset134 Microsoft YaHei UI;}}\n");
    ws_add(out, L"\\viewkind4\\uc1\\pard\\f0\\fs");
    ws_int(out, pt1 * 2);
    ws_ch(out, L' ');
    rtf_esc_append(out, p1);
    ws_add(out, L"\\par\\b\\fs");
    ws_int(out, pt2 * 2);
    ws_ch(out, L' ');
    rtf_esc_append(out, p2);
    ws_add(out, L"\\b0\\fs");
    ws_int(out, pt1 * 2);
    ws_add(out, L"\\par}");
}

/* 首次运行的演示数据 */
void model_seed_sample(void)
{
    Schedule s;
    SYSTEMTIME st;
    GetLocalTime(&st);
    st.wMinute = 0; st.wSecond = 0; st.wMilliseconds = 0;

    /* 1. 一次性：3 天后的预约 */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(47, 111, 235);
    s.repKind = REP_NONE;
    s.repInterval = 1;
    GetLocalTime(&s.start);
    {
        time_t t = time(NULL) + 3 * 86400;
        struct tm *pt = localtime(&t);
        if (pt) {
            s.start.wYear = (WORD)(pt->tm_year + 1900);
            s.start.wMonth = (WORD)(pt->tm_mon + 1);
            s.start.wDay = (WORD)pt->tm_mday;
        }
    }
    s.start.wHour = 14; s.start.wMinute = 30; s.start.wSecond = 0;
    s.start.wMilliseconds = 0;
    s.end = s.start; s.end.wHour = 15; s.end.wMinute = 30;
    s.hasEnd = 1;
    wcscpy(s.title, L"体检预约 · 市第一医院");
    wcscpy(s.note, L"带上身份证和医保卡，提前 15 分钟到前台取号。\n"
                   L"检查前一天晚上 22:00 后禁食。");
    {
        WStr r;
        ws_init(&r);
        make_demo_rtf(&r,
                      L"带上身份证和医保卡，提前 15 分钟到前台取号。",
                      L"检查前一天晚上 22:00 后禁食。", 10, 14);
        if (r.p) {
            wcsncpy(s.noteRtf, r.p, MAX_RTF - 1);
            s.noteRtf[MAX_RTF - 1] = 0;
        }
        ws_free(&r);
    }
    wcscpy(s.link, L"https://www.example.com/appointment");
    model_add(&s);

    /* 2. 每周多选：周一 / 周三 / 周五 */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(16, 152, 96);
    s.repKind = REP_WEEKLY;
    s.repInterval = 1;
    s.weekMask = (1 << 1) | (1 << 3) | (1 << 5);
    s.start = st;
    s.start.wHour = 9; s.start.wMinute = 30;
    s.end = s.start; s.end.wHour = 10; s.end.wMinute = 0;
    s.hasEnd = 1;
    wcscpy(s.title, L"晨会 · 项目进度同步");
    wcscpy(s.note, L"每人 2 分钟，同步昨天进展、今天计划、遇到的风险。");
    model_add(&s);

    /* 3. 自定义周期：每 3 天 */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(232, 125, 30);
    s.repKind = REP_CUSTOM;
    s.repInterval = 3;
    s.repUnit = UNIT_DAY;
    s.start = st;
    s.start.wHour = 20; s.start.wMinute = 0;
    wcscpy(s.title, L"健身房 · 力量训练");
    wcscpy(s.note, L"每 3 天一次，避免连续两天训练同一肌群。");
    model_add(&s);

    /* 4. 每月：交房租 */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(214, 45, 79);
    s.repKind = REP_CUSTOM;
    s.repInterval = 1;
    s.repUnit = UNIT_MONTH;
    s.start = st;
    s.start.wDay = 1;
    s.start.wHour = 9; s.start.wMinute = 0;
    wcscpy(s.title, L"交房租");
    wcscpy(s.note, L"每月 1 号，转账后截图存到云盘。");
    model_add(&s);

    /* 5. 纪念日：记录已经过去的事（每年还会过一次周年） */
    memset(&s, 0, sizeof(s));
    s.enabled = 1;
    s.color = RGB(150, 62, 54);
    s.kind = KIND_ANNIV;
    s.repKind = REP_CUSTOM;
    s.repInterval = 1;
    s.repUnit = UNIT_YEAR;
    GetLocalTime(&s.start);
    s.start.wYear = (WORD)(s.start.wYear - 3);
    s.start.wMonth = 5;
    s.start.wDay = 20;
    s.start.wHour = 0; s.start.wMinute = 0; s.start.wSecond = 0;
    wcscpy(s.title, L"入职纪念日");
    wcscpy(s.note, L"那天第一次走进这间办公室，桌上只有一台旧显示器。");
    model_add(&s);

    model_refresh_next();
}

/* ======================================================================
 *  \u6570\u636e\u4fdd\u5b58\u4f4d\u7f6e\uff1a\u7edf\u4e00\u76ee\u5f55 + \u81ea\u5b9a\u4e49\u8def\u5f84 + \u65e7\u6570\u636e\u5bfc\u5165
 *
 *  \u2460 \u4ee5\u524d\u4e09\u4efd\u72b6\u6001\u6587\u4ef6\uff08\u4e3b\u6570\u636e / \u4fbf\u7b3a\u72b6\u6001 / \u4fbf\u7b3a\u5899\u5e03\u5c40\uff09\u90fd\u8ddf\u7740 exe \u8d70\uff0cexe \u4e00\u6362\u4f4d\u7f6e\u6570\u636e\u5c31\u201c\u4e22\u201d\u4e86\uff1b
 *     \u73b0\u5728\u53ea\u8ba4\u4e00\u4e2a\u201c\u6570\u636e\u76ee\u5f55\u201d\uff08`HKCU\Software\Jian\DataDir`\uff09\uff0c\u9ed8\u8ba4\u4ecd\u662f exe \u6240\u5728\u76ee\u5f55\u3002
 *  \u2461 \u56db\u4efd\u4e1c\u897f\u540c\u8fdb\u540c\u51fa\uff1a`zsched_data.txt`\u3001`zsched_notes.txt`\u3001`zsched_board.txt`\u3001`zsched_history\\`\u3002
 *  \u2462 \u65e7\u6570\u636e\u53ef\u4ee5\u81ea\u52a8\u641c\u5230\uff08exe \u76ee\u5f55 / \u684c\u9762 / \u6587\u6863 / \u4e0b\u8f7d / %APPDATA%\Jian\uff09\u5e76\u5207\u8fc7\u53bb\uff0c
 *     \u5207\u6362\u524d\u4e00\u5b9a\u5148\u628a\u5f53\u524d\u6570\u636e\u5b58\u4e00\u4efd\u5230\u5386\u53f2\u91cc\uff0c\u4e14\u4e0d\u4f1a\u8986\u76d6\u4efb\u4f55\u5df2\u6709\u6587\u4ef6\u3002
 * =====================================================================*/
static const wchar_t *kAppSetKey = L"Software\\Jian";
static const wchar_t *kAppSetDir = L"DataDir";
static wchar_t g_dataDir[MAX_PATH];

/* exe 所在目录（带尾 \\） */
static int count_token(const wchar_t *path, const wchar_t *token)
{
    size_t len = 0;
    wchar_t *buf = zs_read_all(path, &len);
    wchar_t *p;
    int n = 0;
    size_t tl;
    if (!buf) return 0;
    tl = wcslen(token);
    for (p = buf; (p = wcsstr(p, token)) != NULL; p += tl) n++;
    free(buf);
    return n;
}

static void setting_dir_get(wchar_t *out, size_t cch)
{
    HKEY k;
    DWORD type = 0, cb;
    out[0] = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAppSetKey, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS) return;
    cb = (DWORD)(cch * sizeof(wchar_t));
    if (RegQueryValueExW(k, kAppSetDir, NULL, &type, (BYTE *)out, &cb) != ERROR_SUCCESS ||
        type != REG_SZ) {
        out[0] = 0;
    }
    out[cch - 1] = 0;
    RegCloseKey(k);
}

static void setting_dir_set(const wchar_t *dir)
{
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kAppSetKey, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &k, NULL) != ERROR_SUCCESS) return;
    if (dir && dir[0]) {
        RegSetValueExW(k, kAppSetDir, 0, REG_SZ, (const BYTE *)dir,
                       (DWORD)((wcslen(dir) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(k, kAppSetDir);
    }
    RegCloseKey(k);
}

void app_data_init(void)
{
    wchar_t dir[MAX_PATH];
    setting_dir_get(dir, MAX_PATH);
    if (dir[0] && !zs_dir_exists(dir)) dir[0] = 0;      /* \u8bbe\u7f6e\u7684\u76ee\u5f55\u4e0d\u5728\uff08\u6bd4\u5982 U \u76d8\u62d4\u4e86\uff09\u5c31\u56de\u5230 exe \u65c1 */
    if (!dir[0]) zs_exe_dir(dir, MAX_PATH);
    {
        size_t n = wcslen(dir);
        if (n && dir[n - 1] != L'\\' && dir[n - 1] != L'/') wcsncat(dir, L"\\", MAX_PATH - n - 1);
    }
    wcsncpy(g_dataDir, dir, MAX_PATH - 1);
    g_dataDir[MAX_PATH - 1] = 0;
}


/* v2.0 首次启动迁移备份：把现有数据/便笺状态/便笺墙状态复制到
   zsched_history\v2_backup_YYYYMMDD_HHMMSS\，写一个完成标记；只做一次，不覆盖原文件。 */
void zs_migration_backup_once(const wchar_t *dataFile)
{
    wchar_t dir[MAX_PATH], hist[MAX_PATH], mark[MAX_PATH], dst[MAX_PATH], src[MAX_PATH];
    wchar_t name[64];
    SYSTEMTIME st;
    WBuf b;
    int copied = 0;
    static const wchar_t *files[3] = { DATA_FILE_NAME, DATA_NOTES_NAME, DATA_BOARD_NAME };
    int i;

    if (!dataFile || !dataFile[0]) return;
    zs_dir_of_path(dataFile, dir, MAX_PATH);
    if (!dir[0]) return;
    zs_path_join(hist, MAX_PATH, dir, DATA_HIST_NAME);
    zs_path_join(mark, MAX_PATH, hist, L"v2_backup.done");
    if (zs_file_exists(mark)) return;
    if (!zs_dir_exists(hist)) CreateDirectoryW(hist, NULL);

    GetLocalTime(&st);
    wb_init(&b, name, 64);
    wb_add(&b, L"v2_backup_");
    wb_int(&b, st.wYear); wb_u2(&b, st.wMonth); wb_u2(&b, st.wDay);
    wb_ch(&b, L'_');
    wb_u2(&b, st.wHour); wb_u2(&b, st.wMinute); wb_u2(&b, st.wSecond);
    zs_path_join(dst, MAX_PATH, hist, name);
    if (!CreateDirectoryW(dst, NULL) && !zs_dir_exists(dst)) return;

    for (i = 0; i < 3; i++) {
        wchar_t out[MAX_PATH];
        zs_path_join(src, MAX_PATH, dir, files[i]);
        if (!zs_file_exists(src)) continue;
        zs_path_join(out, MAX_PATH, dst, files[i]);
        if (zs_copy_file(src, out)) copied = 1;
    }
    if (copied) {
        HANDLE h = CreateFileW(mark, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD wr = 0;
            const char *msg = "v2 migration backup done";
            WriteFile(h, msg, (DWORD)strlen(msg), &wr, NULL);
            CloseHandle(h);
        }
    }
}

const wchar_t *app_data_dir(void)
{
    if (!g_dataDir[0]) app_data_init();
    return g_dataDir;
}

/* \u628a\u5f53\u524d\u56db\u4efd\u4e1c\u897f\u590d\u5236\u5230 dstDir\uff08\u4e0d\u5220\u4e0d\u79fb\uff09 */
int app_data_copy_to(const wchar_t *dstDir)
{
    wchar_t srcFile[MAX_PATH], dstFile[MAX_PATH], srcHist[MAX_PATH], dstHist[MAX_PATH];
    int ok = 0;
    if (!dstDir || !dstDir[0]) return 0;
    CreateDirectoryW(dstDir, NULL);
    srcFile[0] = 0;
    if (model_path()[0]) wcsncpy(srcFile, model_path(), MAX_PATH - 1);
    else zs_path_join(srcFile, MAX_PATH, app_data_dir(), DATA_FILE_NAME);
    if (srcFile[0]) {
        zs_dir_of_path(srcFile, srcHist, MAX_PATH);
        zs_path_join(dstFile, MAX_PATH, dstDir, DATA_FILE_NAME);
        if (zs_copy_file(srcFile, dstFile)) ok = 1;
    }
    {
        wchar_t s[MAX_PATH], d[MAX_PATH], histDir[MAX_PATH];
        const wchar_t *old = srcFile[0] ? srcHist : NULL;
        if (old) {
            zs_path_join(s, MAX_PATH, old, DATA_NOTES_NAME);
            zs_path_join(d, MAX_PATH, dstDir, DATA_NOTES_NAME);
            zs_copy_file(s, d);
            zs_path_join(s, MAX_PATH, old, DATA_BOARD_NAME);
            zs_path_join(d, MAX_PATH, dstDir, DATA_BOARD_NAME);
            zs_copy_file(s, d);
            /* 历史目录：注意源路径要用 histDir（以前错用了数据目录 srcHist，
               结果历史文件永远找不到、历史目录拷过去是空的） */
            zs_path_join(histDir, MAX_PATH, old, DATA_HIST_NAME);
            zs_path_join(dstHist, MAX_PATH, dstDir, DATA_HIST_NAME);
            CreateDirectoryW(dstHist, NULL);
            {   /* \u5386\u53f2\u76ee\u5f55\u91cc\u7684 *.txt \u9010\u4e2a\u590d\u5236 */
                WIN32_FIND_DATAW fd;
                wchar_t pat[MAX_PATH];
                HANDLE h;
                zs_path_join(pat, MAX_PATH, histDir, L"*.txt");
                h = FindFirstFileW(pat, &fd);
                if (h != INVALID_HANDLE_VALUE) {
                    do {
                        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                        zs_path_join(s, MAX_PATH, histDir, fd.cFileName);
                        zs_path_join(d, MAX_PATH, dstHist, fd.cFileName);
                        if (!zs_file_exists(d)) zs_copy_file(s, d);
                    } while (FindNextFileW(h, &fd));
                    FindClose(h);
                }
            }
        }
    }
    return ok;
}

/* \u6362\u6570\u636e\u76ee\u5f55\uff1bmigrate=1 \u65f6\u5148\u628a\u73b0\u6709\u6587\u4ef6\u590d\u5236\u8fc7\u53bb */
int app_data_dir_set(const wchar_t *dir, int migrate)
{
    wchar_t d[MAX_PATH];
    size_t n;
    if (!dir || !dir[0]) return 0;
    wcsncpy(d, dir, MAX_PATH - 1);
    d[MAX_PATH - 1] = 0;
    n = wcslen(d);
    if (n && d[n - 1] != L'\\' && d[n - 1] != L'/') wcsncat(d, L"\\", MAX_PATH - n - 1);
    if (!zs_dir_exists(d)) {
        if (!CreateDirectoryW(d, NULL)) return 0;
    }
    if (migrate) app_data_copy_to(d);
    setting_dir_set(d);
    wcsncpy(g_dataDir, d, MAX_PATH - 1);
    g_dataDir[MAX_PATH - 1] = 0;
    return 1;
}

void app_data_reload(void)
{
    wchar_t f[MAX_PATH];
    zs_path_join(f, MAX_PATH, app_data_dir(), DATA_FILE_NAME);
    model_free();                     /* 先清空当前数据，别和新目录里的叠在一起 */
    model_init();
    model_set_path(f);
    if (!model_load(f)) {
        model_seed_sample();
        model_save(f);
    } else if (model_loaded_legacy()) {
        model_save(f);
    }
    model_refresh_next();
}

/* \u627e\u672c\u673a\u5df2\u6709\u7684\u6570\u636e\uff1a\u770b\u8fd9\u51e0\u4e2a\u76ee\u5f55\u91cc\u6709\u6ca1\u6709 zsched_data.txt\uff0c\u6309\u4fee\u6539\u65f6\u95f4\u65b0\u2192\u65e7\u6392 */
int app_data_find(AppDataCand *out, int max)
{
    wchar_t cands[10][MAX_PATH];
    int nc = 0, i, n = 0;
    wchar_t *env;

#define ADD_DIR(p)                                                            \
    do {                                                                      \
        if ((p)[0]) {                                                         \
            int k, dup = 0;                                                   \
            for (k = 0; k < nc; k++)                                          \
                if (_wcsicmp(cands[k], (p)) == 0) { dup = 1; break; }          \
            if (!dup && nc < 10) { wcsncpy(cands[nc], (p), MAX_PATH - 1);     \
                                   cands[nc][MAX_PATH - 1] = 0; nc++; }       \
        }                                                                     \
    } while (0)

    ADD_DIR(app_data_dir());
    {   /* exe \u65c1 */
        wchar_t d[MAX_PATH];
        zs_exe_dir(d, MAX_PATH);
        ADD_DIR(d);
    }
    {   /* \u684c\u9762 / \u6587\u6863 / \u4e0b\u8f7d / \u4e00\u9a71\u684c\u9762 */
        wchar_t d[MAX_PATH];
        env = _wgetenv(L"USERPROFILE");
        if (env && env[0]) {
            zs_path_join(d, MAX_PATH, env, L"Desktop");        ADD_DIR(d);
            zs_path_join(d, MAX_PATH, env, L"Documents");      ADD_DIR(d);
            zs_path_join(d, MAX_PATH, env, L"Downloads");      ADD_DIR(d);
            zs_path_join(d, MAX_PATH, env, L"OneDrive\\Desktop");   ADD_DIR(d);
            zs_path_join(d, MAX_PATH, env, L"OneDrive\\Documents"); ADD_DIR(d);
        }
        env = _wgetenv(L"APPDATA");
        if (env && env[0]) { zs_path_join(d, MAX_PATH, env, L"Jian"); ADD_DIR(d); }
        env = _wgetenv(L"LOCALAPPDATA");
        if (env && env[0]) { zs_path_join(d, MAX_PATH, env, L"Jian"); ADD_DIR(d); }
    }
#undef ADD_DIR

    for (i = 0; i < nc && n < max; i++) {
        wchar_t f[MAX_PATH], nf[MAX_PATH];
        WIN32_FIND_DATAW fd;
        HANDLE h;
        zs_path_join(f, MAX_PATH, cands[i], DATA_FILE_NAME);
        if (!zs_file_exists(f)) continue;
        memset(&out[n], 0, sizeof(out[n]));
        wcsncpy(out[n].dir, cands[i], MAX_PATH - 1);
        wcsncpy(out[n].dataFile, f, MAX_PATH - 1);
        out[n].items = count_token(f, L"[item]");
        zs_path_join(nf, MAX_PATH, cands[i], DATA_NOTES_NAME);
        if (zs_file_exists(nf)) {
            out[n].hasNotes = 1;
            out[n].notes = count_token(nf, L"id=");
        }
        h = FindFirstFileW(f, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            out[n].mtime = ((long long)fd.ftLastWriteTime.dwHighDateTime << 32) |
                           fd.ftLastWriteTime.dwLowDateTime;
            FindClose(h);
        }
        n++;
    }
    {   /* \u6309\u4fee\u6539\u65f6\u95f4\u4ece\u65b0\u5230\u65e7\u6392\u5e8f\uff08\u63d2\u5165\u6392\u5e8f\uff0c\u6700\u591a 10 \u4efd\uff09 */
        int a, b;
        for (a = 1; a < n; a++) {
            AppDataCand key = out[a];
            b = a - 1;
            while (b >= 0 && out[b].mtime < key.mtime) { out[b + 1] = out[b]; b--; }
            out[b + 1] = key;
        }
    }
    return n;
}

/* \u5148\u628a\u5f53\u524d\u6570\u636e\u5b58\u4e00\u4efd\u5230\u5386\u53f2\uff0c\u518d\u5207\u5230\u5019\u9009\u76ee\u5f55\uff08\u4e0d\u590d\u5236\u3001\u4e0d\u8986\u76d6\u4efb\u4f55\u6587\u4ef6\uff09 */
int app_data_import(const AppDataCand *c)
{
    if (!c || !c->dir[0]) return 0;
    model_archive(L"import");                 /* \u5f53\u524d\u6570\u636e\u5148\u5b58\u6863\uff0c\u4e0d\u4f1a\u4e22 */
    if (!app_data_dir_set(c->dir, 0)) return 0;
    app_data_reload();
    return 1;
}

