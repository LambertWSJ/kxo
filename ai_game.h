#pragma once

#include <linux/list.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include "game.h"

typedef int (*ai_alg)(unsigned int table, char player);

struct ai_avg {
    s64 nsecs_o;
    s64 nsecs_x;
    u64 load_avg_o;
    u64 load_avg_x;
};

struct ai_game {
    struct xo_table xo_tlb;
    char turn;
    u8 finish;
    struct mutex lock;
    struct work_struct ai_one_work;
    struct work_struct ai_two_work;
    struct work_struct drawboard_work;
};
