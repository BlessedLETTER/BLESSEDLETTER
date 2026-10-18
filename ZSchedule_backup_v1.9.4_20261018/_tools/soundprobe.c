/* 临时探针：验证 sound.c 的播放通道（MF / MCI / 系统程序）。
   用法：soundprobe.exe <文件>...   结果写 _tools\soundprobe_out.txt */
#include <stdio.h>
#include "../sound.c"

const COLORREF g_palette[8] = {
    RGB(59, 111, 224), RGB(22, 163, 106), RGB(235, 148, 40), RGB(219, 74, 74),
    RGB(124, 92, 224), RGB(13, 148, 168), RGB(214, 51, 132), RGB(112, 122, 136)
};
int img_is_image(const wchar_t *path) { (void)path; return 0; }

int main(int argc, char **argv)
{
    FILE *o = fopen("_tools\\soundprobe_out.txt", "wb");
    int i;
    if (!o) return 1;
    fputs("\xEF\xBB\xBF", o);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    for (i = 1; i < argc; i++) {
        wchar_t wpath[MAX_PATH];
        int ch;
        /* 探针的 argv 是 ANSI（GBK）来的，用 CP_ACP 转 */
        MultiByteToWideChar(CP_ACP, 0, argv[i], -1, wpath, MAX_PATH);
        ch = snd_play_file(wpath);
        fprintf(o, "play %s -> channel %d%s\n", argv[i], ch,
                ch == 1 ? " (Media Foundation)" : ch == 2 ? " (MCI)"
              : ch == 3 ? " (ShellExecute)" : " (FAILED)");
        if (ch) {
            int t;
            for (t = 0; t < 10; t++) {    /* 观察 1 秒，确认没崩 */
                Sleep(100);
            }
            fprintf(o, "  still active=%d\n", snd_is_active());
            snd_stop();
            fprintf(o, "  stopped, active=%d\n", snd_is_active());
        }
    }
    snd_shutdown();
    CoUninitialize();
    fclose(o);
    return 0;
}
