# -*- coding: utf-8 -*-
"""v1.9 桌面便笺照片交互补丁：只拖不缩放 + 滚轮滚正文 + 便笺手柄优先。"""
import io

p = 'noteview.c'
t = io.open(p, encoding='utf-8').read()


def rep(old, new, cnt=1):
    global t
    assert t.count(old) == cnt, (t.count(old), old[:90])
    t = t.replace(old, new)


# 0) NoteWnd 增加“这次拖动是否真的动过”
rep('''    int    imgHdl;''', '''    int    imgHdl;
    int    imgMoved;          /* v1.9：这次按住的照片有没有真的被拖走（单击只是选中） */''')

# 1) 滚轮：不再缩放照片，统一滚正文
start = t.find('    case WM_MOUSEWHEEL:')
assert start > 0
end = t.find('    case WM_MOUSEMOVE:', start)
assert end > start
new_wheel = '''    case WM_MOUSEWHEEL:
        if (n) {
            /* v1.9：滚轮不再缩放照片（照片按自己的比例自动定大小），在照片上也照样滚正文 */
            Schedule *is = model_by_id(n->id);
            NoteLayout NL;
            int dir = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1;
            if (is) {
                note_layout(n, is, &NL);
                if (NL.maxScroll > 0) {
                    int step = n->lhText * 2;
                    int off;
                    if (step < 8) step = 8;
                    off = NL.scroll + (dir > 0 ? -step : step);
                    if (off > NL.maxScroll) off = NL.maxScroll;
                    if (off < 0) off = 0;
                    if (off != NL.scroll) {
                        n->nscr = off;
                        note_repaint(n);
                    }
                }
            }
        }
        return 0;

'''
t = t[:start] + new_wheel + t[end:]

# 2) 悬停：便笺手柄优先；照片上只显示“可拖动”
old_hover = '''                {
                    int ihdl = 0;
                    int onImg = note_img_hit(n, pt, &ihdl, NULL, NULL);
                    if (onImg != n->imgHot) {
                        n->imgHot = onImg;
                        note_repaint(n);
                    }
                    if (onImg) {
                        if (ihdl == (IMG_HDL_L | IMG_HDL_T) || ihdl == (IMG_HDL_R | IMG_HDL_B))
                            SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
                        else if (ihdl == (IMG_HDL_R | IMG_HDL_T) || ihdl == (IMG_HDL_L | IMG_HDL_B))
                            SetCursor(LoadCursorW(NULL, IDC_SIZENESW));
                        else if (ihdl & (IMG_HDL_L | IMG_HDL_R))
                            SetCursor(LoadCursorW(NULL, IDC_SIZEWE));
                        else if (ihdl & (IMG_HDL_T | IMG_HDL_B))
                            SetCursor(LoadCursorW(NULL, IDC_SIZENS));
                        else
                            SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                        return 0;
                    }
                }'''
new_hover = '''                if (note_hit_grip(n, pt)) {        /* 便笺右下角的缩放手柄优先 */
                    SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
                    return 0;
                }
                {
                    int onImg = note_img_hit(n, pt, NULL, NULL, NULL);
                    if (onImg != n->imgHot) {
                        n->imgHot = onImg;
                        note_repaint(n);
                    }
                    if (onImg) {                   /* 照片上：可拖动（双击进编辑） */
                        SetCursor(LoadCursorW(NULL, IDC_SIZEALL));
                        return 0;
                    }
                }'''
rep(old_hover, new_hover)

# 3) 按下：便笺手柄优先；照片 = 选中 + 拖动
old_down = '''            {
                int ihdl = 0, iimg = -1;
                RECT icontent;
                NoteLayout NL;
                Schedule *is = model_by_id(n->id);
                if (is) note_layout(n, is, &NL);
                if (note_img_hit(n, pt, &ihdl, &icontent, &iimg)) {
                    if (is) is->imgMode = IMGM_FLOAT;      /* 一拖就转为浮动 */
                    n->mode = (ihdl == 0) ? 3 : 4;
                    n->imgSelIdx = (iimg >= 0 && iimg < MAX_IMGS) ? iimg : 0;
                    n->imgHdl = ihdl;
                    n->imgPt0 = pt;
                    n->imgBox0 = (iimg >= 0 && NL.imgBase[iimg].right > NL.imgBase[iimg].left)
                                 ? NL.imgBase[iimg] : NL.img[iimg];
                    n->imgContent0 = icontent;
                    n->dragRc = n->rc;
                    SetCapture(hwnd);
                    note_repaint(n);
                    return 0;
                }
            }'''
new_down = '''            if (note_hit_grip(n, pt)) {       /* 便笺自己的右下角手柄优先于照片 */
                n->mode = 2;
                n->dragged = 0;
                n->dragRc = n->rc;
                GetCursorPos(&n->dragOrg);
                SetCapture(hwnd);
                return 0;
            }
            {
                int iimg = -1;
                RECT icontent;
                NoteLayout NL;
                Schedule *is = model_by_id(n->id);
                if (is) note_layout(n, is, &NL);
                if (note_img_hit(n, pt, NULL, &icontent, &iimg)) {
                    /* v1.9：单击选中 + 拖动（只改位置）；双击进编辑窗 */
                    if (is) is->imgMode = IMGM_FLOAT;
                    n->mode = 3;
                    n->imgSelIdx = (iimg >= 0 && iimg < MAX_IMGS) ? iimg : 0;
                    n->imgHdl = 0;
                    n->imgHotIdx = n->imgSelIdx;
                    n->imgPt0 = pt;
                    n->imgBox0 = (iimg >= 0 && NL.imgBase[iimg].right > NL.imgBase[iimg].left)
                                 ? NL.imgBase[iimg] : NL.img[iimg];
                    n->imgContent0 = icontent;
                    n->imgMoved = 0;
                    n->dragRc = n->rc;
                    SetCapture(hwnd);
                    note_repaint(n);
                    return 0;
                }
            }'''
rep(old_down, new_down)

# 4) 拖动照片：只记位置
old_move = '''            } else if (n->mode == 3 || n->mode == 4) {
                Schedule *is = model_by_id(n->id);
                RECT box;
                if (!is) { n->mode = 0; return 0; }
                box = n->imgBox0;
                {
                    int dx = pt.x - n->imgPt0.x;
                    int dy = pt.y - n->imgPt0.y;
                    if (n->mode == 3) {
                        OffsetRect(&box, dx, dy);
                    } else {
                        if (n->imgHdl & IMG_HDL_L) box.left += dx;
                        if (n->imgHdl & IMG_HDL_R) box.right += dx;
                        if (n->imgHdl & IMG_HDL_T) box.top += dy;
                        if (n->imgHdl & IMG_HDL_B) box.bottom += dy;
                    }
                    note_img_clamp(n, &n->imgContent0, &box);
                    img_box_store(&n->imgContent0, &box, is);
                    {
                        int kk = (n->imgSelIdx >= 0 && n->imgSelIdx < MAX_IMGS) ? n->imgSelIdx : 0;
                        is->imgB[kk][0] = is->imgX; is->imgB[kk][1] = is->imgY;
                        is->imgB[kk][2] = is->imgW; is->imgB[kk][3] = is->imgH;
                    }
                }
                note_repaint(n);
                return 0;
            } else if (n->mode == 2) {'''
new_move = '''            } else if (n->mode == 3) {
                Schedule *is = model_by_id(n->id);
                RECT box;
                int dx, dy;
                if (!is) { n->mode = 0; return 0; }
                box = n->imgBox0;
                dx = pt.x - n->imgPt0.x;
                dy = pt.y - n->imgPt0.y;
                if (dx || dy) n->imgMoved = 1;
                OffsetRect(&box, dx, dy);
                note_img_clamp(n, &n->imgContent0, &box);
                {   /* 只记住位置；大小始终按照片自身比例（v1.9） */
                    int cw = n->imgContent0.right - n->imgContent0.left;
                    int ch = n->imgContent0.bottom - n->imgContent0.top;
                    int kk = (n->imgSelIdx >= 0 && n->imgSelIdx < MAX_IMGS) ? n->imgSelIdx : 0;
                    is->imgMode = IMGM_FLOAT;
                    if (cw > 0) {
                        int fx = (box.left - n->imgContent0.left) * 1000 / cw;
                        if (fx < 0) fx = 0;
                        if (fx > 1000) fx = 1000;
                        is->imgB[kk][0] = fx;
                    }
                    if (ch > 0) {
                        int fy = (box.top - n->imgContent0.top) * 1000 / ch;
                        if (fy < 0) fy = 0;
                        if (fy > 1000) fy = 1000;
                        is->imgB[kk][1] = fy;
                    }
                }
                note_repaint(n);
                return 0;
            } else if (n->mode == 2) {'''
rep(old_move, new_move)

# 5) 松手：只有真的移动过才存盘/提示；单击 = 选中
old_up = '''            if (n->mode == 3 || n->mode == 4) {
                int mode = n->mode;
                n->mode = 0;
                n->imgHdl = 0;
                ReleaseCapture();
                model_save(model_path());
                main_set_status(mode == 3
                    ? L"\\u7167\\u7247\\u5df2\\u79fb\\u5230\\u65b0\\u4f4d\\u7f6e\\uff08\\u53f3\\u952e\\u53ef\\u590d\\u4f4d\\uff09"
                    : L"\\u7167\\u7247\\u5c3a\\u5bf8\\u5df2\\u8c03\\u6574\\uff08\\u53f3\\u952e\\u53ef\\u590d\\u4f4d\\uff09");
                note_repaint(n);
                return 0;
            }'''
new_up = '''            if (n->mode == 3 || n->mode == 4) {
                int moved = n->imgMoved;
                n->mode = 0;
                n->imgHdl = 0;
                n->imgMoved = 0;
                ReleaseCapture();
                if (moved) {
                    model_save(model_path());
                    main_set_status(L"照片已移到新位置（右键「照片 → 位置与大小复位」可还原）");
                } else {
                    main_set_status(L"已选中照片：拖动可移动位置，双击进入编辑窗");
                }
                note_repaint(n);
                return 0;
            }'''
rep(old_up, new_up)

# 6) note_img_hit：不再回手柄位（照片不可缩放）
old_hit = '''    tol = NSC(n, IMG_HDL) / 2 + NSC(n, 3);
    for (i = NL.imgCount - 1; i >= 0; i--) {
        RECT ib = NL.img[i];
        int hdl;
        if (ib.right <= ib.left || ib.bottom <= ib.top) continue;
        hdl = img_handle_at(&ib, pt.x, pt.y, tol);
        if (hdl || (pt.x >= ib.left && pt.x <= ib.right &&
                    pt.y >= ib.top && pt.y <= ib.bottom)) {
            if (phdl) *phdl = hdl;'''
new_hit = '''    (void)tol;
    for (i = NL.imgCount - 1; i >= 0; i--) {
        RECT ib = NL.img[i];
        if (ib.right <= ib.left || ib.bottom <= ib.top) continue;
        if (pt.x >= ib.left && pt.x <= ib.right &&
            pt.y >= ib.top && pt.y <= ib.bottom) {
            if (phdl) *phdl = 0;           /* v1.9：照片没有缩放手柄，只有“移动” */'''
rep(old_hit, new_hit)

# 7) 右键菜单文案：照片（可拖动、可缩放） -> 照片（拖动调整位置）
rep('''        AppendMenuW(m, MF_POPUP | (hasImg ? 0 : MF_GRAYED), (UINT_PTR)mi,
                    L"\\u7167\\u7247\\uff08\\u53ef\\u62d6\\u52a8\\u3001\\u53ef\\u7f29\\u653e\\uff09(&Z)");''',
'''        AppendMenuW(m, MF_POPUP | (hasImg ? 0 : MF_GRAYED), (UINT_PTR)mi,
                    L"照片（拖动调整位置；双击可编辑）(&Z)");''')

io.open(p, 'w', encoding='utf-8', newline='').write(t)
print('noteview v1.9 patch ok')
