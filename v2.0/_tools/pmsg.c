/* 测试辅助：发消息
   pmsg.exe <pid> <class> <msg> <wparam> <lparam>     发到该类的窗口
   pmsg.exe main <pid> <cmd> [lparam] [hi]            给主窗口发 WM_COMMAND */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static DWORD g_pid;
static wchar_t g_cls[256];
static HWND g_hit;

static BOOL CALLBACK cb(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    wchar_t cls[256];
    (void)lp;
    GetWindowThreadProcessId(h, &pid);
    if (pid != g_pid) return TRUE;
    GetClassNameW(h, cls, 256);
    if (_wcsicmp(cls, g_cls) == 0 && IsWindowVisible(h)) { g_hit = h; return FALSE; }
    return TRUE;
}

int main(int argc, char **argv)
{
    SetProcessDPIAware();   /* 否则 post 出去的鼠标坐标会被系统按 DPI 缩放 */
    /* pmsg.exe get <pid> <class> <ctrlId>  读子控件文本 */
    if (argc >= 5 && _stricmp(argv[1], "get") == 0) {
        HWND ctl;
        wchar_t back[512];
        char u8[1024];
        g_pid = (DWORD)strtoul(argv[2], NULL, 10);
        MultiByteToWideChar(CP_UTF8, 0, argv[3], -1, g_cls, 256);
        EnumWindows(cb, 0);
        if (!g_hit) { printf("parent not found\n"); return 1; }
        ctl = GetDlgItem(g_hit, (int)strtol(argv[4], NULL, 10));
        if (!ctl) { printf("ctrl not found\n"); return 1; }
        back[0] = 0;
        GetWindowTextW(ctl, back, 512);
        WideCharToMultiByte(CP_UTF8, 0, back, -1, u8, 1024, NULL, NULL);
        printf("get ctl %s = [%s] hwnd=%p\n", argv[4], u8, (void *)ctl);
        return 0;
    }
    /* pmsg.exe ctl <pid> <class> <ctrlId> <text...>  设置子控件文本 */
    if (argc >= 5 && _stricmp(argv[1], "ctl") == 0) {
        HWND ctl;
        wchar_t txt[512];
        g_pid = (DWORD)strtoul(argv[2], NULL, 10);
        MultiByteToWideChar(CP_UTF8, 0, argv[3], -1, g_cls, 256);
        EnumWindows(cb, 0);
        if (!g_hit) { printf("parent not found\n"); return 1; }
        ctl = GetDlgItem(g_hit, (int)strtol(argv[4], NULL, 10));
        if (!ctl) { printf("ctrl not found\n"); return 1; }
        MultiByteToWideChar(CP_UTF8, 0, argc > 5 ? argv[5] : "", -1, txt, 512);
        SetWindowTextW(ctl, txt);
        {
            wchar_t back[512];
            wchar_t cls[128];
            char u8[1024], c8[256];
            GetWindowTextW(ctl, back, 512);
            GetClassNameW(ctl, cls, 128);
            WideCharToMultiByte(CP_UTF8, 0, back, -1, u8, 1024, NULL, NULL);
            WideCharToMultiByte(CP_UTF8, 0, cls, -1, c8, 256, NULL, NULL);
            printf("set ctl %s = %s -> 回读=%s class=%s hwnd=%p\n",
                   argv[4], argv[5], u8, c8, (void *)ctl);
        }
        return 0;
    }
    if (argc >= 6 && _stricmp(argv[1], "main") != 0) {
        UINT msg;
        WPARAM wp;
        LPARAM lp2;
        g_pid = (DWORD)strtoul(argv[1], NULL, 10);
        MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, g_cls, 256);
        msg = (UINT)strtoul(argv[3], NULL, 0);
        wp = (WPARAM)strtoul(argv[4], NULL, 0);
        lp2 = (LPARAM)strtol(argv[5], NULL, 0);
        EnumWindows(cb, 0);
        if (!g_hit) { printf("window not found\n"); return 1; }
        PostMessageW(g_hit, msg, wp, lp2);
        printf("sent msg=%u wp=%u lp=%ld\n", msg, (unsigned)wp, (long)lp2);
        return 0;
    }
    if (argc >= 3) {
        UINT cmd;
        LONG_PTR lp = 0;
        WPARAM hi = 0;
        BOOL ok;
        g_pid = (DWORD)strtoul(argv[2], NULL, 10);
        cmd = (UINT)strtoul(argv[3], NULL, 10);
        if (argc > 4) lp = (LONG_PTR)strtol(argv[4], NULL, 10);
        if (argc > 5) hi = (WPARAM)strtoul(argv[5], NULL, 10);
        wcscpy(g_cls, L"ZScheduleMainWnd");
        EnumWindows(cb, 0);
        if (!g_hit) { printf("main window not found\n"); return 1; }
        ok = PostMessageW(g_hit, WM_COMMAND, MAKEWPARAM(cmd, hi), lp);
        printf("posted %u hi=%u lp=%ld ok=%d\n", cmd, (unsigned)hi, (long)lp, ok ? 1 : 0);
        return ok ? 0 : 3;
    }
    return 2;
}
