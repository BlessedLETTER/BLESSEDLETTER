#ifndef ZS_CORE_BOARD_H
#define ZS_CORE_BOARD_H

#include "core/types.h"

/* 原子板生命周期注册表 */

typedef struct ZsBoard {
    const char *name;
    int  (*init)(void);
    void (*shutdown)(void);
} ZsBoard;
int  zs_boards_init(void);
void zs_boards_shutdown(void);

#endif /* ZS_CORE_BOARD_H */
