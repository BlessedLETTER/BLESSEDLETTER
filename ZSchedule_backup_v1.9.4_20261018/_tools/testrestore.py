# -*- coding: utf-8 -*-
"""还原测试环境：把 DataDir 恢复成测试前的样子（原来没有这个值就删掉）。"""
import winreg

with open("_ttest/old_datadir.txt", "r", encoding="utf-8") as fp:
    old = fp.read().strip()

k = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Jian", 0, winreg.KEY_ALL_ACCESS)
try:
    cur, _t = winreg.QueryValueEx(k, "DataDir")
except FileNotFoundError:
    cur = None
print("now DataDir =", cur)
if old:
    winreg.SetValueEx(k, "DataDir", 0, winreg.REG_SZ, old)
    print("restored ->", old)
else:
    try:
        winreg.DeleteValue(k, "DataDir")
        print("deleted DataDir (原来没有这个值)")
    except FileNotFoundError:
        print("DataDir 已不存在")
winreg.CloseKey(k)

k = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Jian", 0, winreg.KEY_READ)
try:
    v, _t = winreg.QueryValueEx(k, "DataDir")
    print("verify: DataDir =", v)
except FileNotFoundError:
    print("verify: DataDir 不存在 ✓")
winreg.CloseKey(k)
