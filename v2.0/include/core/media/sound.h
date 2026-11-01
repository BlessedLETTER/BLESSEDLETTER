#ifndef ZS_CORE_SOUND_H
#define ZS_CORE_SOUND_H

#include "core/types.h"

/* 声音播放（sound.c）：返回实际走的通道 */
enum { SNDV_NONE = 0, SNDV_MF = 1, SNDV_MCI = 2, SNDV_SHELL = 3 };
int    snd_play_file(const wchar_t *path);   /* 1=Media Foundation 2=MCI 3=交给系统程序 0=失败 */
void   snd_stop(void);
int    snd_is_active(void);
void   snd_shutdown(void);

void snd_open_file(const wchar_t *path);
const wchar_t *snd_desc(const Schedule *s, wchar_t *buf, size_t cch);

#endif /* ZS_CORE_SOUND_H */
