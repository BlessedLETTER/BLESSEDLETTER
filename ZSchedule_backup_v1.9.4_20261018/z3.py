# -*- coding: utf-8 -*-
import io
p = 'image.c'
s = io.open(p, encoding='utf-8').read()
a = s.index("/* \u4fbf\u7b3a/\u770b\u56fe\u7528\u7684\u201c\u539f\u5c3a\u5bf8\u201d\u7f13\u5b58")
b = s.index("/* \u586b\u6ee1 (x,y,cx,cy)")
removed = s[a:b]
assert 'IMG_FULL_MAX' in removed and len(removed) < 3000, len(removed)
s = s[:a] + s[b:]
# 用已有的 FullEnt 版本：img_draw_cover 里的变量类型要跟着改
s = s.replace("""    ImgEntry *e = img_get_full(path);   /* \u7528\u539f\u5c3a\u5bf8\uff0c\u4e0d\u7528\u7f29\u7565\u56fe */""",
              """    FullEnt *e = img_get_full(path);    /* \u7528\u539f\u5c3a\u5bf8\uff0c\u4fdd\u8bc1\u653e\u5927\u540e\u4e5f\u6e05\u6670 */""")
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('duplicate removed, len', len(s))
