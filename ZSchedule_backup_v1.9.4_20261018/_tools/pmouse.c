/* 测试辅助：在指定类名窗口上模拟左键单击 / 拖拽
   pmouse.exe <pid> <class> click
   pmouse.exe <pid> <class> drag <dx> <dy> */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static DWORD g_pid;
static wchar_t g_cls[256];
static HWND g_hit;
static RECT g_rc;

static BOOL CALLBACK cb(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    wchar_t cls[256];
    (void)lp;
    GetWindowThreadProcessId(h, &pid);
    if (pid != g_pid) return TRUE;
    GetClassNameW(h, cls, 256);
    if (_wcsicmp(cls, g_cls) == 0 && IsWindowVisible(h)) {
        g_hit = h;
        GetWindowRect(h, &g_rc);
        return FALSE;
    }
    return TRUE;
}

int main(int argc, char **argv)
{
    int cx, cy, dx = 0, dy = 0;
    if (argc < 4) return 2;
    SetProcessDPIAware();
    g_pid = (DWORD)strtoul(argv[1], NULL, 10);
    MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, g_cls, 256);
    /* pmouse.exe <pid> <class> clickctl <ctrlId>  —— 点某个子控件的中心 */
    if (_stricmp(argv[3], "clickctl") == 0 && argc >= 5) {
        HWND ctl;
        RECT r;
        EnumWindows(cb, 0);
        if (!g_hit) { printf("window not found\n"); return 1; }
        ctl = GetDlgItem(g_hit, (int)strtol(argv[4], NULL, 10));
        if (!ctl) { printf("ctrl not found\n"); return 1; }
        GetWindowRect(ctl, &r);
        SetCursorPos((r.left + r.right) / 2, (r.top + r.bottom) / 2);
        Sleep(200);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Sleep(80);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        printf("clicked ctl %s at %d,%d\n", argv[4], (r.left + r.right) / 2, (r.top + r.bottom) / 2);
        return 0;
    }
    if (argc > 4) dx = atoi(argv[4]);
    if (argc > 5) dy = atoi(argv[5]);
    EnumWindows(cb, 0);
    if (!g_hit) { printf("window not found\n"); return 1; }
    cx = (g_rc.right - g_rc.left) / 2;
    cy = (g_rc.bottom - g_rc.top) / 2;
    SetCursorPos(g_rc.left + cx, g_rc.top + cy);
    Sleep(120);
    PostMessageW(g_hit, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(cx, cy));
    Sleep(150);
    if (_stricmp(argv[3], "drag") == 0) {
        SetCursorPos(g_rc.left + cx + dx, g_rc.top + cy + dy);
        Sleep(120);
        PostMessageW(g_hit, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(cx + dx, cy + dy));
        Sleep(150);
    }
    PostMessageW(g_hit, WM_LBUTTONUP, 0, MAKELPARAM(cx + dx, cy + dy));
    printf("%s rect=%d,%d,%d,%d delta=%d,%d\n", argv[3],
           (int)g_rc.left, (int)g_rc.top, (int)(g_rc.right - g_rc.left),
           (int)(g_rc.bottom - g_rc.top), dx, dy);
    return 0;
}
