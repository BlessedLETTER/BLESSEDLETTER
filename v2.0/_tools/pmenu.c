/* 测试辅助：把某进程主窗口的菜单树打印出来（勾选/禁用标记 + id + 文本，UTF-8） */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static DWORD g_pid;
static HWND  g_main;

static BOOL CALLBACK cb(HWND h, LPARAM lp)
{
    DWORD pid = 0;
    wchar_t cls[256];
    (void)lp;
    GetWindowThreadProcessId(h, &pid);
    if (pid != g_pid) return TRUE;
    GetClassNameW(h, cls, 256);
    if (wcscmp(cls, L"ZScheduleMainWnd") == 0) { g_main = h; return FALSE; }
    return TRUE;
}

static void dump_menu(HMENU m, int depth)
{
    int n = GetMenuItemCount(m), i;
    wchar_t txt[256];
    for (i = 0; i < n; i++) {
        MENUITEMINFOW mi;
        int k;
        char u8[2048];
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_ID | MIIM_STRING | MIIM_STATE | MIIM_SUBMENU;
        mi.dwTypeData = txt;
        mi.cch = 255;
        if (!GetMenuItemInfoW(m, (UINT)i, TRUE, &mi)) continue;
        txt[255] = 0;
        WideCharToMultiByte(CP_UTF8, 0, txt, -1, u8, 2048, NULL, NULL);
        for (k = 0; k < depth; k++) printf("    ");
        printf("%s%s id=%u  %s\n",
               (mi.fState & MFS_CHECKED) ? "[v] " : "",
               (mi.fState & (MFS_DISABLED | MFS_GRAYED)) ? "[x] " : "",
               (unsigned)mi.wID, u8);
        if (mi.hSubMenu) dump_menu(mi.hSubMenu, depth + 1);
    }
}

int main(int argc, char **argv)
{
    SetProcessDPIAware();
    if (argc < 2) return 2;
    g_pid = (DWORD)strtoul(argv[1], NULL, 10);
    EnumWindows(cb, 0);
    if (!g_main) { printf("main window not found\n"); return 1; }
    dump_menu(GetMenu(g_main), 0);
    return 0;
}
