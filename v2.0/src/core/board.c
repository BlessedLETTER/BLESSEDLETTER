/* ZSchedule v2.0 · core/board.c
   原子板生命周期注册表：图片 / 富文本 / 纸张 / 声音。
   组合根（main.c）只调用 zs_boards_init/shutdown，不再零散地 init/free。 */
#include "core/types.h"
#include "core/board.h"
#include "core/media/image.h"
#include "core/media/richtext.h"
#include "core/media/sound.h"
#include "core/style/paper.h"

static int  board_paper_init(void)  { paper_init();  return 1; }
static void board_paper_free(void)  { paper_free(); }

static const ZsBoard g_boards[] = {
    { "image",    img_init, NULL },
    { "richtext", rt_init,  NULL },
    { "paper",    board_paper_init, board_paper_free },
    { "sound",    NULL,     snd_shutdown }
};

int zs_boards_init(void)
{
    size_t i;
    for (i = 0; i < sizeof(g_boards) / sizeof(g_boards[0]); i++) {
        if (g_boards[i].init && !g_boards[i].init()) return 0;
    }
    return 1;
}

void zs_boards_shutdown(void)
{
    size_t i = sizeof(g_boards) / sizeof(g_boards[0]);
    while (i > 0) {
        i--;
        if (g_boards[i].shutdown) g_boards[i].shutdown();
    }
}
