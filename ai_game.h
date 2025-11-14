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

static inline fixed_point_t fixed_mul(fixed_point_t a, fixed_point_t b)
{
    return ((s64) a * b) >> FIXED_SCALE_BITS;
}

static inline fixed_point_t fixed_mul_s32(fixed_point_t a, s32 b)
{
    b = ((s64) b * RL_FIXED_1);
    return fixed_mul(a, b);
}
