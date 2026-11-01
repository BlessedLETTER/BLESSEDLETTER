"""v1.9.3 测试数据：竖图+长正文 / 纯正文 / 纯照片 三条便笺，写到 _ttest3。
   直接构造 24 位 BMP（不依赖 PIL），数据文件 UTF-16LE。"""
import os, struct, io, winreg

D = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(D)
DIR = os.path.join(ROOT, "_ttest3")
os.makedirs(DIR, exist_ok=True)


def bmp(path, w, h, bg=(200, 200, 200), border=(190, 30, 30)):
    row = (w * 3 + 3) // 4 * 4
    data = bytearray()
    for y in range(h):
        r = bytearray()
        for x in range(w):
            if x < 6 or x >= w - 6 or y < 6 or y >= h - 6:
                r += bytes(border)
            else:
                # X 对角线
                if abs(y - x * h // w) < 3 or abs(y - (h - 1 - x * h // w)) < 3:
                    r += bytes((140, 140, 140))
                else:
                    r += bytes(bg)
        r += b"\x00" * (row - len(r))
        data += r
    size = 54 + len(data)
    hdr = b"BM" + struct.pack("<IHHI", size, 0, 0, 54)
    hdr += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    open(path, "wb").write(hdr + data)


bmp(os.path.join(DIR, "tall.bmp"), 400, 1200)
bmp(os.path.join(DIR, "wide.bmp"), 1200, 300, bg=(180, 190, 205))

BODY = "\n".join([
    "第一行：这是 v1.9.3 的正文滚动测试。",
    "第二行：滚轮往下滚，正文应该一行一行往上走。",
    "第三行：以前滚到底时整段正文会消失，这是要修的主 bug。",
    "第四行：照片放大后，正文至少还要留一行可见。",
    "第五行：其余正文应该能用滚轮滚出来，而不是被裁掉。",
    "第六行：便笺墙卡片也是同一套规则。",
    "第七行：这一行用来凑长度，确保内容超过可视高度。",
    "第八行：滚到最底时，最后这一行应该贴在便笺底边。",
    "第九行：如果能看到这行，说明滚动范围也算对了。",
    "第十行：结束时滚动条应该在最低处。",
])

items = []
items.append(dict(id=1, title="竖图+长正文（v1.9.3 测试）", note=BODY, img=1, attach="tall.bmp"))
items.append(dict(id=2, title="纯正文（v1.9.3 回归）", note=BODY, img=0))
items.append(dict(id=3, title="纯照片（v1.9.3 回归）", note="", img=1, attach="wide.bmp"))


def esc(s):
    return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r")


out = ["ZScheduleData/4"]
for it in items:
    out += ["[item]",
            "id=%d" % it["id"],
            "title=" + esc(it["title"]),
            "note=" + esc(it["note"]),
            "rtf=", "link=",
            "start=2026-11-14 09:00:00", "end=2026-11-14 09:00:00",
            "hasend=0", "rep=0", "interval=1", "unit=0", "week=0",
            "enabled=1", "done=0", "kind=0",
            "showimg=%d" % (1 if it["img"] else 0),
            "imgpct=0", "timetable=0", "ttnext=0", "cdunit=0",
            "remind=0", "remlead=0", "remrep=10", "remtimes=1",
            "sndmode=0",
            "imgn=%d" % (1 if it["img"] else 0),
            "imgmode=0", "imglayer=0",
            "color=3B6FE0"]
    if it["img"]:
        out.append("attach=" + os.path.join(DIR, it["attach"]).replace("\\", "\\\\"))
    out.append("[/item]")
text = "\r\n".join(out) + "\r\n"
with open(os.path.join(DIR, "zsched_data.txt"), "wb") as f:
    f.write(b"\xff\xfe" + text.encode("utf-16-le"))

notes = ["ttauto=1",
         "id=1 style=1 color=0 top=1 x=140 y=110 w=620 h=620",
         "id=2 style=1 color=0 top=1 x=790 y=110 w=520 h=420",
         "id=3 style=1 color=0 top=1 x=140 y=760 w=520 h=420"]
with open(os.path.join(DIR, "zsched_notes.txt"), "wb") as f:
    f.write(b"\xff\xfe" + ("\r\n".join(notes) + "\r\n").encode("utf-16-le"))

key = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Jian")
winreg.SetValueEx(key, "DataDir", 0, winreg.REG_SZ, DIR + "\\")
winreg.CloseKey(key)
print("test data ready:", DIR)
