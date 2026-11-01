#ifndef ZS_CORE_STRBUF_H
#define ZS_CORE_STRBUF_H

#include "core/types.h"

void  ws_init(WStr *s);
void  ws_free(WStr *s);
int   ws_reserve(WStr *s, size_t need);   /* 0 = 成功 */
void  ws_add(WStr *s, const wchar_t *txt);
void  ws_ch(WStr *s, wchar_t c);
void  ws_int(WStr *s, long long v);
void  ws_u2(WStr *s, unsigned v);

void  wb_init(WBuf *b, wchar_t *buf, size_t cap);
void  wb_add(WBuf *b, const wchar_t *txt);
void  wb_ch(WBuf *b, wchar_t c);
void  wb_int(WBuf *b, long long v);
void  wb_u2(WBuf *b, unsigned v);

void  wb_u3(WBuf *b, unsigned v);

#endif /* ZS_CORE_STRBUF_H */
