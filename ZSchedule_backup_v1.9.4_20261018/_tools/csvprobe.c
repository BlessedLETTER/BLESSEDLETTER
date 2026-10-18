/* 临时探针：直接 #include csvimport.c，调用里面的 static 函数做离线验证。
   编译（在工程目录，见 _tools/_probe_build.bat）：
     gcc -O1 -Wall -Wextra -DUNICODE -D_UNICODE -ffunction-sections -fdata-sections
         -Wl,--gc-sections -o _tools\csvprobe.exe _tools\csvprobe.c model.c <libs>
   输出写到 _tools\csvprobe_out.txt（UTF-8 with BOM）。 */
#include <stdio.h>
#include <stdarg.h>
#include "../csvimport.c"

/* 探针不需要真的开便笺/刷新界面：给链接器补几个空实现 */
HINSTANCE g_hInst;
const COLORREF g_palette[8] = {
    RGB(59, 111, 224), RGB(22, 163, 106), RGB(235, 148, 40), RGB(219, 74, 74),
    RGB(124, 92, 224), RGB(13, 148, 168), RGB(214, 51, 132), RGB(112, 122, 136)
};
COLORREF palette_color(int idx) { return g_palette[idx < 0 || idx > 7 ? 0 : idx]; }
void main_set_status(const wchar_t *msg) { (void)msg; }
void main_refresh_all(void) { }
int  note_count(void) { return 0; }
int  note_list(int *ids, int max) { (void)ids; (void)max; return 0; }
void note_get_look(int id, int *style, int *colorIndex)
{ (void)id; if (style) *style = NOTE_STYLE_PAPER; if (colorIndex) *colorIndex = 0; }
void note_open_batch(const int *ids, int count, int style, int colorIndex)
{ (void)ids; (void)count; (void)style; (void)colorIndex; }
int  img_is_image(const wchar_t *path) { (void)path; return 0; }

static FILE *g_out;

static void P(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_out, fmt, ap);
    va_end(ap);
    fputc('\n', g_out);
}

/* 宽字符 → UTF-8（写文件用） */
static const char *u8(const wchar_t *s, char *buf, int cch)
{
    if (!s) s = L"";
    WideCharToMultiByte(CP_UTF8, 0, s, -1, buf, cch, NULL, NULL);
    buf[cch - 1] = 0;
    return buf;
}

static void re_dump(const wchar_t *tag, const wchar_t *pat, int icase, const wchar_t *subj)
{
    ZRe re;
    ZExec ex;
    char b1[128], b2[128];
    if (!zre_compile(&re, pat, icase)) { P("[RE] %ls  COMPILE FAIL", tag); return; }
    if (zre_find(&re, subj, 0, &ex)) {
        wchar_t g1[64], g2[64];
        const wchar_t *p1 = re_gstr(&ex, subj, 1, g1, 64);
        const wchar_t *p2 = re_gstr(&ex, subj, 2, g2, 64);
        P("[RE] %ls  hit[%d,%d) g1=%s g2=%s", tag, ex.gs[0], ex.ge[0],
          u8(p1 ? p1 : L"-", b1, 128), u8(p2 ? p2 : L"-", b2, 128));
    } else {
        P("[RE] %ls  no match", tag);
    }
}

static void test_regex(void)
{
    ZRe re;
    ZExec ex;
    int i;
    char b[512];

    P("=== 1. 正则引擎 ===");
    re_dump(L"周几", L"(?:星期|周|礼拜)\\s*([一二三四五六日天1-7])", 1, L"周一 08:00-09:40");
    re_dump(L"星期五", L"(?:星期|周|礼拜)\\s*([一二三四五六日天1-7])", 1, L"星期五 10:00");
    re_dump(L"周次不误伤", L"(?:星期|周|礼拜)\\s*([一二三四五六日天1-7])", 1, L"1-16周(单)");
    re_dump(L"英文星期",
            L"(?:星期|周|礼拜)\\s*([一二三四五六日天1-7])|(mon|tue|wed|thu|fri|sat|sun)", 1, L"TUE 8:00");
    re_dump(L"时刻", L"([0-9]{1,2})\\s*[:.]\\s*([0-9]{1,2})", 0, L"08:00-09:40");
    re_dump(L"节次", L"第?\\s*([0-9]{1,2})\\s*(?:[-~至到,、]\\s*([0-9]{1,2}))?\\s*节", 0, L"第3-4节");
    re_dump(L"节次2", L"第?\\s*([0-9]{1,2})\\s*(?:[-~至到,、]\\s*([0-9]{1,2}))?\\s*节", 0, L"1、2节");
    re_dump(L"日期", L"([0-9]{4})\\s*[-/.年]\\s*([0-9]{1,2})\\s*[-/.月]\\s*([0-9]{1,2})", 0, L"2026-10-16");
    re_dump(L"整数字符串", L"^[0-9]+$", 0, L"12345");
    re_dump(L"整数字符串-反例", L"^[0-9]+$", 0, L"12a45");

    if (zre_compile(&re, L"[0-9]+5", 0)) {
        int ok = zre_find(&re, L"12345", 0, &ex);
        P("[RE] 量词回溯 ([0-9]+5 on 12345): %s [%d,%d)", ok ? "hit" : "MISS", ex.gs[0], ex.ge[0]);
    }
    if (zre_compile(&re, L"(a|ab)c", 0)) {
        int ok = zre_find(&re, L"abc", 0, &ex);
        P("[RE] 分支回溯 ((a|ab)c on abc): %s [%d,%d)", ok ? "hit" : "MISS", ex.gs[0], ex.ge[0]);
    }
    if (zre_compile(&re, L"a[^0-9]c", 0)) {
        int h1 = zre_find(&re, L"abc", 0, &ex);
        int h2 = zre_find(&re, L"a1c", 0, &ex);
        P("[RE] [^0-9]: abc=%d a1c=%d (期望 1 / 0)", h1, h2);
    }
    if (zre_compile(&re, L"^[0-9]{2,4}$", 0)) {
        int a = zre_find(&re, L"12", 0, &ex), b2 = zre_find(&re, L"12345", 0, &ex);
        P("[RE] {2,4}: 12=%d 12345=%d (期望 1 / 0)", a, b2);
    }
    {
        int tm[4][2];
        int n = scan_times(L"08:00-09:40", tm, 4);
        P("[SCAN] times=%d", n);
        for (i = 0; i < n; i++) P("        %d:%02d", tm[i][0], tm[i][1]);
    }
    {
        int mask = 0;
        scan_weekdays(L"周一、周三 08:00", &mask);
        P("[SCAN] 周一、周三 mask=%d (期望 %d)", mask, (1 << 1) | (1 << 3));
    }
    {
        wchar_t *cn = cn_time_rewrite(L"8点30分到10点");
        P("[SCAN] 中文时刻改写 = %s", u8(cn ? cn : L"(null)", b, 512));
        free(cn);
    }
    {
        int p1 = 0, p2 = 0;
        int ok = scan_period(L"第3-4节", &p1, &p2);
        P("[SCAN] 节次 第3-4节 → ok=%d p1=%d p2=%d", ok, p1, p2);
        ok = scan_period(L"1、2节", &p1, &p2);
        P("[SCAN] 节次 1、2节 → ok=%d p1=%d p2=%d", ok, p1, p2);
    }
}

static void dump_items(const char *tag)
{
    int i;
    char t[512], b[1024], w[128];
    P("--- %s：识别 %d 节，跳过 %d 行，正文列 %d，模式 %d", tag, g_itemN, g_aux_info.skipped,
      g_aux_info.bodyCols, g_aux_info.mode);
    for (i = 0; i < g_itemN; i++) {
        wchar_t when[64];
        item_when(&g_items[i], when, 64);
        P("  %d) %s  |%s|  起始 %04d-%02d-%02d %02d:%02d 周掩码=%d%s", i + 1,
          u8(g_items[i].title, t, 512), u8(when, w, 128),
          g_items[i].start.wYear, g_items[i].start.wMonth, g_items[i].start.wDay,
          g_items[i].start.wHour, g_items[i].start.wMinute, g_items[i].weekMask,
          g_items[i].hasEnd ? " 有结束" : "");
        P("     正文: %s", g_items[i].body[0] ? u8(g_items[i].body, b, 1024) : "(空)");
    }
}

static void test_csv(const char *file, const char *tag)
{
    wchar_t wpath[MAX_PATH];
    CsvInfo info;
    int i, k;
    char line[2048];
    MultiByteToWideChar(CP_ACP, 0, file, -1, wpath, MAX_PATH);
    P("");
    P("=== %s ===", tag);
    if (!re_ok()) { P("正则初始化失败"); return; }
    if (!csv_load(wpath)) { P("读取失败"); return; }
    P("行=%d 列=%d 分隔符=%lc", g_rowN, g_colN, g_delim ? g_delim : L'?');
    for (i = 0; i < g_rowN && i < 8; i++) {
        WBuf b;
        wchar_t tmp[2048];
        wb_init(&b, tmp, 2048);
        for (k = 0; k < g_rows[i].n; k++) {
            if (k) wb_add(&b, L" | ");
            wb_add(&b, g_rows[i].f[k] ? g_rows[i].f[k] : L"");
        }
        P("  行%d: %s", i + 1, u8(tmp, line, 2048));
    }
    analyze(&info);
    g_aux_info = info;
    P("表头行=%d 名称列=%d 时间列=%d 星期列=%d 日期列=%d",
      info.headRow, info.colName, info.colTime, info.colWeek, info.colDate);
    dump_items(tag);
    csv_free_all();
    (void)k;
}

int main(void)
{
    g_out = fopen("_tools\\csvprobe_out.txt", "wb");
    if (!g_out) return 1;
    fputs("\xEF\xBB\xBF", g_out);        /* BOM：UTF-8 一看就懂 */
    test_regex();
    test_csv("_tools\\t1_table.csv", "① 表格型（表头 + 时刻列）");
    test_csv("_tools\\t2_week_period.csv", "② 表格型（星期列 + 节次列）");
    test_csv("_tools\\t3_grid.csv", "③ 网格型（行=节次 列=星期，单元格多行）");
    test_csv("_tools\\t4_gbk.csv", "④ 表格型（GBK + 分号 + 全角数字）");
    test_csv("_tools\\t5_nohdr.csv", "⑤ 没有表头（按内容推断）");
    test_csv("_tools\\t6_dates.csv", "⑥ 带日期列（一次性课程）");
    fclose(g_out);
    return 0;
}
