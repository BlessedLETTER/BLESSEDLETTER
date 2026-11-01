"""v1.9.5 课表测试数据：multi / dup / stale / off 四种场景。
   时间相对当前本地时间生成；数据写到 _ttest5，并设置 DataDir。"""
import os, sys, io, datetime, winreg

D = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(D)
DIR = os.path.join(ROOT, "_ttest5")
scen = sys.argv[1] if len(sys.argv) > 1 else "multi"
os.makedirs(DIR, exist_ok=True)

now = datetime.datetime.now().replace(microsecond=0)


def fmt(dt):
    return dt.strftime("%Y-%m-%d %H:%M:%S")


def item(id, title, start, end, extra=""):
    return "\n".join([
        "[item]", "id=%d" % id, "title=" + title, "note=", "rtf=", "link=",
        "start=" + fmt(start), "end=" + fmt(end),
        "hasend=1", "rep=0", "interval=1", "unit=0", "week=0",
        "enabled=1", "done=0", "kind=0", "showimg=0", "imgpct=0",
        "timetable=1", "ttnext=1", "cdunit=0",
        "remind=0", "remlead=0", "remrep=10", "remtimes=1", "sndmode=0",
        "imgn=0", "imgmode=0", "imglayer=0", "color=3B6FE0",
        extra, "[/item]"])


def write(items, notes, ttauto=1):
    txt = "ZScheduleData/4\n" + "\n".join(items) + "\n"
    with open(os.path.join(DIR, "zsched_data.txt"), "wb") as f:
        f.write(b"\xff\xfe" + txt.encode("utf-16-le"))
    lines = ["ttauto=%d" % ttauto] + notes
    with open(os.path.join(DIR, "zsched_notes.txt"), "wb") as f:
        f.write(b"\xff\xfe" + ("\r\n".join(lines) + "\r\n").encode("utf-16-le"))
    for fn in ("zsched_board.txt",):
        p = os.path.join(DIR, fn)
        if os.path.exists(p):
            os.remove(p)


tomorrow = now + datetime.timedelta(days=1)
b_start = tomorrow.replace(hour=8, minute=0, second=0)
c_start = tomorrow.replace(hour=10, minute=0, second=0)
a_start = now + datetime.timedelta(seconds=15)
a_end = a_start + datetime.timedelta(seconds=60)

if scen == "multi":
    items = [
        item(1, "A-当前课", a_start, a_end),
        item(2, "B-明天第一节", b_start, b_start + datetime.timedelta(hours=1)),
        item(3, "C-明天第二节", c_start, c_start + datetime.timedelta(hours=1)),
    ]
    notes = ["id=1 style=1 color=0 top=1 x=200 y=150 w=520 h=420",
             "id=3 style=1 color=0 top=1 x=800 y=150 w=520 h=420"]
    write(items, notes)
elif scen == "dup":
    items = [
        item(1, "Same-副本1", a_start, a_end),
        item(2, "Same-副本2", a_start, a_end),
        item(3, "B-明天第一节", b_start, b_start + datetime.timedelta(hours=1)),
    ]
    notes = ["id=1 style=1 color=0 top=1 x=200 y=150 w=520 h=420",
             "id=2 style=1 color=0 top=1 x=800 y=150 w=520 h=420"]
    write(items, notes)
elif scen == "stale":
    past_s = now - datetime.timedelta(hours=2)
    past_e = now - datetime.timedelta(hours=1)
    items = [
        item(1, "A-已结束", past_s, past_e),
        item(2, "B-明天第一节", b_start, b_start + datetime.timedelta(hours=1)),
    ]
    notes = ["id=1 style=1 color=0 top=1 x=200 y=150 w=520 h=420"]
    write(items, notes)
elif scen == "off":
    past_s = now - datetime.timedelta(hours=2)
    past_e = now - datetime.timedelta(hours=1)
    items = [
        item(1, "A-已结束", past_s, past_e),
        item(2, "B-明天第一节", b_start, b_start + datetime.timedelta(hours=1)),
    ]
    notes = ["id=1 style=1 color=0 top=1 x=200 y=150 w=520 h=420"]
    write(items, notes, ttauto=0)
else:
    raise SystemExit("unknown scenario")

key = winreg.CreateKey(winreg.HKEY_CURRENT_USER, r"Software\Jian")
winreg.SetValueEx(key, "DataDir", 0, winreg.REG_SZ, DIR + "\\")
winreg.CloseKey(key)
print("scenario", scen, "ready; A", fmt(a_start), "->", fmt(a_end))
