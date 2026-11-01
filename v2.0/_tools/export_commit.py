# -*- coding: utf-8 -*-
"""把某个提交的完整文件树导出到指定目录（用于给版本打包独立备份）。
用法：py _tools/export_commit.py <commit-ish 或 HEAD> <目标目录>
注意：dulwich 的 Tree.items() 返回 (name, mode, sha) 三元组；子目录用 mode & 0o040000 判断。"""
import os
import sys

from dulwich.repo import Repo
from dulwich.object_store import tree_lookup_path


def export(repo, sha, dest):
    n = 0

    def walk(tree_sha, path):
        nonlocal n
        tree = repo[tree_sha]
        for name, mode, item_sha in tree.items():
            nm = name.decode("utf-8") if isinstance(name, bytes) else name
            full = os.path.join(path, nm)
            if mode & 0o040000:
                os.makedirs(full, exist_ok=True)
                walk(item_sha, full)
            else:
                os.makedirs(os.path.dirname(full), exist_ok=True)
                data = repo[item_sha].data
                with open(full, "wb") as fp:
                    fp.write(data)
                n += 1

    commit = repo[sha]
    walk(commit.tree, dest)
    return n


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ref, dest = sys.argv[1], sys.argv[2]
    repo = Repo(".")
    sha = repo.head() if ref.upper() == "HEAD" else ref.encode()
    os.makedirs(dest, exist_ok=True)
    n = export(repo, sha, dest)
    print("导出 %d 个文件 → %s（来自 %s）" % (n, dest, sha.decode()[:8]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
