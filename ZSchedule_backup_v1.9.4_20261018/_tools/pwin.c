/* 测试辅助：列出某进程顶层窗口（类名|标题|x,y,w,h，真实像素）
   用法：pwin.exe <pid> [类名|*] [focus|all]
     focus  = 把该类名窗口置顶并聚焦（截图前用）
     all    = 连不可见窗口一起列（排查“窗口到底有没有创建”） */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static DWORD g_pid;
static wchar_t g_cls[256];
static int g_focus, g_all;

static BOOL CALLBACK cb(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    wchar_t cls[256], txt[256], line[1024];
    RECT r;
    (void)lp;
    GetWindowThreadProcessId(h, &pid);
    if (pid != g_pid) return TRUE;
    if (!g_all && !IsWindowVisible(h)) return TRUE;
    GetClassNameW(h, cls, 256);
    GetWindowTextW(h, txt, 256);
    if (g_cls[0] && _wcsicmp(g_cls, L"*") != 0 && _wcsicmp(cls, g_cls) != 0) return TRUE;
    GetWindowRect(h, &r);
    if (g_cls[0] && _wcsicmp(g_cls, L"*") != 0 && _wcsicmp(cls, g_cls) == 0 && g_focus) {
        SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ShowWindow(h, SW_SHOW);
        SetForegroundWindow(h);
        printf("focused ok\n");
    }
    _snwprintf(line, 1024, L"%s|%s|%d,%d,%d,%d%s", cls, txt, (int)r.left, (int)r.top,
               (int)(r.right - r.left), (int)(r.bottom - r.top),
               IsWindowVisible(h) ? L"" : L"  (隐藏)");
    {
        char u8[4096];
        WideCharToMultiByte(CP_UTF8, 0, line, -1, u8, 4096, NULL, NULL);
        printf("%s\n", u8);
    }
    return TRUE;
}

int main(int argc, char **argv)
{
    SetProcessDPIAware();
    if (argc < 2) return 2;
    g_pid = (DWORD)strtoul(argv[1], NULL, 10);
    g_cls[0] = 0;
    if (argc > 2) MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, g_cls, 256);
    if (argc > 3) {
        if (_stricmp(argv[3], "focus") == 0) g_focus = 1;
        if (_stricmp(argv[3], "all") == 0) g_all = 1;
        if (_stricmp(argv[3], "allfocus") == 0) { g_all = 1; g_focus = 1; }
    }
    EnumWindows(cb, 0);
    return 0;
}
