/* ============================================================================
 *  ZSchedule - main.c
 *  程序入口：初始化公共控件 / GDI+ / 数据文件，进入消息循环
 * ==========================================================================*/
#include "zsched.h"

/* WIN32_LEAN_AND_MEAN 挡掉了 ole 头，手工声明需要的 COM 入口 */
__declspec(dllimport) HRESULT WINAPI CoInitializeEx(void *pvReserved, DWORD dwCoInit);
__declspec(dllimport) void    WINAPI CoUninitialize(void);

/* 数据文件位置：命令行第 1 个参数 > 设置里的“数据目录” > exe 所在目录 */
static void resolve_data_path(wchar_t *out, size_t cch, const wchar_t *override)
{
    if (override && override[0]) {
        wcsncpy(out, override, cch - 1);
        out[cch - 1] = 0;
        return;
    }
    wcsncpy(out, app_data_dir(), cch - 1);
    out[cch - 1] = 0;
    wcsncat(out, DATA_FILE_NAME, cch - wcslen(out) - 1);
}

/* 从命令行取参数（宽字符） */
static const wchar_t *first_cmdline_arg(void)
{
    static wchar_t buf[MAX_PATH];
    LPWSTR *argv;
    int argc = 0;
    const wchar_t *res = NULL;

    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return NULL;
    if (argc >= 2 && argv[1][0]) {
        wcsncpy(buf, argv[1], MAX_PATH - 1);
        buf[MAX_PATH - 1] = 0;
        res = buf;
    }
    LocalFree(argv);
    return res;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow)
{
    INITCOMMONCONTROLSEX icc;
    HWND hwnd;
    MSG msg;
    wchar_t dataPath[MAX_PATH];

    (void)hPrev;
    (void)lpCmd;
    g_hInst = hInst;

    /* 高 DPI 感知，避免界面模糊 */
    SetProcessDPIAware();

    /* WIC（WebP 等额外图片格式解码）需要 COM 环境 */
    if (FAILED(CoInitializeEx(NULL, 2 /* COINIT_APARTMENTTHREADED */))) {
        /* 即使失败也不影响主流程，只是无法解码 WebP */
    }

    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_BAR_CLASSES | ICC_DATE_CLASSES | ICC_LISTVIEW_CLASSES |
                ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);

    img_init();
    rt_init();          /* 装载 RichEdit（富文本正文用） */

    model_init();
    app_data_init();          /* 读“数据目录”设置（默认 exe 同目录） */
    resolve_data_path(dataPath, MAX_PATH, first_cmdline_arg());
    model_set_path(dataPath);
    if (!model_load(dataPath)) {
        model_seed_sample();
        model_save(dataPath);
    } else if (model_loaded_legacy()) {
        model_save(dataPath);      /* 旧格式载入后立即升级回写为 v2 */
    }

    main_register_classes(hInst);
    hwnd = main_create();
    if (!hwnd) {
        MessageBoxW(NULL, L"窗口创建失败。", APP_TITLE, MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (main_handle_shortcut(&msg, 0)) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    img_shutdown();
    snd_shutdown();       /* 停掉可能还在播的铃声，并关掉 Media Foundation */
    model_free();
    CoUninitialize();
    return (int)msg.wParam;
}
