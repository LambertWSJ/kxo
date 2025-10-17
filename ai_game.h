#pragma once

#include <linux/list.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include "game.h"


#define SET_RECORD_CELL(reocrd, id, cell)                                 \
    ({                                                                    \
        __typeof__(id) _id = id;                                          \
        (record & ~(3ull << (_id * 4)) | ((uint64_t) cell << (_id * 4))); \
    })

#define GET_RECORD_CELL(reocrd, id) \
    (record & (0xfull << ((uint64_t) id * 4))) >> ((uint64_t) id * 4)


struct ai_game {
    struct xo_table xo_tlb;
    char turn;
    u8 finish;
    struct mutex lock;
    struct work_struct ai_one_work;
    struct work_struct ai_two_work;
    struct work_struct drawboard_work;
};
