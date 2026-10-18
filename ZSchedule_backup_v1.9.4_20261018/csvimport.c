/* ============================================================================
 *  ZSchedule - csvimport.c
 *  CSV 课表导入：识别文件格式 → 只取“便笺支持的”信息（名称 / 时间）→
 *  其余信息统一写进正文 → 逐行生成课表便笺（timetable=1，到点自动开关便笺）。
 *
 *  为什么自带一个正则引擎：本机工具链（w64devkit）没有 <regex.h>，而
 *  “从表头里认名称列 / 从单元格里认星期·时刻·节次”用正则最稳。
 *  下面是够用的子集：
 *    字面量   .   [abc] [^a-z] [\d]   \d \D \w \W \s \S \. \\ 等转义
 *    *  +  ?  {m} {m,} {m,n}（贪婪）   ( ) 捕获（最多 9 组）   (?: ) 非捕获
 *    | 分支   ^ $ 锚点   可选忽略大小写
 *  不支持反向引用与前后顾——识别课表用不到。分组内是“整体贪婪”的：
 *  组内部不会再为组后面的内容让字符（写模式时避免 `(\w+)x` 这种写法即可）。
 *
 *  识别两种常见格式：
 *    ① 表格型：一行一节课，表头里有“课程名称 / 上课时间 / 星期 / 节次”等列；
 *    ② 网格型：学期课表那种「行 = 节次、列 = 星期」，格子里是课程名（可带教师/教室）。
 * ==========================================================================*/
#include "zsched.h"

/* ======================================================================
 *  1. 极简正则引擎
 * ====================================================================*/
#define ZRE_MAX_NODES  640
#define ZRE_MAX_CLS    24
#define ZRE_MAX_GROUPS 9
#define ZRE_STEP_LIMIT 400000

enum { ZN_EMPTY = 0, ZN_LIT, ZN_ANY, ZN_CLASS, ZN_ALT, ZN_GROUP, ZN_REP, ZN_BOL, ZN_EOL };

typedef struct ZNode ZNode;
struct ZNode {
    int      type;
    int      gi;                       /* 捕获组号（-1 = 不捕获） */
    int      min, max;                 /* ZN_REP：次数，max < 0 = 不限 */
    wchar_t  ch;                       /* ZN_LIT */
    wchar_t  clo[ZRE_MAX_CLS], chi[ZRE_MAX_CLS];
    int      clsN, neg;
    ZNode   *child;                    /* GROUP / REP / ALT 的子表达式 */
    ZNode   *next;                     /* 串在后面的结点 */
    ZNode   *alt;                      /* ALT 的另一分支 */
};

typedef struct {
    ZNode nodes[ZRE_MAX_NODES];
    int   n;
    ZNode *root;
    int   ngroups;
    int   icase;
    int   err;
} ZRe;

typedef struct {
    int gs[10], ge[10];                /* 组起止下标（0 = 整个匹配），-1 = 未参与 */
    int steps;
    int icase;
    int last;                           /* 最近一次“走到底”的结束位置 */
} ZExec;

typedef struct {
    const wchar_t *p;
    ZRe *re;
} ZParse;

static ZNode *zp_new(ZParse *zp)
{
    ZRe *re = zp->re;
    ZNode *n;
    if (re->n >= ZRE_MAX_NODES) { re->err = 1; return NULL; }
    n = &re->nodes[re->n++];
    memset(n, 0, sizeof(*n));
    n->gi = -1;
    return n;
}

static int cls_add(ZNode *n, wchar_t lo, wchar_t hi)
{
    if (n->clsN >= ZRE_MAX_CLS) return 0;
    n->clo[n->clsN] = lo;
    n->chi[n->clsN] = hi;
    n->clsN++;
    return 1;
}

static void cls_add_word(ZNode *n)
{
    /* \w：ASCII 词字符 + 汉字 */
    cls_add(n, L'0', L'9');
    cls_add(n, L'A', L'Z');
    cls_add(n, L'a', L'z');
    cls_add(n, L'_', L'_');
    cls_add(n, (wchar_t)0x3400, (wchar_t)0x9FFF);
}

static void cls_add_space(ZNode *n)
{
    cls_add(n, L' ', L' ');
    cls_add(n, L'\t', L'\t');
    cls_add(n, L'\r', L'\r');
    cls_add(n, L'\n', L'\n');
}

static ZNode *parse_alt(ZParse *zp);

static ZNode *parse_class(ZParse *zp)
{
    ZNode *n = zp_new(zp);
    if (!n) return NULL;
    n->type = ZN_CLASS;
    zp->p++;                                    /* [ */
    if (*zp->p == L'^') { n->neg = 1; zp->p++; }
    while (*zp->p && *zp->p != L']') {
        wchar_t lo, hi;
        if (*zp->p == L'\\') {
            wchar_t e;
            zp->p++;
            e = *zp->p++;
            if (e == L'd') { cls_add(n, L'0', L'9'); continue; }
            if (e == L'D') { n->neg = 1; cls_add(n, L'0', L'9'); continue; }
            if (e == L'w') { cls_add_word(n); continue; }
            if (e == L's') { cls_add_space(n); continue; }
            if (!e) { zp->re->err = 1; return NULL; }
            lo = e;
        } else {
            lo = *zp->p++;
        }
        hi = lo;
        if (*zp->p == L'-' && zp->p[1] && zp->p[1] != L']') {
            zp->p++;
            if (*zp->p == L'\\' && zp->p[1]) zp->p++;
            hi = *zp->p++;
        }
        if (!cls_add(n, lo, hi)) { zp->re->err = 1; return NULL; }
    }
    if (*zp->p != L']') { zp->re->err = 1; return NULL; }
    zp->p++;
    return n;
}

static ZNode *parse_atom(ZParse *zp)
{
    wchar_t c = *zp->p;
    ZNode *n;
    if (c == L'(') {
        int cap = 1;
        zp->p++;
        if (zp->p[0] == L'?' && zp->p[1] == L':') { cap = 0; zp->p += 2; }
        n = zp_new(zp);
        if (!n) return NULL;
        n->type = ZN_GROUP;
        if (cap) {
            if (zp->re->ngroups >= ZRE_MAX_GROUPS) { zp->re->err = 1; return NULL; }
            zp->re->ngroups++;
            n->gi = zp->re->ngroups;
        }
        n->child = parse_alt(zp);
        if (zp->re->err) return NULL;
        if (*zp->p != L')') { zp->re->err = 1; return NULL; }
        zp->p++;
        return n;
    }
    if (c == L'[') return parse_class(zp);
    if (c == L'.' || c == L'^' || c == L'$') {
        zp->p++;
        n = zp_new(zp);
        if (!n) return NULL;
        n->type = (c == L'.') ? ZN_ANY : (c == L'^' ? ZN_BOL : ZN_EOL);
        return n;
    }
    if (c == L'\\') {
        zp->p++;
        c = *zp->p;
        if (!c) { zp->re->err = 1; return NULL; }
        zp->p++;
        n = zp_new(zp);
        if (!n) return NULL;
        switch (c) {
        case L'd': n->type = ZN_CLASS; cls_add(n, L'0', L'9'); return n;
        case L'D': n->type = ZN_CLASS; n->neg = 1; cls_add(n, L'0', L'9'); return n;
        case L'w': n->type = ZN_CLASS; cls_add_word(n); return n;
        case L'W': n->type = ZN_CLASS; n->neg = 1; cls_add_word(n); return n;
        case L's': n->type = ZN_CLASS; cls_add_space(n); return n;
        case L'S': n->type = ZN_CLASS; n->neg = 1; cls_add_space(n); return n;
        case L'n': n->type = ZN_LIT; n->ch = L'\n'; return n;
        case L't': n->type = ZN_LIT; n->ch = L'\t'; return n;
        default:   n->type = ZN_LIT; n->ch = c;    return n;   /* \. \\ \+ \? 等 */
        }
    }
    if (!c || c == L')' || c == L'|' || c == L'*' || c == L'+' || c == L'?' ||
        c == L'{' || c == L'}') {
        zp->re->err = 1;
        return NULL;
    }
    zp->p++;
    n = zp_new(zp);
    if (!n) return NULL;
    n->type = ZN_LIT;
    n->ch = c;
    return n;
}

static ZNode *parse_rep(ZParse *zp)
{
    ZNode *a = parse_atom(zp);
    int lo = -1, hi = -1;
    if (!a) return NULL;
    {
        wchar_t c = *zp->p;
        if (c == L'*') { lo = 0; hi = -1; zp->p++; }
        else if (c == L'+') { lo = 1; hi = -1; zp->p++; }
        else if (c == L'?') { lo = 0; hi = 1; zp->p++; }
        else if (c == L'{') {
            const wchar_t *q = zp->p + 1;
            if (*q >= L'0' && *q <= L'9') {
                int a1 = 0, a2 = -1, ok = 0;
                while (*q >= L'0' && *q <= L'9') { a1 = a1 * 10 + (int)(*q - L'0'); q++; }
                if (*q == L',') {
                    q++;
                    if (*q >= L'0' && *q <= L'9') {
                        a2 = 0;
                        while (*q >= L'0' && *q <= L'9') { a2 = a2 * 10 + (int)(*q - L'0'); q++; }
                    }
                } else {
                    a2 = a1;
                }
                if (*q == L'}') { ok = 1; zp->p = q + 1; }
                if (ok) { lo = a1; hi = a2; }
            }
        }
    }
    if (lo >= 0) {
        ZNode *r = zp_new(zp);
        if (!r) return NULL;
        r->type = ZN_REP;
        r->child = a;
        r->min = lo;
        r->max = hi;
        return r;
    }
    return a;
}

static int zp_end(ZParse *zp)
{
    wchar_t c = *zp->p;
    return !c || c == L')' || c == L'|';
}

static ZNode *parse_seq(ZParse *zp)
{
    ZNode *head = NULL, *tail = NULL;
    while (!zp_end(zp)) {
        ZNode *a = parse_rep(zp);
        if (zp->re->err) return NULL;
        if (!a) break;
        if (!head) head = a;
        else       tail->next = a;
        tail = a;
    }
    if (!head) {
        head = zp_new(zp);
        if (!head) return NULL;
        head->type = ZN_EMPTY;
    }
    return head;
}

static ZNode *parse_alt(ZParse *zp)
{
    ZNode *first = parse_seq(zp);
    if (zp->re->err) return NULL;
    if (*zp->p != L'|') return first;
    {
        ZNode *alt = zp_new(zp);
        if (!alt) return NULL;
        alt->type = ZN_ALT;
        alt->child = first;
        zp->p++;                                   /* | */
        alt->alt = parse_alt(zp);
        if (zp->re->err) return NULL;
        return alt;
    }
}

static int zre_compile(ZRe *re, const wchar_t *pat, int icase)
{
    ZParse zp;
    memset(re, 0, sizeof(*re));
    re->icase = icase;
    zp.p = pat;
    zp.re = re;
    re->root = parse_alt(&zp);
    if (re->err || !re->root || *zp.p != 0) {
        re->err = 1;
        re->root = NULL;
        return 0;
    }
    return 1;
}

static int zch_eq(wchar_t a, wchar_t b, int icase)
{
    if (a == b) return 1;
    if (icase) {
        if (a >= L'A' && a <= L'Z') a = (wchar_t)(a - L'A' + L'a');
        if (b >= L'A' && b <= L'Z') b = (wchar_t)(b - L'A' + L'b');
        if (a == b) return 1;
    }
    return 0;
}

static int cls_hit(const ZNode *n, wchar_t c)
{
    int i, hit = 0;
    for (i = 0; i < n->clsN; i++)
        if (c >= n->clo[i] && c <= n->chi[i]) { hit = 1; break; }
    return n->neg ? !hit : hit;
}

/* ---- 匹配器：用“续接（continuation）”实现，组/分支/量词都能正常回退 ----
   一次匹配 = 匹配一条链（结点由 next 串起来），链走完后接着走续接 k。 */
typedef struct ZCont ZCont;
struct ZCont {
    int          kind;      /* 0 = 继续匹配一条链；1 = 量词再走一轮；2 = 组结束（记终点） */
    const ZNode *chain;     /* kind 0：要匹配的链 */
    const ZNode *rep;       /* kind 1：量词结点 */
    int          count;     /* kind 1：已经走了几轮 */
    int          gi;        /* kind 2：组号 */
    const ZCont *next;
};

static int m_chain(const ZNode *n, const ZCont *k, const wchar_t *s, int pos, ZExec *ex);
static int m_rep(const ZNode *rep, const ZCont *k, const wchar_t *s, int pos, int done, ZExec *ex);

static int m_cont(const ZCont *k, const wchar_t *s, int pos, ZExec *ex)
{
    if (!k) {                                 /* 整条模式走完：这就是匹配终点 */
        ex->last = pos;
        return pos;
    }
    if (k->kind == 2) {                       /* 组内链刚走完：到这一格就是组的终点 */
        if (k->gi >= 0 && k->gi <= 9) ex->ge[k->gi] = pos;
        if (!k->chain) return m_cont(k->next, s, pos, ex);
        return m_chain(k->chain, k->next, s, pos, ex);
    }
    if (k->kind == 1) return m_rep(k->rep, k->next, s, pos, k->count, ex);
    if (!k->chain) return m_cont(k->next, s, pos, ex);
    return m_chain(k->chain, k->next, s, pos, ex);
}

/* 量词：贪婪——先尽量多吃，实在不行再收手 */
static int m_rep(const ZNode *rep, const ZCont *k, const wchar_t *s, int pos, int done, ZExec *ex)
{
    if (ex->steps++ > ZRE_STEP_LIMIT) return -1;
    if ((rep->max < 0 || done < rep->max) && done < 200) {
        ZCont more, inner;
        more.kind = 1;
        more.rep = rep;
        more.count = done + 1;
        more.chain = NULL;
        more.gi = -1;
        more.next = k;
        inner.kind = 0;
        inner.chain = rep->child;
        inner.next = &more;
        inner.rep = NULL;
        inner.count = 0;
        inner.gi = -1;
        if (m_cont(&inner, s, pos, ex) >= 0) return ex->last;   /* 见下：直接返回位置 */
    }
    if (done >= rep->min) return m_cont(k, s, pos, ex);
    return -1;
}

static int m_chain(const ZNode *n, const ZCont *k, const wchar_t *s, int pos, ZExec *ex)
{
    if (!n) return m_cont(k, s, pos, ex);
    if (++ex->steps > ZRE_STEP_LIMIT) return -1;
    switch (n->type) {
    case ZN_EMPTY: return m_chain(n->next, k, s, pos, ex);
    case ZN_BOL:   return (pos == 0) ? m_chain(n->next, k, s, pos, ex) : -1;
    case ZN_EOL:   return (s[pos] == 0) ? m_chain(n->next, k, s, pos, ex) : -1;
    case ZN_LIT:
        if (zch_eq(s[pos], n->ch, ex->icase))
            return m_chain(n->next, k, s, pos + 1, ex);
        return -1;
    case ZN_ANY:
        if (s[pos] == 0) return -1;
        return m_chain(n->next, k, s, pos + 1, ex);
    case ZN_CLASS: {
        wchar_t c = s[pos];
        int hit;
        if (!c) return -1;
        hit = cls_hit(n, c);
        if (!hit && ex->icase && c < 128) {
            wchar_t alt = 0;
            if (c >= L'a' && c <= L'z') alt = (wchar_t)(c - 32);
            else if (c >= L'A' && c <= L'Z') alt = (wchar_t)(c + 32);
            if (alt) hit = cls_hit(n, alt);
        }
        return hit ? m_chain(n->next, k, s, pos + 1, ex) : -1;
    }
    case ZN_GROUP: {
        int ss = ex->gs[n->gi], se = ex->ge[n->gi];
        ZCont kend, kin;
        ex->gs[n->gi] = pos;                  /* 起点先记下，终点由 kend 记 */
        kend.kind = 2;
        kend.gi = n->gi;
        kend.chain = n->next;
        kend.rep = NULL;
        kend.count = 0;
        kend.next = k;
        kin.kind = 0;
        kin.chain = n->child;
        kin.next = &kend;
        kin.rep = NULL;
        kin.count = 0;
        kin.gi = -1;
        if (m_cont(&kin, s, pos, ex) >= 0) return ex->last;
        ex->gs[n->gi] = ss;
        ex->ge[n->gi] = se;
        return -1;
    }
    case ZN_ALT: {
        ZCont kb, ka;
        kb.kind = 0; kb.chain = n->next; kb.next = k;
        kb.rep = NULL; kb.count = 0; kb.gi = -1;
        ka.kind = 0; ka.next = &kb; ka.rep = NULL; ka.count = 0; ka.gi = -1;
        ka.chain = n->child;
        if (m_cont(&ka, s, pos, ex) >= 0) return ex->last;
        ka.chain = n->alt;
        if (m_cont(&ka, s, pos, ex) >= 0) return ex->last;
        return -1;
    }
    case ZN_REP: {
        ZCont kb;
        kb.kind = 0; kb.chain = n->next; kb.next = k;
        kb.rep = NULL; kb.count = 0; kb.gi = -1;
        return m_rep(n, &kb, s, pos, 0, ex);
    }
    default:
        return -1;
    }
}

/* 在 subj 的 start 位置之后找第一个匹配 */
static int zre_find(const ZRe *re, const wchar_t *subj, int start, ZExec *ex)
{
    int pos;
    if (!re || re->err || !re->root || !subj) return 0;
    if (start < 0) start = 0;
    ex->icase = re->icase;
    ex->last = -1;
    for (pos = start; ; pos++) {
        int i, e;
        for (i = 0; i < 10; i++) { ex->gs[i] = -1; ex->ge[i] = -1; }
        ex->steps = 0;
        e = m_chain(re->root, NULL, subj, pos, ex);
        if (e >= 0) {
            ex->gs[0] = pos;
            ex->ge[0] = e;
            return 1;
        }
        if (subj[pos] == 0) break;
    }
    return 0;
}

/* 取第 gi 组文本（gi=0 是整个匹配），没参与返回 NULL */
static const wchar_t *re_gstr(const ZExec *ex, const wchar_t *subj, int gi,
                              wchar_t *buf, size_t cch)
{
    int a, b, n;
    if (!ex || gi < 0 || gi > 9 || !buf || cch == 0) return NULL;
    a = ex->gs[gi];
    b = ex->ge[gi];
    if (a < 0 || b < a) return NULL;
    n = b - a;
    if ((size_t)n >= cch) n = (int)cch - 1;
    memcpy(buf, subj + a, (size_t)n * sizeof(wchar_t));
    buf[n] = 0;
    return buf;
}

static int re_gint(const ZExec *ex, const wchar_t *subj, int gi, int defv)
{
    wchar_t b[24];
    if (!re_gstr(ex, subj, gi, b, 24)) return defv;
    return (int)wcstol(b, NULL, 10);
}

/* ======================================================================
 *  2. 识别用的正则表（第一次用到时编译一次）
 * ====================================================================*/
enum {
    RE_H_NAME = 0, RE_H_TIME, RE_H_WEEK, RE_H_DATE, RE_H_NO,
    RE_WEEKDAY, RE_TIME, RE_CNTIME, RE_PERIOD, RE_DATE, RE_INT,
    RE_COUNT
};
static ZRe g_re[RE_COUNT];
static int g_reReady;

static void re_setup(void)
{
    int i;
    if (g_reReady) return;
    g_reReady = 1;
    zre_compile(&g_re[RE_H_NAME], L"课程名称|课程|科目|课名|课目|名称|标题|name|subject|course|title", 1);
    zre_compile(&g_re[RE_H_TIME], L"上课时间|上课|时间|时段|节次|节|学时|period|time|hour", 1);
    zre_compile(&g_re[RE_H_WEEK], L"星期|周次|周几|礼拜|weekday|week", 1);
    zre_compile(&g_re[RE_H_DATE], L"日期|date", 1);
    zre_compile(&g_re[RE_H_NO],   L"序号|行号|编号|index|no", 1);
    /* 星期：支持「周一 / 星期三 / 礼拜日 / 周3」与英文缩写 */
    zre_compile(&g_re[RE_WEEKDAY],
                L"(?:星期|周|礼拜)\\s*([一二三四五六日天1-7])|(mon|tue|wed|thu|fri|sat|sun)", 1);
    /* 时刻：8:00 / 08:00 / 8.00 / 8：00（已全角转半角） */
    zre_compile(&g_re[RE_TIME], L"([0-9]{1,2})\\s*[:.]\\s*([0-9]{1,2})", 0);
    /* 中文写法：8点30 / 8时30分 */
    zre_compile(&g_re[RE_CNTIME], L"([0-9]{1,2})\\s*[点时]\\s*([0-9]{1,2})?", 0);
    /* 节次：第3-4节 / 1、2节 / 第5节 */
    zre_compile(&g_re[RE_PERIOD], L"第?\\s*([0-9]{1,2})\\s*(?:[-~至到,、]\\s*([0-9]{1,2}))?\\s*节", 0);
    zre_compile(&g_re[RE_DATE], L"([0-9]{4})\\s*[-/.年]\\s*([0-9]{1,2})\\s*[-/.月]\\s*([0-9]{1,2})", 0);
    zre_compile(&g_re[RE_INT], L"^[0-9]+$", 0);
    for (i = 0; i < RE_COUNT; i++)
        if (g_re[i].err) { g_reReady = -1; break; }
}

static int re_ok(void)
{
    re_setup();
    return g_reReady == 1;
}

static int re_hit(int id, const wchar_t *subj)
{
    ZExec ex;
    if (!subj || !subj[0]) return 0;
    return zre_find(&g_re[id], subj, 0, &ex);
}

/* ======================================================================
 *  3. 文本小工具
 * ====================================================================*/
static wchar_t *w_dup(const wchar_t *s)
{
    size_t n;
    wchar_t *p;
    if (!s) s = L"";
    n = wcslen(s) + 1;
    p = (wchar_t *)malloc(n * sizeof(wchar_t));
    if (p) memcpy(p, s, n * sizeof(wchar_t));
    return p;
}

static void w_trim(wchar_t *s)
{
    size_t a = 0, b;
    if (!s) return;
    b = wcslen(s);
    while (a < b && (s[a] == L' ' || s[a] == L'\t')) a++;
    while (b > a && (s[b - 1] == L' ' || s[b - 1] == L'\t')) b--;
    memmove(s, s + a, (b - a) * sizeof(wchar_t));
    s[b - a] = 0;
}

static int all_digits(const wchar_t *s)
{
    if (!s || !s[0]) return 0;
    for (; *s; s++)
        if (*s < L'0' || *s > L'9') return 0;
    return 1;
}

static int has_cjk(const wchar_t *s)
{
    for (; s && *s; s++)
        if (*s >= 0x3400 && *s <= 0x9FFF) return 1;
    return 0;
}

/* 全角 → 半角（数字/字母/常见标点），顺手把全角空格变成普通空格 */
static void norm_wide(wchar_t *s)
{
    for (; s && *s; s++) {
        wchar_t c = *s;
        if (c == 0x3000) *s = L' ';
        else if (c >= 0xFF01 && c <= 0xFF5E) *s = (wchar_t)(c - 0xFEE0);
        else if (c == 0x2018 || c == 0x2019) *s = L'\'';
        else if (c == 0x201C || c == 0x201D) *s = L'"';
    }
}

/* ======================================================================
 *  4. 读文件（UTF-8 / UTF-16 / GBK 三种常见编码都认）
 * ====================================================================*/
static wchar_t *read_text_file(const wchar_t *path, size_t *plen)
{
    HANDLE h;
    DWORD sz = 0, rd = 0;
    char *raw;
    wchar_t *w = NULL;
    int wlen = 0;

    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    sz = GetFileSize(h, NULL);
    if (sz == 0 || sz > 8u * 1024 * 1024) { CloseHandle(h); return NULL; }
    raw = (char *)malloc((size_t)sz + 4);
    if (!raw) { CloseHandle(h); return NULL; }
    if (!ReadFile(h, raw, sz, &rd, NULL)) { CloseHandle(h); free(raw); return NULL; }
    CloseHandle(h);
    raw[rd] = 0;
    raw[rd + 1] = 0;

    if (rd >= 2 && (BYTE)raw[0] == 0xFF && (BYTE)raw[1] == 0xFE) {
        /* UTF-16LE + BOM */
        size_t n = (rd - 2) / 2;
        w = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
        if (w) {
            memcpy(w, raw + 2, n * sizeof(wchar_t));
            w[n] = 0;
            wlen = (int)n;
        }
    } else {
        int off = 0, nulEven = 0, i;
        /* 没有 BOM：偶数位全是 00 就当成 UTF-16LE（Windows 记事本“Unicode”写法） */
        for (i = 0; i < (int)rd && i < 512; i += 2)
            if (raw[i] == 0) nulEven++;
        if (nulEven > 8 && nulEven * 4 > (int)rd) {
            rd &= ~1u;
            w = (wchar_t *)malloc(((size_t)rd / 2 + 1) * sizeof(wchar_t));
            if (w) {
                memcpy(w, raw, (size_t)rd);
                wlen = (int)(rd / 2);
                w[wlen] = 0;
            }
        } else {
            UINT cp = CP_UTF8;
            DWORD fl = MB_ERR_INVALID_CHARS;
            if (rd >= 3 && (BYTE)raw[0] == 0xEF && (BYTE)raw[1] == 0xBB && (BYTE)raw[2] == 0xBF)
                off = 3;
            wlen = MultiByteToWideChar(cp, fl, raw + off, (int)rd - off, NULL, 0);
            if (wlen <= 0) {
                /* 不是合法 UTF-8：按 GBK(936) 试，再退回系统 ANSI */
                cp = 936;
                wlen = MultiByteToWideChar(cp, fl, raw + off, (int)rd - off, NULL, 0);
                if (wlen <= 0) {
                    cp = CP_ACP;
                    fl = 0;
                    wlen = MultiByteToWideChar(cp, fl, raw + off, (int)rd - off, NULL, 0);
                }
            }
            if (wlen > 0) {
                w = (wchar_t *)malloc(((size_t)wlen + 1) * sizeof(wchar_t));
                if (w) {
                    MultiByteToWideChar(cp, fl, raw + off, (int)rd - off, w, wlen);
                    w[wlen] = 0;
                }
            }
        }
    }
    free(raw);
    if (w && plen) *plen = (size_t)(wlen > 0 ? wlen : 0);
    return w;
}

/* ======================================================================
 *  5. CSV 表格
 * ====================================================================*/
#define CSV_MAX_ROWS  400
#define CSV_MAX_COLS  48

typedef struct {
    wchar_t *f[CSV_MAX_COLS];      /* 字段文本（malloc） */
    int      n;
} CsvRow;

static CsvRow g_rows[CSV_MAX_ROWS];
static int     g_rowN;
static int     g_colN;
static wchar_t g_delim = L',';
static int     g_capped;                  /* 行数超上限 */
static wchar_t g_path[MAX_PATH];

static void rows_free(void)
{
    int i, k;
    for (i = 0; i < g_rowN; i++)
        for (k = 0; k < g_rows[i].n; k++) {
            free(g_rows[i].f[k]);
            g_rows[i].f[k] = NULL;
        }
    g_rowN = 0;
    g_colN = 0;
    g_capped = 0;
}

/* 逗号 / 分号 / 制表符 / 竖线里，挑出现最多的当分隔符（只看头 5 条记录，引号内不算） */
static void delim_decide(const wchar_t *buf)
{
    static const wchar_t dl[4] = { L',', L';', L'\t', L'|' };
    int cnt[4] = { 0, 0, 0, 0 };
    const wchar_t *p = buf;
    int inQ = 0, recs = 0, k;
    while (*p && recs < 5) {
        wchar_t c = *p;
        if (c == L'"') { inQ = !inQ; p++; continue; }
        if (!inQ && (c == L'\n' || c == L'\r')) {
            while (*p == L'\n' || *p == L'\r') p++;
            recs++;
            continue;
        }
        if (!inQ)
            for (k = 0; k < 4; k++)
                if (c == dl[k]) cnt[k]++;
        p++;
    }
    {
        int best = -1, bestN = 0;
        for (k = 0; k < 4; k++)
            if (cnt[k] > bestN) { bestN = cnt[k]; best = k; }
        g_delim = (best < 0) ? 0 : dl[best];
    }
}

/* 切一行：支持引号包裹、"" 转义 */
static void row_split(const wchar_t *line, wchar_t delim, CsvRow *row)
{
    const wchar_t *p = line;
    row->n = 0;
    for (;;) {
        WStr cell;
        int inQ = 0, done = 0;
        ws_init(&cell);
        while (*p && !done) {
            wchar_t c = *p;
            if (inQ) {
                if (c == L'"') {
                    if (p[1] == L'"') { ws_ch(&cell, L'"'); p += 2; continue; }
                    inQ = 0;
                    p++;
                    continue;
                }
                ws_ch(&cell, c);
                p++;
                continue;
            }
            if (c == L'"') { inQ = 1; p++; continue; }
            if (delim && c == delim) { p++; done = 1; break; }
            ws_ch(&cell, c);
            p++;
        }
        if (row->n < CSV_MAX_COLS) {
            wchar_t *txt;
            if (cell.p) { norm_wide(cell.p); w_trim(cell.p); }
            txt = w_dup(cell.p ? cell.p : L"");
            row->f[row->n++] = txt ? txt : w_dup(L"");
        }
        ws_free(&cell);
        if (!*p && !done) break;
        if (row->n >= CSV_MAX_COLS) break;
    }
    if (row->n > g_colN) g_colN = row->n;
}

/* 读成表格：先定分隔符（看头几行谁的分隔符最多），再逐行切 */
/* 按记录解析：引号里的换行属于同一格（Excel 导出“课程名\n教师\n教室”就是这样） */
static void csv_parse_records(const wchar_t *buf)
{
    WStr rec;
    const wchar_t *p = buf;
    int inQ = 0;
    ws_init(&rec);
    for (;;) {
        wchar_t c = *p;
        if (!c) {
            if (rec.p && rec.len) {
                w_trim(rec.p);
                if (g_rowN < CSV_MAX_ROWS) { row_split(rec.p, g_delim, &g_rows[g_rowN]); g_rowN++; }
                else g_capped = 1;
            }
            break;
        }
        if (c == L'"') { inQ = !inQ; ws_ch(&rec, c); p++; continue; }
        if (!inQ && (c == L'\n' || c == L'\r')) {
            if (rec.p && rec.len) {
                w_trim(rec.p);
                if (g_rowN < CSV_MAX_ROWS) { row_split(rec.p, g_delim, &g_rows[g_rowN]); g_rowN++; }
                else { g_capped = 1; p = p + wcslen(p); break; }
            }
            ws_free(&rec);
            ws_init(&rec);
            while (*p == L'\n' || *p == L'\r') p++;
            continue;
        }
        ws_ch(&rec, c);
        p++;
    }
    ws_free(&rec);
}

static int csv_load(const wchar_t *path)
{
    wchar_t *buf;
    size_t len = 0;

    rows_free();
    buf = read_text_file(path, &len);
    if (!buf) return 0;
    (void)len;
    wcsncpy(g_path, path, MAX_PATH - 1);
    g_path[MAX_PATH - 1] = 0;

    delim_decide(buf);
    csv_parse_records(buf);
    free(buf);
    return g_rowN > 0;
}

/* ======================================================================
 *  6. 课表识别
 * ====================================================================*/
/* 列的分类标志 */
enum { CK_NAME = 1, CK_TIME = 2, CK_WEEK = 4, CK_DATE = 8, CK_NO = 16 };

typedef struct {
    int mode;                      /* 1 = 表格型，2 = 网格型 */
    int headRow;                   /* -1 = 没找到表头 */
    int rows, cols;
    int colName, colTime, colWeek, colDate;
    int isTime[CSV_MAX_COLS];      /* 这一列是“时间类”来源：残值才写正文 */
    int isNo[CSV_MAX_COLS];        /* 序号列（纯数字就丢掉） */
    int bodyCols;                  /* 写进正文的列数 */
    int skipped;                   /* 跳过（无效/重复）的行 */
    int noTime;                    /* 没有时间信息、只能按默认时间导入的行 */
    int periodUsed;                /* 有行用“节次”推时间 */
    int deriveUsed;                /* 有行用默认 08:00 */
    wchar_t headName[MAX_TITLE];
    wchar_t headTime[MAX_TITLE];
} CsvInfo;

typedef struct {
    wchar_t  title[MAX_TITLE];
    wchar_t  body[1600];
    SYSTEMTIME start, end;
    int      hasEnd;
    int      weekly;
    int      weekMask;
    wchar_t  when[64];
    int      derived;              /* 时间是按内置作息表推的 */
} CsvItem;

#define CSV_MAX_ITEMS 300
static CsvItem g_items[CSV_MAX_ITEMS];
static int     g_itemN;
static int     g_dupSkipped;

/* 默认作息表：第 N 节的开始时刻（识别不到具体时刻时用它推） */
static const int g_periodStart[12][2] = {
    { 8, 0 }, { 8, 55 }, { 10, 0 }, { 10, 55 }, { 14, 0 }, { 14, 55 },
    { 16, 0 }, { 16, 55 }, { 19, 0 }, { 19, 55 }, { 20, 45 }, { 21, 40 }
};
#define PERIOD_MIN 45

static int period_start(int p, int *hh, int *mm)
{
    if (p < 1 || p > 12) return 0;
    *hh = g_periodStart[p - 1][0];
    *mm = g_periodStart[p - 1][1];
    return 1;
}

/* 周一..周日 → 位（bit0 = 周日） */
static int weekday_bit(int n)          /* n: 1=周一 … 7=周日 */
{
    if (n < 1 || n > 7) return -1;
    return (n == 7) ? 0 : n;
}

static int cjk_weekday(wchar_t c)
{
    switch (c) {
    case L'一': return 1;
    case L'二': return 2;
    case L'三': return 3;
    case L'四': return 4;
    case L'五': return 5;
    case L'六': return 6;
    case L'日':
    case L'天': return 7;
    default: return 0;
    }
}

/* 找一个字符串里的所有星期（返回命中的“天”位掩码） */
static int scan_weekdays(const wchar_t *s, int *mask)
{
    ZExec ex;
    int start = 0, found = 0;
    while (re_ok() && zre_find(&g_re[RE_WEEKDAY], s, start, &ex)) {
        wchar_t g1[8], g2[8];
        int wd = 0;
        if (re_gstr(&ex, s, 1, g1, 8)) {
            wchar_t c = g1[0];
            if (c >= L'1' && c <= L'7') wd = (int)(c - L'0');
            else wd = cjk_weekday(c);
        } else if (re_gstr(&ex, s, 2, g2, 8)) {
            static const wchar_t *nm[7] = { L"mon", L"tue", L"wed", L"thu", L"fri", L"sat", L"sun" };
            int k;
            for (k = 0; k < 7; k++)
                if (!_wcsicmp(g2, nm[k])) { wd = k + 1; break; }
        }
        if (wd > 0) {
            int b = weekday_bit(wd);
            if (b >= 0) { *mask |= (1 << b); found = 1; }
        }
        start = (ex.ge[0] > ex.gs[0]) ? ex.ge[0] : ex.gs[0] + 1;
    }
    return found;
}

/* 整个值就是个“星期”：一 / 1 / 周一 / 星期二 / 周1（这格的信息已经被时间接管了） */
static int is_weekday_only(const wchar_t *v)
{
    wchar_t tmp[32];
    int mask = 0;
    size_t n;
    if (!v || !v[0] || wcslen(v) > 16) return 0;
    wcsncpy(tmp, v, 31);
    tmp[31] = 0;
    w_trim(tmp);
    n = wcslen(tmp);
    if (n == 1 && cjk_weekday(tmp[0])) return 1;
    if (all_digits(tmp)) {
        int d = (int)wcstol(tmp, NULL, 10);
        return (d >= 1 && d <= 7);
    }
    if (!scan_weekdays(tmp, &mask) || !mask) return 0;
    {
        ZExec ex;
        int start = 0, guard = 0;
        while (re_ok() && zre_find(&g_re[RE_WEEKDAY], tmp, start, &ex) && guard++ < 8) {
            int k;
            for (k = ex.gs[0]; k < ex.ge[0]; k++) tmp[k] = L' ';
            start = (ex.ge[0] > ex.gs[0]) ? ex.ge[0] : ex.gs[0] + 1;
        }
    }
    w_trim(tmp);
    return tmp[0] == 0;
}

/* 字符串里的整点数字：8:00 / 8.00（最多取 4 个） */
static int scan_times(const wchar_t *s, int tm[][2], int maxOut)
{
    ZExec ex;
    int start = 0, n = 0;
    while (n < maxOut && re_ok() && zre_find(&g_re[RE_TIME], s, start, &ex)) {
        int hh = re_gint(&ex, s, 1, -1), mm = re_gint(&ex, s, 2, -1);
        if (hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59) {
            tm[n][0] = hh;
            tm[n][1] = mm;
            n++;
        }
        start = (ex.ge[0] > ex.gs[0]) ? ex.ge[0] : ex.gs[0] + 1;
    }
    return n;
}

/* 把「8点30分」这类中文写法改写成 8:30，返回新串（调用方 free） */
static wchar_t *cn_time_rewrite(const wchar_t *s)
{
    WStr out;
    ZExec ex;
    int start = 0, last = 0;
    ws_init(&out);
    while (re_ok() && zre_find(&g_re[RE_CNTIME], s, start, &ex)) {
        int hh = re_gint(&ex, s, 1, -1), mm = re_gint(&ex, s, 2, -1);
        int i;
        if (hh < 0 || hh > 23) {
            start = (ex.ge[0] > ex.gs[0]) ? ex.ge[0] : ex.gs[0] + 1;
            continue;
        }
        if (mm < 0 || mm > 59) mm = 0;
        for (i = last; i < ex.gs[0]; i++) ws_ch(&out, s[i]);
        {
            wchar_t t[16];
            WBuf b;
            wb_init(&b, t, 16);
            wb_int(&b, hh);
            wb_ch(&b, L':');
            if (mm < 10) wb_ch(&b, L'0');
            wb_int(&b, mm);
            ws_add(&out, t);
        }
        last = ex.ge[0];
        start = (ex.ge[0] > ex.gs[0]) ? ex.ge[0] : ex.gs[0] + 1;
    }
    for (; s[last]; last++) ws_ch(&out, s[last]);
    if (!out.p) ws_add(&out, L"");
    return out.p;
}

/* 节次：返回 1 并给出起止节（p2 = p1 表示单节） */
static int scan_period(const wchar_t *s, int *p1, int *p2)
{
    ZExec ex;
    if (!re_ok() || !zre_find(&g_re[RE_PERIOD], s, 0, &ex)) return 0;
    *p1 = re_gint(&ex, s, 1, 0);
    *p2 = re_gint(&ex, s, 2, *p1);
    if (*p1 < 1 || *p1 > 12) return 0;
    if (*p2 < *p1) *p2 = *p1;
    if (*p2 > 12) *p2 = 12;
    return 1;
}

static int scan_date(const wchar_t *s, int *y, int *mo, int *d)
{
    ZExec ex;
    if (!re_ok() || !zre_find(&g_re[RE_DATE], s, 0, &ex)) return 0;
    *y = re_gint(&ex, s, 1, 0);
    *mo = re_gint(&ex, s, 2, 0);
    *d = re_gint(&ex, s, 3, 0);
    if (*y < 1900 || *mo < 1 || *mo > 12 || *d < 1 || *d > 31) return 0;
    return 1;
}

/* 去掉“时间类”匹配之后，还剩多少有意义的字（用来判断原始列要不要写进正文） */
static int residual_meaning(const wchar_t *s)
{
    size_t n = wcslen(s);
    wchar_t *tmp = (wchar_t *)malloc((n + 1) * sizeof(wchar_t));
    int i, k;
    static const int ids[5] = { RE_WEEKDAY, RE_TIME, RE_CNTIME, RE_PERIOD, RE_DATE };
    static const wchar_t *drop = L" ,，、;；/|()（）[]【】-~—–至到节第周天·.。:：";
    if (!tmp) return 0;
    memcpy(tmp, s, (n + 1) * sizeof(wchar_t));
    for (k = 0; k < 5; k++) {
        ZExec ex;
        int start = 0, guard = 0;
        while (re_ok() && zre_find(&g_re[ids[k]], tmp, start, &ex) && guard++ < 32) {
            int a = ex.gs[0], b = ex.ge[0];
            for (i = a; i < b; i++) tmp[i] = L' ';
            if (b <= a) start = a + 1; else start = b;
        }
    }
    for (i = 0; tmp[i]; i++) {
        wchar_t c = tmp[i];
        if (c == L' ' || c == L'\t') continue;
        if (wcschr(drop, c)) continue;
        break;
    }
    k = (tmp[i] != 0);
    free(tmp);
    return k;
}

/* 表头关键词打分：命中几个关键词列 */
static int head_score(int rowIdx, int *kinds)
{
    int i, score = 0;
    CsvRow *r = &g_rows[rowIdx];
    for (i = 0; i < r->n && i < CSV_MAX_COLS; i++) {
        const wchar_t *h = r->f[i];
        int k = 0;
        if (re_hit(RE_H_NAME, h)) k |= CK_NAME;
        if (re_hit(RE_H_TIME, h)) k |= CK_TIME;
        if (re_hit(RE_H_WEEK, h)) k |= CK_WEEK;
        if (re_hit(RE_H_DATE, h)) k |= CK_DATE;
        if (re_hit(RE_H_NO, h))   k |= CK_NO;
        if (kinds) kinds[i] = k;
        if (k) score++;
    }
    return score;
}

static int row_is_empty(const CsvRow *r)
{
    int i;
    for (i = 0; i < r->n; i++)
        if (r->f[i] && r->f[i][0]) return 0;
    return 1;
}

/* 值像不像“课程名”：有汉字（或较长英文）且不含时间信息 */
static int looks_like_name(const wchar_t *v)
{
    if (!v || !v[0]) return 0;
    if (has_cjk(v)) {
        if (re_hit(RE_WEEKDAY, v) || re_hit(RE_PERIOD, v)) return 0;
        if (re_hit(RE_DATE, v)) return 0;
        {
            ZExec ex;
            if (re_ok() && zre_find(&g_re[RE_TIME], v, 0, &ex)) return 0;
        }
        return 1;
    }
    return 0;
}

/* 一行的“时间池”：除名称列、序号列以外的字段拼在一起 */
static void build_pool(const CsvRow *r, const CsvInfo *info, wchar_t *pool, size_t cch)
{
    int i;
    WBuf b;
    wb_init(&b, pool, cch);
    for (i = 0; i < r->n && i < CSV_MAX_COLS; i++) {
        const wchar_t *v = r->f[i];
        if (!v || !v[0]) continue;
        if (i == info->colName) continue;
        if (info->isNo[i] && all_digits(v)) continue;
        wb_add(&b, v);
        wb_ch(&b, L' ');
    }
}

/* 把“正文”追加一行（带列名） */
static void body_add(wchar_t *body, size_t cch, const wchar_t *label, const wchar_t *val)
{
    size_t cur = wcslen(body);
    if (!val || !val[0]) return;
    if (cur + 1 >= cch) return;
    if (cur) {
        wcsncat(body, L"\n", cch - cur - 1);
        cur = wcslen(body);
    }
    if (label && label[0]) {
        wcsncat(body, label, cch - cur - 1);
        wcsncat(body, L"：", cch - wcslen(body) - 1);
    }
    wcsncat(body, val, cch - wcslen(body) - 1);
}

/* 明天的此刻 = 从今天起第一个命中 weekMask 的日期（含今天） */
static void first_date_of_mask(int mask, SYSTEMTIME *st)
{
    time_t t = time(NULL);
    struct tm *pt = localtime(&t);
    int i;
    GetLocalTime(st);
    if (!pt) return;
    for (i = 0; i < 7; i++) {
        time_t tt = t + (time_t)i * 86400;
        struct tm *q = localtime(&tt);
        long long ord;
        int wd;
        if (!q) break;
        ord = civil_days(q->tm_year + 1900, q->tm_mon + 1, q->tm_mday);
        wd = weekday_of_day(ord);
        if (mask & (1 << wd)) {
            st->wYear = (WORD)(q->tm_year + 1900);
            st->wMonth = (WORD)(q->tm_mon + 1);
            st->wDay = (WORD)q->tm_mday;
            return;
        }
    }
}

/* 一行（表格型）→ 一条课表便笺 */
static int row_to_item(const CsvRow *r, const CsvInfo *info, CsvItem *it)
{
    wchar_t pool[1200], title[MAX_TITLE], body[1600];
    wchar_t *cn = NULL;
    int mask = 0, times[4][2], tn = 0, p1 = 0, p2 = 0, hasPeriod = 0;
    int dy = 0, dmo = 0, dd = 0, hasDate = 0;
    int i, hh = -1, mm = 0, ehh = -1, emm = 0;
    int derived = 0;

    memset(it, 0, sizeof(*it));
    memset(body, 0, sizeof(body));
    title[0] = 0;
    pool[0] = 0;

    /* 名称 */
    if (info->colName >= 0 && info->colName < r->n && r->f[info->colName])
        wcsncpy(title, r->f[info->colName], MAX_TITLE - 1);
    if (!title[0]) {
        /* 没认到名称列：正文池里挑一个最像名称的字段 */
        int bestLen = 0;
        for (i = 0; i < r->n && i < CSV_MAX_COLS; i++) {
            int len;
            if (i == info->colName) continue;
            if (!looks_like_name(r->f[i])) continue;
            len = (int)wcslen(r->f[i]);
            if (len > bestLen) { bestLen = len; wcsncpy(title, r->f[i], MAX_TITLE - 1); }
        }
    }
    title[MAX_TITLE - 1] = 0;
    w_trim(title);
    {   /* 标题里不留换行（多行单元格的锅） */
        wchar_t *q;
        for (q = title; *q; q++)
            if (*q == L'\n' || *q == L'\r') *q = L' ';
    }

    build_pool(r, info, pool, 1200);

    /* 时间信息全部从“时间池”里认 */
    cn = cn_time_rewrite(pool);
    {
        const wchar_t *tp = cn ? cn : pool;
        scan_weekdays(tp, &mask);
        tn = scan_times(tp, times, 4);
        hasPeriod = scan_period(tp, &p1, &p2);
        hasDate = scan_date(tp, &dy, &dmo, &dd);
    }

    /* 名称还是空的：用池子里“去掉时间信息后剩下的话”当名字 */
    if (!title[0] && cn) {
        wchar_t tmp[1200];
        ZExec ex;
        int start = 0, guard = 0;
        wcsncpy(tmp, cn, 1199);
        tmp[1199] = 0;
        while (re_ok() && zre_find(&g_re[RE_WEEKDAY], tmp, start, &ex) && guard++ < 32) {
            int a = ex.gs[0], b = ex.ge[0], k;
            for (k = a; k < b; k++) tmp[k] = L' ';
            start = (b > a) ? b : a + 1;
        }
        {
            int k;
            guard = 0;
            start = 0;
            while (re_ok() && zre_find(&g_re[RE_TIME], tmp, start, &ex) && guard++ < 32) {
                int a = ex.gs[0], b = ex.ge[0];
                for (k = a; k < b; k++) tmp[k] = L' ';
                start = (b > a) ? b : a + 1;
            }
            guard = 0;
            start = 0;
            while (re_ok() && zre_find(&g_re[RE_PERIOD], tmp, start, &ex) && guard++ < 32) {
                int a = ex.gs[0], b = ex.ge[0];
                for (k = a; k < b; k++) tmp[k] = L' ';
                start = (b > a) ? b : a + 1;
            }
        }
        w_trim(tmp);
        if (tmp[0]) wcsncpy(title, tmp, MAX_TITLE - 1);
        title[MAX_TITLE - 1] = 0;
    }
    if (!title[0]) return 0;                       /* 认不出名字就不导入 */

    /* 时间：具体时刻 > 节次 > 默认 */
    if (tn >= 2) {
        hh = times[0][0]; mm = times[0][1];
        ehh = times[1][0]; emm = times[1][1];
    } else if (tn == 1) {
        hh = times[0][0]; mm = times[0][1];
    } else if (hasPeriod) {
        int sh, sm, eh, em;
        if (period_start(p1, &sh, &sm)) {
            hh = sh; mm = sm;
            if (period_start(p2, &eh, &em)) {
                em += PERIOD_MIN;
                if (em >= 60) { em -= 60; eh++; }
                ehh = eh; emm = em;
            }
            derived = 1;
        }
    } else if (mask) {
        hh = 8; mm = 0;
        derived = 1;
    }
    if (hh < 0) return 0;                          /* 连时间都没有：不导入 */

    if (hasDate) {
        it->start.wYear = (WORD)dy; it->start.wMonth = (WORD)dmo; it->start.wDay = (WORD)dd;
    } else {
        SYSTEMTIME base;
        GetLocalTime(&base);
        it->start.wYear = base.wYear; it->start.wMonth = base.wMonth; it->start.wDay = base.wDay;
    }
    it->start.wHour = (WORD)hh; it->start.wMinute = (WORD)mm; it->start.wSecond = 0;
    it->start.wMilliseconds = 0;
    if (ehh >= 0) {
        it->hasEnd = 1;
        it->end = it->start;
        it->end.wHour = (WORD)ehh; it->end.wMinute = (WORD)emm; it->end.wSecond = 0;
        if (it->end.wHour < it->start.wHour ||
            (it->end.wHour == it->start.wHour && it->end.wMinute <= it->start.wMinute)) {
            it->hasEnd = 0;                        /* 跨天/顺序不对：当没有结束时间 */
        }
    }
    if (mask && !hasDate) {
        it->weekly = 1;
        it->weekMask = mask;
        first_date_of_mask(mask, &it->start);
        it->start.wHour = (WORD)hh;
        it->start.wMinute = (WORD)mm;
        if (it->hasEnd) {
            it->end = it->start;
            it->end.wHour = (WORD)ehh;
            it->end.wMinute = (WORD)emm;
        }
    } else if (mask && hasDate) {
        it->weekly = 1;
        it->weekMask = mask;
    }
    it->derived = derived;

    /* 正文：其余列统一写进来；时间类列只写“没用上的那部分” */
    for (i = 0; i < r->n && i < CSV_MAX_COLS; i++) {
        const wchar_t *v = r->f[i];
        const wchar_t *label = NULL;
        if (i == info->colName) continue;
        if (!v || !v[0]) continue;
        if (info->isNo[i] && all_digits(v)) continue;
        if (info->headRow >= 0 && g_rows[info->headRow].f[i] && g_rows[info->headRow].f[i][0])
            label = g_rows[info->headRow].f[i];
        if (info->isTime[i] && (!residual_meaning(v) || is_weekday_only(v))) continue;
        body_add(body, 1600, label, v);
    }
    if (hasPeriod) {
        wchar_t pb[48];
        WBuf b;
        wb_init(&b, pb, 48);
        wb_add(&b, L"第");
        wb_int(&b, p1);
        if (p2 != p1) { wb_add(&b, L"-"); wb_int(&b, p2); }
        wb_add(&b, L"节");
        if (derived && tn < 2) wb_add(&b, L"（时间按内置作息表推导）");
        body_add(body, 1600, L"节次", pb);
    }

    wcsncpy(it->title, title, MAX_TITLE - 1);
    wcsncpy(it->body, body, 1599);
    it->body[1599] = 0;
    return 1;
}

/* 预览用：周一 08:00-09:40 / 每周 */
static void item_when(const CsvItem *it, wchar_t *buf, size_t cch)
{
    WBuf b;
    static const wchar_t *nm[7] = { L"日", L"一", L"二", L"三", L"四", L"五", L"六" };
    wb_init(&b, buf, cch);
    if (it->weekly) {
        int k, first = 1;
        wb_add(&b, L"周");
        for (k = 0; k < 7; k++) {
            if (!(it->weekMask & (1 << k))) continue;
            if (!first) wb_ch(&b, L'/');
            wb_add(&b, nm[k]);
            first = 0;
        }
        wb_ch(&b, L' ');
    }
    {
        wchar_t t[16];
        WBuf bb;
        wb_init(&bb, t, 16);
        wb_int(&bb, it->start.wHour);
        wb_ch(&bb, L':');
        if (it->start.wMinute < 10) wb_ch(&bb, L'0');
        wb_int(&bb, it->start.wMinute);
        wb_add(&b, t);
    }
    if (it->hasEnd) {
        wchar_t t[16];
        WBuf bb;
        wb_ch(&b, L'-');
        wb_init(&bb, t, 16);
        wb_int(&bb, it->end.wHour);
        wb_ch(&bb, L':');
        if (it->end.wMinute < 10) wb_ch(&bb, L'0');
        wb_int(&bb, it->end.wMinute);
        wb_add(&b, t);
    }
    if (it->derived) wb_add(&b, L"（推导）");
}

/* 表格型：表头 + 逐行 */
static void analyze_table(CsvInfo *info)
{
    int i, k, dataFrom;
    int kinds[CSV_MAX_COLS];
    int nameScore[CSV_MAX_COLS], timeScore[CSV_MAX_COLS];

    memset(kinds, 0, sizeof(kinds));
    memset(nameScore, 0, sizeof(nameScore));
    memset(timeScore, 0, sizeof(timeScore));

    /* 表头：前 8 行里关键词最多的那一行（≥2 个关键词才算表头） */
    info->headRow = -1;
    {
        int best = 0, bestIdx = -1;
        for (i = 0; i < g_rowN && i < 8; i++) {
            int s;
            if (row_is_empty(&g_rows[i])) continue;
            s = head_score(i, kinds);
            if (s > best) { best = s; bestIdx = i; }
        }
        if (best >= 2) {
            info->headRow = bestIdx;
            head_score(bestIdx, kinds);
        } else {
            memset(kinds, 0, sizeof(kinds));
        }
    }
    dataFrom = (info->headRow >= 0) ? info->headRow + 1 : 0;

    /* 逐列统计内容（名称列按“名字长度”加权：课名通常比教师名长） */
    for (i = 0; i < g_rowN; i++) {
        CsvRow *r = &g_rows[i];
        if (i == info->headRow || i < dataFrom) continue;
        for (k = 0; k < r->n && k < CSV_MAX_COLS; k++) {
            const wchar_t *v = r->f[k];
            if (!v || !v[0]) continue;
            if (looks_like_name(v)) nameScore[k] += (int)wcslen(v);
            if (re_hit(RE_WEEKDAY, v) || re_hit(RE_PERIOD, v) || re_hit(RE_DATE, v) ||
                re_hit(RE_TIME, v))
                timeScore[k]++;
        }
    }

    info->colName = info->colTime = info->colWeek = info->colDate = -1;
    for (k = 0; k < g_colN; k++) {
        if ((kinds[k] & CK_NAME) && info->colName < 0) info->colName = k;
        if ((kinds[k] & CK_WEEK) && info->colWeek < 0) info->colWeek = k;
        if ((kinds[k] & CK_DATE) && info->colDate < 0) info->colDate = k;
        if ((kinds[k] & CK_TIME) && info->colTime < 0) info->colTime = k;
        if (kinds[k] & CK_NO) info->isNo[k] = 1;
    }
    /* 表头没认到就用内容兜底 */
    if (info->colName < 0) {
        int best = 0;
        for (k = 0; k < g_colN; k++)
            if (nameScore[k] > best) { best = nameScore[k]; info->colName = k; }
    }
    if (info->colTime < 0 && info->colWeek < 0) {
        int best = 0;
        for (k = 0; k < g_colN; k++) {
            if (k == info->colName) continue;
            if (timeScore[k] > best) { best = timeScore[k]; info->colTime = k; }
        }
    }
    /* 时间类列：表头认出来的 + 内容像时间的（残值才写正文） */
    for (k = 0; k < g_colN; k++) {
        if (k == info->colName) continue;
        if ((kinds[k] & (CK_TIME | CK_WEEK | CK_DATE)) || timeScore[k] > 0)
            info->isTime[k] = 1;
    }
    if (info->colName >= 0) info->isTime[info->colName] = 0;

    if (info->colName >= 0 && info->colName < g_colN &&
        g_rows[info->headRow >= 0 ? info->headRow : 0].f[info->colName])
        wcsncpy(info->headName, g_rows[info->headRow >= 0 ? info->headRow : 0].f[info->colName],
                MAX_TITLE - 1);
    if (!info->headName[0]) wcsncpy(info->headName, L"（按内容推断）", MAX_TITLE - 1);
    if (info->colTime >= 0 && info->headRow >= 0 && g_rows[info->headRow].f[info->colTime])
        wcsncpy(info->headTime, g_rows[info->headRow].f[info->colTime], MAX_TITLE - 1);
    else if (info->colWeek >= 0 && info->headRow >= 0 && g_rows[info->headRow].f[info->colWeek])
        wcsncpy(info->headTime, g_rows[info->headRow].f[info->colWeek], MAX_TITLE - 1);
    if (!info->headTime[0]) {
        if (info->colTime >= 0 || info->colWeek >= 0 || info->colDate >= 0)
            wcsncpy(info->headTime, L"（按内容推断）", MAX_TITLE - 1);
        else
            wcsncpy(info->headTime, L"（时间写在别的列里）", MAX_TITLE - 1);
    }

    /* 逐行生成 */
    for (i = dataFrom; i < g_rowN; i++) {
        CsvItem it;
        if (row_is_empty(&g_rows[i])) continue;
        if (g_itemN >= CSV_MAX_ITEMS) { g_capped = 1; break; }
        if (!row_to_item(&g_rows[i], info, &it)) { info->skipped++; continue; }
        if (it.derived && !it.weekly && it.start.wHour == 8 && it.start.wMinute == 0)
            info->deriveUsed = 1;
        if (it.derived) info->periodUsed = 1;
        g_items[g_itemN++] = it;
    }

    /* 正文列数 + 用到的列名 */
    for (k = 0; k < g_colN; k++) {
        if (k == info->colName) continue;
        if (info->isNo[k] || info->isTime[k]) continue;
        info->bodyCols++;
    }
}

/* 网格型：行 = 节次，列 = 星期，格子里是课程（可多行 / 多段） */
static int grid_detect(int *pHead, int *pDataFrom)
{
    int i, k, wdCols = 0, periodRows = 0;
    int head = -1;
    CsvRow *hr;
    if (g_rowN < 3 || g_colN < 4) return 0;
    for (i = 0; i < g_rowN && i < 6; i++) {
        int cnt = 0;
        CsvRow *r = &g_rows[i];
        for (k = 1; k < r->n; k++) {
            int mask = 0;
            if (scan_weekdays(r->f[k], &mask) && r->f[k] && wcslen(r->f[k]) <= 12) cnt++;
        }
        if (cnt >= 3) { head = i; break; }
    }
    if (head < 0) return 0;
    hr = &g_rows[head];
    for (k = 1; k < hr->n; k++) {
        int mask = 0;
        if (scan_weekdays(hr->f[k], &mask)) wdCols++;
    }
    for (i = head + 1; i < g_rowN; i++) {
        int p1 = 0, p2 = 0;
        CsvRow *r = &g_rows[i];
        if (row_is_empty(r)) continue;
        if (scan_period(r->f[0], &p1, &p2)) periodRows++;
        else if (all_digits(r->f[0])) periodRows++;
    }
    if (wdCols < 3 || periodRows < 2) return 0;
    *pHead = head;
    *pDataFrom = head + 1;
    return 1;
}

/* 网格单元格 → 课名 + 正文：
   第一行里用 / ｜ 、 分开的，第一段是课名；其余行依次进正文 */
static void grid_body_add(wchar_t *body, size_t cch, const wchar_t *txt)
{
    size_t cl = wcslen(body);
    if (!txt || !txt[0]) return;
    if (cl) {
        if (cl + 1 >= cch) return;
        wcsncat(body, L"\n", cch - cl - 1);
        cl = wcslen(body);
    }
    wcsncat(body, txt, cch - cl - 1);
}

static void grid_split_group(const wchar_t *group, wchar_t *title, size_t tcch,
                             wchar_t *body, size_t bcch)
{
    static const wchar_t *seps = L"/／|｜、";
    wchar_t tmp[512];
    wchar_t *p, *nl;
    int first = 1;
    if (tcch) title[0] = 0;
    if (bcch) body[0] = 0;
    wcsncpy(tmp, group, 511);
    tmp[511] = 0;
    nl = wcschr(tmp, L'\n');
    if (nl) *nl = 0;
    p = tmp;
    for (;;) {
        wchar_t *q = p;
        wchar_t save;
        while (*q && !wcschr(seps, *q)) q++;
        save = *q;
        *q = 0;
        w_trim(p);
        if (p[0]) {
            if (first) {
                wcsncpy(title, p, tcch - 1);
                title[tcch - 1] = 0;
                first = 0;
            } else {
                grid_body_add(body, bcch, p);
            }
        }
        *q = save;
        if (!save) break;
        p = q + 1;
    }
    if (nl) {
        w_trim(nl + 1);
        grid_body_add(body, bcch, nl + 1);
    }
}

/* 网格里往单元格取出一节课：group 的首段 = 课名，其余进正文 */
static void grid_emit(const wchar_t *group, int mask, int sh, int sm, int eh, int em,
                      int p1, int p2)
{
    CsvItem it;
    SYSTEMTIME base;
    wchar_t pb[24];
    WBuf b;
    memset(&it, 0, sizeof(it));
    grid_split_group(group, it.title, MAX_TITLE, it.body, 1600);
    if (!it.title[0]) return;
    it.weekly = 1;
    it.weekMask = mask;
    GetLocalTime(&base);
    it.start.wYear = base.wYear;
    it.start.wMonth = base.wMonth;
    it.start.wDay = base.wDay;
    first_date_of_mask(mask, &it.start);
    it.start.wHour = (WORD)sh;
    it.start.wMinute = (WORD)sm;
    it.end = it.start;
    it.end.wHour = (WORD)eh;
    it.end.wMinute = (WORD)em;
    it.hasEnd = 1;
    if (it.end.wHour < it.start.wHour ||
        (it.end.wHour == it.start.wHour && it.end.wMinute <= it.start.wMinute))
        it.hasEnd = 0;
    wb_init(&b, pb, 24);
    wb_add(&b, L"第");
    wb_int(&b, p1);
    if (p2 != p1) { wb_add(&b, L"-"); wb_int(&b, p2); }
    wb_add(&b, L"节");
    body_add(it.body, 1600, L"节次", pb);
    if (g_itemN < CSV_MAX_ITEMS) g_items[g_itemN++] = it;
    else g_capped = 1;
}

static void analyze_grid(CsvInfo *info, int head, int dataFrom)
{
    int i, k;
    CsvRow *hr = &g_rows[head];
    int colMask[CSV_MAX_COLS];

    memset(colMask, 0, sizeof(colMask));
    for (k = 1; k < hr->n && k < CSV_MAX_COLS; k++) {
        int mask = 0;
        if (!scan_weekdays(hr->f[k], &mask) && all_digits(hr->f[k])) {
            /* 表头写成「1」「2」的也有 */
            int v = (int)wcstol(hr->f[k], NULL, 10);
            int b = weekday_bit(v);
            if (b >= 0) mask = 1 << b;
        }
        colMask[k] = mask;
    }
    info->headRow = head;
    info->colName = 0;                 /* 网格型：名字在格子里 */

    for (i = dataFrom; i < g_rowN; i++) {
        CsvRow *r = &g_rows[i];
        int p1 = 0, p2 = 0, hasP = 0, sh = 8, sm = 0, eh = 8, em = 45;
        if (row_is_empty(r)) continue;
        hasP = scan_period(r->f[0], &p1, &p2);
        if (hasP) {
            if (!period_start(p1, &sh, &sm)) continue;
            if (period_start(p2, &eh, &em)) {
                em += PERIOD_MIN;
                if (em >= 60) { em -= 60; eh++; }
            } else {
                eh = sh;
                em = sm + PERIOD_MIN;
                if (em >= 60) { em -= 60; eh++; }
            }
            info->periodUsed = 1;
        } else if (all_digits(r->f[0])) {
            p1 = p2 = (int)wcstol(r->f[0], NULL, 10);
            if (p1 < 1 || p1 > 12) continue;
            if (!period_start(p1, &sh, &sm)) continue;
            eh = sh;
            em = sm + PERIOD_MIN;
            if (em >= 60) { em -= 60; eh++; }
            info->periodUsed = 1;
        } else {
            /* 第一列也可能直接写成 08:00 或 08:00-09:40 */
            int tm[4][2];
            int n = scan_times(r->f[0], tm, 4);
            if (n < 1) continue;
            p1 = p2 = 0;
            sh = tm[0][0]; sm = tm[0][1];
            if (n >= 2) { eh = tm[1][0]; em = tm[1][1]; }
            else { eh = sh; em = sm + PERIOD_MIN; if (em >= 60) { em -= 60; eh++; } }
        }
        for (k = 1; k < r->n && k < CSV_MAX_COLS; k++) {
            const wchar_t *cell = r->f[k];
            const wchar_t *p;
            wchar_t group[512];
            int gline = 0;
            if (!colMask[k] || !cell || !cell[0]) continue;
            /* 格子里按空行分段：一段 = 一节课 */
            p = cell;
            memset(group, 0, sizeof(group));
            for (;;) {
                const wchar_t *e = p;
                size_t n;
                wchar_t seg[256];
                while (*e && *e != L'\n' && *e != L'\r') e++;
                n = (size_t)(e - p);
                if (n > 255) n = 255;
                memcpy(seg, p, n * sizeof(wchar_t));
                seg[n] = 0;
                w_trim(seg);
                if (seg[0]) {
                    size_t cl = wcslen(group);
                    if (cl) {
                        if (cl + 1 < 512) wcsncat(group, L"\n", 511 - cl);
                        cl = wcslen(group);
                    }
                    wcsncat(group, seg, 511 - cl);
                    gline++;
                } else if (gline) {
                    grid_emit(group, colMask[k], sh, sm, eh, em, p1, p2);
                    memset(group, 0, sizeof(group));
                    gline = 0;
                }
                if (!*e) break;
                /* 一次只吃一个换行：空行要留成“空段”，它是一节课的分界 */
                if (*e == L'\r') { e++; if (*e == L'\n') e++; }
                else if (*e == L'\n') e++;
                p = e;
            }
            if (gline) grid_emit(group, colMask[k], sh, sm, eh, em, p1, p2);
        }
    }
    wcsncpy(info->headName, L"单元格内容（首行 = 课名，其余进正文）", MAX_TITLE - 1);
    wcsncpy(info->headTime, L"行 = 节次，列 = 星期", MAX_TITLE - 1);
}

static void analyze(CsvInfo *info)
{
    int head = -1, dataFrom = 0;
    memset(info, 0, sizeof(*info));
    info->headRow = -1;
    info->colName = info->colTime = info->colWeek = info->colDate = -1;
    info->rows = g_rowN;
    info->cols = g_colN;
    g_itemN = 0;
    if (grid_detect(&head, &dataFrom)) {
        info->mode = 2;
        analyze_grid(info, head, dataFrom);
    } else {
        info->mode = 1;
        analyze_table(info);
    }
}

static void csv_free_all(void)
{
    rows_free();
    g_itemN = 0;
    g_dupSkipped = 0;
    g_path[0] = 0;
}

/* ======================================================================
 *  7. 导入 + 识别结果对话框
 * ====================================================================*/
#define CSVDLG_CLASS L"ZScheduleCsvImportWnd"
#define IDC_CSV_LIST   3090
#define IDC_CSV_NOTES  3091      /* 导入并打开便笺 */
#define IDC_CSV_ONLY   3092      /* 仅导入到清单 */
#define IDC_CSV_CANCEL 3093

typedef struct {
    HWND hSelf, hList, hNotes, hOnly, hCancel;
    HFONT fUI;
    int  scale;
    int  alive;
    int  choice;                 /* 0 = 取消，1 = 仅导入，2 = 导入并打开便笺 */
} CsvDlg;
static CsvDlg  g_csv;
static CsvInfo g_aux_info;       /* 对话框要显示识别结果 */

static void csv_dlg_fill(void)
{
    int i;
    for (i = 0; i < g_itemN; i++) {
        wchar_t line[600], when[64];
        WBuf b;
        item_when(&g_items[i], when, 64);
        wb_init(&b, line, 600);
        wb_int(&b, i + 1);
        wb_add(&b, L". ");
        wb_add(&b, g_items[i].title);
        wb_add(&b, L"   ");
        wb_add(&b, when);
        if (g_items[i].body[0]) {
            int nl = 0;
            const wchar_t *p;
            for (p = g_items[i].body; *p; p++) if (*p == L'\n') nl++;
            wb_add(&b, L"   · 正文 ");
            wb_int(&b, nl + 1);
            wb_add(&b, L" 行");
        }
        SendMessageW(g_csv.hList, LB_ADDSTRING, 0, (LPARAM)line);
    }
}

static void csv_dlg_hint(wchar_t *buf, size_t cch)
{
    WBuf b;
    wchar_t fn[MAX_PATH];
    wb_init(&b, buf, cch);
    file_name_of(g_path, fn, MAX_PATH);
    wb_add(&b, L"文件：");
    wb_add(&b, fn);
    wb_add(&b, L"\r\n");
    if (g_aux_info.mode == 2) {
        wb_add(&b, L"格式：课表网格（行 = 节次、列 = 星期）");
        wb_add(&b, L"\r\n");
        wb_add(&b, L"共 ");
        wb_int(&b, g_aux_info.rows);
        wb_add(&b, L" 行 × ");
        wb_int(&b, g_aux_info.cols);
        wb_add(&b, L" 列，识别出 ");
        wb_int(&b, g_itemN);
        wb_add(&b, L" 节课");
    } else {
        wb_add(&b, L"格式：表格（一行一节课）　表头：");
        if (g_aux_info.headRow >= 0) {
            wb_add(&b, L"第 ");
            wb_int(&b, g_aux_info.headRow + 1);
            wb_add(&b, L" 行");
        } else {
            wb_add(&b, L"没识别到（按内容推断）");
        }
        wb_add(&b, L"　列：");
        wb_int(&b, g_aux_info.cols);
        wb_add(&b, L"（分隔符「");
        wb_ch(&b, g_delim ? g_delim : L' ');
        wb_add(&b, L"」）");
        wb_add(&b, L"\r\n");
        wb_add(&b, L"名称列：");
        wb_add(&b, g_aux_info.headName);
        wb_add(&b, L"　时间列：");
        wb_add(&b, g_aux_info.headTime);
        wb_add(&b, L"　其余 ");
        wb_int(&b, g_aux_info.bodyCols);
        wb_add(&b, L" 列写入正文");
    }
    wb_add(&b, L"\r\n");
    wb_add(&b, L"共 ");
    wb_int(&b, g_itemN);
    wb_add(&b, L" 节课可导入");
    if (g_aux_info.periodUsed) wb_add(&b, L"　· 节次按内置作息表推导时间");
    if (g_aux_info.skipped) { wb_add(&b, L"　· 跳过 "); wb_int(&b, g_aux_info.skipped); wb_add(&b, L" 行"); }
    if (g_capped) wb_add(&b, L"　· 行数超上限，只取前 300 节");
}

static LRESULT CALLBACK csv_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        {
            int sc = g_csv.scale;
            HWND h;
            wchar_t hint[1400];
            csv_dlg_hint(hint, 1400);
            g_csv.fUI = CreateFontW(-MulDiv(10, sc * 96, 9600), 0, 0, 0, FW_NORMAL, 0, 0, 0,
                                    DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Microsoft YaHei UI");
            h = CreateWindowExW(0, L"STATIC", hint, WS_CHILD | WS_VISIBLE,
                                MulDiv(16, sc, 100), MulDiv(12, sc, 100),
                                MulDiv(860, sc, 100), MulDiv(80, sc, 100), hwnd, NULL, g_hInst, NULL);
            SendMessageW(h, WM_SETFONT, (WPARAM)g_csv.fUI, TRUE);
            g_csv.hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                          WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                                          MulDiv(16, sc, 100), MulDiv(98, sc, 100),
                                          MulDiv(860, sc, 100), MulDiv(300, sc, 100),
                                          hwnd, (HMENU)(INT_PTR)IDC_CSV_LIST, g_hInst, NULL);
            SendMessageW(g_csv.hList, WM_SETFONT, (WPARAM)g_csv.fUI, TRUE);
            g_csv.hNotes = CreateWindowExW(0, L"BUTTON", L"导入并打开便笺",
                                           WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                           MulDiv(560, sc, 100), MulDiv(406, sc, 100),
                                           MulDiv(150, sc, 100), MulDiv(32, sc, 100),
                                           hwnd, (HMENU)(INT_PTR)IDC_CSV_NOTES, g_hInst, NULL);
            g_csv.hOnly = CreateWindowExW(0, L"BUTTON", L"仅导入到清单",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                          MulDiv(716, sc, 100), MulDiv(406, sc, 100),
                                          MulDiv(160, sc, 100), MulDiv(32, sc, 100),
                                          hwnd, (HMENU)(INT_PTR)IDC_CSV_ONLY, g_hInst, NULL);
            g_csv.hCancel = CreateWindowExW(0, L"BUTTON", L"取消",
                                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                            MulDiv(440, sc, 100), MulDiv(406, sc, 100),
                                            MulDiv(112, sc, 100), MulDiv(32, sc, 100),
                                            hwnd, (HMENU)(INT_PTR)IDC_CSV_CANCEL, g_hInst, NULL);
            SendMessageW(g_csv.hNotes, WM_SETFONT, (WPARAM)g_csv.fUI, TRUE);
            SendMessageW(g_csv.hOnly, WM_SETFONT, (WPARAM)g_csv.fUI, TRUE);
            SendMessageW(g_csv.hCancel, WM_SETFONT, (WPARAM)g_csv.fUI, TRUE);
            csv_dlg_fill();
            SetFocus(g_csv.hList);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDC_CSV_NOTES:
            g_csv.choice = 2;
            g_csv.alive = 0;
            DestroyWindow(hwnd);
            return 0;
        case IDC_CSV_ONLY:
            g_csv.choice = 1;
            g_csv.alive = 0;
            DestroyWindow(hwnd);
            return 0;
        case IDC_CSV_CANCEL:
            g_csv.choice = 0;
            g_csv.alive = 0;
            DestroyWindow(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        g_csv.choice = 0;
        g_csv.alive = 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_csv.fUI) { DeleteObject(g_csv.fUI); g_csv.fUI = NULL; }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void center_over(HWND owner, HWND child, int w, int h)
{
    RECT ro, wa;
    int x, y;
    if (!owner || !GetWindowRect(owner, &ro)) {
        ro.left = 0; ro.top = 0; ro.right = 1024; ro.bottom = 768;
    }
    x = ro.left + ((ro.right - ro.left) - w) / 2;
    y = ro.top + ((ro.bottom - ro.top) - h) / 2;
    wa.left = 0; wa.top = 0; wa.right = 1024; wa.bottom = 768;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (x + w > wa.right) x = wa.right - w;
    if (y + h > wa.bottom) y = wa.bottom - h;
    if (x < wa.left) x = wa.left;
    if (y < wa.top) y = wa.top;
    SetWindowPos(child, NULL, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
}

static int csv_dialog(HWND owner)
{
    static int regd;
    HWND hwnd;
    MSG msg;
    int sc, wa;
    RECT rc;
    if (!regd) {
        WNDCLASSEXW wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = csv_proc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = CSVDLG_CLASS;
        wc.hIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(APP_ICON_ID), IMAGE_ICON,
                                     GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), 0);
        wc.hIconSm = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(APP_ICON_ID), IMAGE_ICON,
                                       GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
        RegisterClassExW(&wc);
        regd = 1;
    }
    {
        HDC hdc = GetDC(NULL);
        sc = MulDiv(GetDeviceCaps(hdc, LOGPIXELSY), 100, 96);
        ReleaseDC(NULL, hdc);
    }
    wa = GetSystemMetrics(SM_CYSCREEN) - 80;
    if (MulDiv(462, sc, 100) > wa) sc = (int)((LONGLONG)wa * 100 / 462);
    if (MulDiv(892, sc, 100) > GetSystemMetrics(SM_CXSCREEN) - 40)
        sc = (int)((LONGLONG)(GetSystemMetrics(SM_CXSCREEN) - 40) * 100 / 892);
    if (sc < 70) sc = 70;
    g_csv.scale = sc;
    g_csv.choice = 0;
    g_csv.alive = 1;
    rc.left = 0; rc.top = 0;
    rc.right = MulDiv(892, sc, 100);
    rc.bottom = MulDiv(462, sc, 100);
    AdjustWindowRectEx(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, CSVDLG_CLASS, L"导入 CSV 课表",
                           WS_POPUP | WS_CAPTION | WS_SYSMENU,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           rc.right - rc.left, rc.bottom - rc.top,
                           owner, NULL, g_hInst, NULL);
    if (!hwnd) return 0;
    g_csv.hSelf = hwnd;
    center_over(owner, hwnd, rc.right - rc.left, rc.bottom - rc.top);
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    if (owner) EnableWindow(owner, FALSE);
    while (g_csv.alive) {
        if (!GetMessageW(&msg, NULL, 0, 0)) break;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            g_csv.choice = 0;
            g_csv.alive = 0;
            break;
        }
        if (g_csv.hSelf && IsDialogMessageW(g_csv.hSelf, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_csv.hSelf && IsWindow(g_csv.hSelf)) DestroyWindow(g_csv.hSelf);
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    return g_csv.choice;
}

static int csv_dup_exists(const Schedule *s)
{
    int i;
    for (i = 0; i < model_count(); i++) {
        const Schedule *o = model_at(i);
        if (!o) continue;
        if (wcscmp(o->title, s->title) != 0) continue;
        if (o->repKind != s->repKind || o->weekMask != s->weekMask) continue;
        if (o->start.wHour != s->start.wHour || o->start.wMinute != s->start.wMinute) continue;
        return 1;
    }
    return 0;
}

/* 生成便笺：一条 CSV 行 = 一条课表便笺 */
static void csv_apply(HWND owner, int openNotes)
{
    int ids[CSV_MAX_ITEMS];
    int i, added = 0, opened = 0, failed = 0;
    int style = NOTE_STYLE_PAPER, color = 0;
    g_dupSkipped = 0;

    if (note_count() > 0) {
        int lid[1];
        if (note_list(lid, 1) > 0) {
            int st = NOTE_STYLE_PAPER, ci = 0;
            note_get_look(lid[0], &st, &ci);
            if (st == NOTE_STYLE_SYSTEM) style = NOTE_STYLE_SYSTEM;   /* 沿用已有外观 */
            if (ci >= 0 && ci < NOTE_COLOR_COUNT) color = ci;
        }
    }

    for (i = 0; i < g_itemN; i++) {
        CsvItem *it = &g_items[i];
        Schedule s;
        Schedule *add;
        memset(&s, 0, sizeof(s));
        s.enabled = 1;
        s.repInterval = 1;
        s.ttNext = 1;
        s.kind = KIND_TASK;
        s.timetable = 1;                 /* 课表便笺：到点自动开关便笺 */
        s.cdUnit = CDU_MIN;              /* 课表默认显示到分 */
        s.color = palette_color(added % 8);
        s.start = it->start;
        s.hasEnd = it->hasEnd;
        s.end = it->end;
        if (it->weekly) { s.repKind = REP_WEEKLY; s.weekMask = it->weekMask; }
        else              s.repKind = REP_NONE;
        wcsncpy(s.title, it->title, MAX_TITLE - 1);
        wcsncpy(s.note, it->body, MAX_NOTE - 1);
        model_normalize(&s);
        if (csv_dup_exists(&s)) { g_dupSkipped++; continue; }
        add = model_add(&s);
        if (add) {
            if (added < CSV_MAX_ITEMS) ids[added] = add->id;
            added++;
        } else {
            failed++;
        }
    }

    if (added > 0) {
        model_save(model_path());
        model_refresh_next();
        main_refresh_all();
        if (owner) PostMessageW(owner, WM_COMMAND, IDM_VIEW_LIST, 0);
        if (openNotes) {
            int lim = added < 16 ? added : 16;     /* 一次最多开 16 张，免得铺满屏幕 */
            note_open_batch(ids, lim, style, color);
            opened = lim;
        }
    }
    {
        wchar_t msg[320];
        WBuf b;
        wb_init(&b, msg, 320);
        if (added > 0) {
            wb_add(&b, L"CSV 课表：已导入 ");
            wb_int(&b, added);
            wb_add(&b, L" 节（课表便笺）");
            if (opened) { wb_add(&b, L"，已打开 "); wb_int(&b, opened); wb_add(&b, L" 张便笺"); }
        } else {
            wb_add(&b, L"CSV 课表：没有新增（");
        }
        if (g_dupSkipped) { wb_add(&b, L"跳过重复 "); wb_int(&b, g_dupSkipped); wb_add(&b, L" 条"); }
        if (failed) { wb_add(&b, L"失败 "); wb_int(&b, failed); wb_add(&b, L" 条"); }
        if (added <= 0) wb_add(&b, L"）");
        main_set_status(msg);
    }
}

/* ======================================================================
 *  8. 入口：文件 → 导入 CSV 课表…
 * ====================================================================*/
static int pick_csv_file(HWND owner, wchar_t *out)
{
    static const wchar_t *flt =
        L"课表文件 (*.csv;*.txt)\0*.csv;*.txt\0"
        L"Excel 导出的 CSV (*.csv)\0*.csv\0"
        L"所有文件 (*.*)\0*.*\0\0";
    OPENFILENAMEW ofn;
    if (!out) return 0;
    out[0] = 0;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = flt;
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"选择要导入的 CSV 课表文件";
    ofn.lpstrFile = out;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetOpenFileNameW(&ofn) ? 1 : 0;
}

void csv_import_show(HWND owner)
{
    wchar_t path[MAX_PATH];
    CsvInfo info;
    int choice;

    if (!re_ok()) {
        MessageBoxW(owner, L"内置识别引擎初始化失败。", L"导入 CSV 课表", MB_ICONERROR);
        return;
    }
    if (!pick_csv_file(owner, path)) return;
    if (!csv_load(path)) {
        MessageBoxW(owner, L"这个文件读不出来，或者里面没有内容。", L"导入 CSV 课表",
                    MB_ICONWARNING);
        csv_free_all();
        return;
    }
    analyze(&info);
    g_aux_info = info;
    if (g_itemN <= 0) {
        MessageBoxW(owner,
                    L"没能从文件里识别出课程。\n\n"
                    L"支持的两种格式：\n"
                    L"① 表格：一行一节课，表头含“课程名称 / 上课时间 / 星期 / 节次”等列；\n"
                    L"② 网格：行 = 节次、列 = 星期，格子里填课程名。",
                    L"导入 CSV 课表", MB_ICONINFORMATION);
        csv_free_all();
        return;
    }
    choice = csv_dialog(owner);
    if (choice >= 1) csv_apply(owner, choice == 2);
    csv_free_all();
}
