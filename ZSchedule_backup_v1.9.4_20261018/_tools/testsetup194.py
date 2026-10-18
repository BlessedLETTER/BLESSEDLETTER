"""v1.9.4 测试数据：三张便笺（其中一张带照片），写到 _ttest4。"""
import os, struct, winreg

D = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(D)
DIR = os.path.join(ROOT, "_ttest4")
os.makedirs(DIR, exist_ok=True)


def bmp(path, w, h, bg=(200, 200, 200), border=(190, 30, 30)):
    row = (w * 3 + 3) // 4 * 4
    data = bytearray()
    for y in range(h):
        r = bytearray()
        for x in range(w):
            if x < 6 or x >= w - 6 or y < 6 or y >= h - 6:
                r += bytes(border)
            elif abs(y - x * h // w) < 3 or abs(y - (h - 1 - x * h // w)) < 3:
                r += bytes((140, 140, 140))
            else:
                r += bytes(bg)
        r += b"\x00" * (row - len(r))
        data += r
    hdr = b"BM" + struct.pack("<IHHI", 54 + len(data), 0, 0, 54)
    hdr += struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    open(path, "wb").write(hdr + data)


bmp(os.path.join(DIR, "tall.bmp"), 400, 1200)

BODY = "这是 v1.9.4 复制/粘贴测试便笺。\n可以复制后到新开的一页粘贴。"

items = [
    dict(id=1, title="复制测试-A", note=BODY, img=1),
    dict(id=2, title="复制测试-B", note="第二张便笺。", img=0),
    dict(id=3, title="复制测试-C", note="第三张便笺。", img=0),
]


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
        out.append("attach=" + os.path.join(DIR, "tall.bmp").replace("\\", "\\\\"))
    out.append("[/item]")

with open(os.path.join(DIR, "zsched_data.txt"), "wb") as f:
    f.write(b"\xff\xfe" + ("\r\n".join(out) + "\r\n").encode("utf-16-le"))

notes = ["ttauto=1",
         "id=1 style=1 color=0 top=1 x=200 y=150 w=500 h=420",
         "id=2 style=1 color=0 top=1 x=720 y=150 w=500 h=420",
         "id=3 style=1 color=0 top=1 x=200 y=600 w=500 h=420"]
with open(os.path.join(DIR, "zsched_notes.txt"), "wb") as f:
    f.write(b"\xff\xfe" + ("\r\n".join(notes) + "\r\n").encode("utf-16-le"))

key = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Jian")
winreg.SetValueEx(key, "DataDir", 0, winreg.REG_SZ, DIR + "\\")
winreg.CloseKey(key)
print("test data ready:", DIR)
