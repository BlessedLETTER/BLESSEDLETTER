# -*- coding: utf-8 -*-
import io
lines = io.open('ui_main.c', encoding='utf-8').read().splitlines()
for i, l in enumerate(lines, 1):
    if 'sched_anniv_elapsed' in l:
        for j in range(max(0, i - 5), min(i + 4, len(lines))):
            print('%d: %s' % (j + 1, lines[j]))
        print('-----')
