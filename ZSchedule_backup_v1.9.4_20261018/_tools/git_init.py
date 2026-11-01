"""用 dulwich（纯 Python 的 git 实现）在工程目录建立仓库并做首次提交。
   本机没有安装 git，所以用这个脚本代替 `git init` + `git commit`；
   以后装了 git 也能直接在同一个 .git 上继续用。

   用法：py _tools\\git_init.py                （建立仓库 + 首次提交）
         py _tools\\git_init.py "提交说明"      （在已有仓库上再提交一次）"""
import os, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
D = os.path.dirname(HERE)               # 工程目录（_tools 的上一级）
os.chdir(D)

from dulwich import porcelain
from dulwich.repo import Repo

AUTHOR = b"ZSchedule Dev <dev@zsched.local>"

FIRST_MSG = """初始提交：笺 v1.3.1（便笺式桌面日程管理器）

纯 C + Win32 API 实现，单文件 exe、绿色免安装，除系统自带的
gdiplus / comctl32 / msftedit 外无任何依赖。

本轮（v1.3.1）内容：
- 修复便笺“收起后无法放回”与误报“便笺都收起来了”（3 处独立缺陷）
- v1.3 正文逐段文字颜色 + 书签形态
- v1.2 统一/自定义数据保存位置 + 导入旧数据
- v1.1 课表模式 + 倒计时显示精度 + 倒数日文案修复
- v1.0 三视图、便笺墙、桌面便笺、富文本、图片、提醒、自启动、历史存档

版本与修复明细见 CHANGELOG.md；实现笔记见 progress.md；使用说明见 README.md。
运行时数据（zsched_*.txt）与构建产物不入库（见 .gitignore）。
"""


def show_log():
    repo = Repo(D)
    print("\n提交历史：")
    for entry in repo.get_walker():
        c = entry.commit
        when = time.strftime("%Y-%m-%d %H:%M", time.localtime(c.commit_time))
        print("  %s  %s  %s" % (c.id.decode()[:8], when,
                                c.message.decode("utf-8", "replace").splitlines()[0]))
    print("已纳入版本控制的文件数：", len(repo.open_index()))


def main():
    msg = sys.argv[1] if len(sys.argv) > 1 else None
    fresh = not os.path.isdir(os.path.join(D, ".git"))
    if fresh:
        porcelain.init(D)
        print("git 仓库已建立:", os.path.join(D, ".git"))
    else:
        print("git 仓库已存在:", os.path.join(D, ".git"))

    added, ignored = porcelain.add(D, paths=["."])
    print("已暂存 %d 个文件" % len(added))
    if ignored:
        print("按 .gitignore 忽略 %d 项" % len(ignored))
        for p in sorted(ignored)[:12]:
            print("   忽略:", p)
        if len(ignored) > 12:
            print("   …共 %d 项" % len(ignored))

    text = msg if msg else FIRST_MSG
    cid = porcelain.commit(D, message=text.encode("utf-8"), author=AUTHOR, committer=AUTHOR)
    print("提交完成:", cid.decode() if isinstance(cid, bytes) else cid)
    show_log()
    return 0


if __name__ == "__main__":
    sys.exit(main())
