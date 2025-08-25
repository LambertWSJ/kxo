#include <linux/slab.h>

#include "game.h"

static u32 win_patterns[] = {
    /* ROW */
    GEN_O_WINMASK(0, 1, 2),
    GEN_O_WINMASK(1, 2, 3),
    GEN_O_WINMASK(4, 5, 6),
    GEN_O_WINMASK(5, 6, 7),
    GEN_O_WINMASK(8, 9, 10),
    GEN_O_WINMASK(9, 10, 11),
    GEN_O_WINMASK(12, 13, 14),
    GEN_O_WINMASK(13, 14, 15),
    /* COL */
    GEN_O_WINMASK(0, 4, 8),
    GEN_O_WINMASK(1, 5, 9),
    GEN_O_WINMASK(2, 6, 10),
    GEN_O_WINMASK(3, 7, 11),
    GEN_O_WINMASK(4, 8, 12),
    GEN_O_WINMASK(5, 9, 13),
    GEN_O_WINMASK(6, 10, 14),
    GEN_O_WINMASK(7, 11, 15),
    /* PRIMARY */
    GEN_O_WINMASK(0, 5, 10),
    GEN_O_WINMASK(1, 6, 11),
    GEN_O_WINMASK(4, 9, 14),
    GEN_O_WINMASK(5, 10, 15),
    /* SECONDARY */
    GEN_O_WINMASK(2, 5, 8),
    GEN_O_WINMASK(3, 6, 9),
    GEN_O_WINMASK(6, 9, 12),
    GEN_O_WINMASK(7, 10, 13),
};

const line_t lines[4] = {
    {0, 1, 0, 0, BOARD_SIZE, BOARD_SIZE - GOAL + 1},             // ROW
    {1, 0, 0, 0, BOARD_SIZE - GOAL + 1, BOARD_SIZE},             // COL
    {1, 1, 0, 0, BOARD_SIZE - GOAL + 1, BOARD_SIZE - GOAL + 1},  // PRIMARY
    {1, -1, 0, GOAL - 1, BOARD_SIZE - GOAL + 1, BOARD_SIZE},     // SECONDARY
};

char check_win(unsigned int table)
{
    int len = ARRAY_SIZE(win_patterns);
    for (int i = 0; i < len; i++) {
        unsigned int patt = win_patterns[i];
        /* check O is win */
        if ((table & patt) == patt)
            return CELL_O;

        /* check X is win */
        patt <<= 1;
        if ((table & patt) == patt)
            return CELL_X;
    }

    for_each_empty_grid(i, table) return CELL_EMPTY;

    return CELL_D;
}

fixed_point_t calculate_win_value(char win, unsigned char player)
{
    if (win == player)
        return 1U << FIXED_SCALE_BITS;
    if (win == (player ^ CELL_O ^ CELL_X))
        return 0U;
    return 1U << (FIXED_SCALE_BITS - 1);
}

int *available_moves(uint32_t table)
{
    int *moves = kzalloc(N_GRIDS * sizeof(int), GFP_KERNEL);
    int m = 0;
    for_each_empty_grid(i, table) moves[m++] = i;

    if (m < N_GRIDS)
        moves[m] = -1;
    return moves;
}
