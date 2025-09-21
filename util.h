#pragma once

#include <linux/types.h>
#include "game.h"

static inline int eval_line_segment_score(unsigned int table,
                                          char player,
                                          int i)
{
    extern u8 xo_segment_lines[WIN_PATT_LEN(BOARD_SIZE, GOAL)][GOAL];

    int score = 0;
    for (int j = 0; j < GOAL; j++) {
        unsigned char curr = TABLE_GET_CELL(table, xo_segment_lines[i][j]);
        if (curr == player) {
            if (score < 0)
                return 0;
            if (score)
                score *= 10;
            else
                score = 1;
        } else if (curr != CELL_EMPTY) {
            if (score > 0)
                return 0;
            if (score)
                score *= 10;
            else
                score = -1;
        }
    }
    return score;
}

static inline int get_score(const unsigned int table, char player)
{
    int score = 0;
    const int seg_sz = WIN_PATT_LEN(BOARD_SIZE, GOAL);
    for (int i = 0; i < seg_sz; i++)
        score += eval_line_segment_score(table, i, player);

    return score;
}
