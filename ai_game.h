#pragma once

#include <linux/list.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include "game.h"

struct ai_game {
    struct xo_table xo_tlb;
    char turn;
    u8 finish;
    struct mutex lock;
    struct work_struct ai_one_work;
    struct work_struct ai_two_work;
    struct work_struct drawboard_work;
};
