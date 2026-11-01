/* 临时探针：验证 model_load 是否正确读出 sndmode / sndfile */
#include <stdio.h>
#include "../model.c"

const COLORREF g_palette[8] = {
    RGB(59, 111, 224), RGB(22, 163, 106), RGB(235, 148, 40), RGB(219, 74, 74),
    RGB(124, 92, 224), RGB(13, 148, 168), RGB(214, 51, 132), RGB(112, 122, 136)
};
int img_is_image(const wchar_t *path) { (void)path; return 0; }

int main(void)
{
    FILE *o = fopen("_tools\\loadprobe_out.txt", "wb");
    wchar_t path[MAX_PATH];
    int i;
    if (!o) return 1;
    fputs("\xEF\xBB\xBF", o);
    MultiByteToWideChar(CP_ACP, 0, "_ttest\\zsched_data.txt", -1, path, MAX_PATH);
    model_init();
    model_set_path(path);
    fprintf(o, "loaded items = %d\n", model_load(path));
    for (i = 0; i < model_count(); i++) {
        Schedule *s = model_at(i);
        char t[256], f[512];
        WideCharToMultiByte(CP_UTF8, 0, s->title, -1, t, 256, NULL, NULL);
        WideCharToMultiByte(CP_UTF8, 0, s->sndFile, -1, f, 512, NULL, NULL);
        fprintf(o, "  id=%d kind=%d title=%s remind=%d sndMode=%d sndFile=[%s] pathExists=%d\n",
                s->id, s->kind, t, s->remind, s->sndMode, f,
                GetFileAttributesW(s->sndFile) != INVALID_FILE_ATTRIBUTES);
    }
    fclose(o);
    model_free();
    return 0;
}
