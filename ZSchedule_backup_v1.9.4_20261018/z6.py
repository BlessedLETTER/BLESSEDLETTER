# -*- coding: utf-8 -*-
import io
p = 'ui_main.c'
s = io.open(p, encoding='utf-8').read()

def rep(s, old, new, n=1):
    assert s.count(old) == n, ('match', s.count(old), repr(old[:60]))
    return s.replace(old, new)

# 面板大号文案
s = rep(s, """                wb_add(&b, L"\\u5df2\\u8fc7\\u53bb ");
                wb_int(&b, (long long)sched_anniv_elapsed(s, NULL, NULL, NULL));
                wb_add(&b, L" \\u5929");""",
"""                wb_add(&b, sched_anniv_text(s, buf + 70, 80));""")

# 表格：两处（建立 + 每秒刷新）
s = rep(s, """            WBuf wb;
            wb_init(&wb, b1, 128);
            wb_add(&wb, L"\\u5df2\\u8fc7\\u53bb ");
            wb_int(&wb, (long long)sched_anniv_elapsed(s, NULL, NULL, NULL));
            wb_add(&wb, L" \\u5929");
            it.pszText = b1;""",
"""            it.pszText = (LPWSTR)sched_anniv_text(s, b3, 256);""")

s = rep(s, """                WBuf wb;
                wb_init(&wb, b1, 128);
                wb_add(&wb, L"\\u5df2\\u8fc7\\u53bb ");
                wb_int(&wb, (long long)sched_anniv_elapsed(s, NULL, NULL, NULL));
                wb_add(&wb, L" \\u5929");
                it.pszText = b1;""",
"""                it.pszText = (LPWSTR)sched_anniv_text(s, b1, 128);""")

io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('ui_main.c ok')
