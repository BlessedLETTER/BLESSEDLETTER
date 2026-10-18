# -*- coding: utf-8 -*-
"""v1.8 迁移功能测试数据：主数据 + 便笺状态 + 便笺墙 + 一份历史。"""
import os, shutil, datetime, winreg

d = os.path.abspath('_ttest')
if os.path.isdir(d):
    shutil.rmtree(d)
os.makedirs(os.path.join(d, 'zsched_history'), exist_ok=True)

now = datetime.datetime.now()
f = (now + datetime.timedelta(days=2)).strftime('%Y-%m-%d %H:%M:%S')
body = 'ZScheduleData/4\r\n'
for i, t in enumerate(['迁移测试 A', '迁移测试 B', '迁移测试 C'], 1):
    body += ('[item]\r\nid=%d\r\ntitle=%s\r\nstart=%s\r\nend=%s\r\nhasend=0\r\nrep=0\r\n'
             'interval=1\r\nenabled=1\r\ndone=0\r\nkind=0\r\ncolor=3B6FE0\r\n'
             'note=第 %d 条（迁移测试）\r\n[/item]\r\n' % (i, t, f, f, i))
open(os.path.join(d, 'zsched_data.txt'), 'wb').write(b'\xff\xfe' + body.encode('utf-16-le'))
open(os.path.join(d, 'zsched_notes.txt'), 'wb').write(
    b'\xff\xfe' + 'ttauto=1\r\nid=1 style=1 color=0 top=1 x=200 y=180 w=560 h=400\r\n'.encode('utf-16-le'))
open(os.path.join(d, 'zsched_board.txt'), 'wb').write(
    b'\xff\xfe' + 'viewpage=1\r\npages=2\r\nid=2 x=-1 y=-1 w=0 h=0 pin=0 hid=0 page=2\r\n'.encode('utf-16-le'))
open(os.path.join(d, 'zsched_history', 'zsched_20261017_000000.txt'), 'wb').write(
    b'\xff\xfe' + 'ZScheduleData/4\r\n[item]\r\nid=9\r\ntitle=历史快照\r\n[/item]\r\n'.encode('utf-16-le'))

old = None
try:
    k = winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Jian', 0, winreg.KEY_READ)
    try:
        old, _ = winreg.QueryValueEx(k, 'DataDir')
    except FileNotFoundError:
        old = None
    winreg.CloseKey(k)
except FileNotFoundError:
    old = None
open('_ttest/old_datadir.txt', 'w', encoding='utf-8').write('' if old is None else old)
k = winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, r'Software\Jian', 0, winreg.KEY_WRITE)
winreg.SetValueEx(k, 'DataDir', 0, winreg.REG_SZ, d + '\\')
winreg.CloseKey(k)
print('test data ready:', sorted(os.listdir(d)))
