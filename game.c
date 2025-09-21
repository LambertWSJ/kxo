#include <linux/slab.h>

#include "game.h"

static const int winpat_len = WIN_PATT_LEN(BOARD_SIZE, GOAL);
static u32 win_patterns[WIN_PATT_LEN(BOARD_SIZE, GOAL)];
u8 xo_segment_lines[WIN_PATT_LEN(BOARD_SIZE, GOAL)][GOAL];

const line_t lines[4] = {
    {0, 1, 0, 0, BOARD_SIZE, BOARD_SIZE - GOAL + 1},             // ROW
    {1, 0, 0, 0, BOARD_SIZE - GOAL + 1, BOARD_SIZE},             // COL
    {1, 1, 0, 0, BOARD_SIZE - GOAL + 1, BOARD_SIZE - GOAL + 1},  // PRIMARY
    {1, -1, 0, GOAL - 1, BOARD_SIZE - GOAL + 1, BOARD_SIZE},     // SECONDARY
};

static bool detect_empty_cell(uint32_t board)
{
    uint32_t lo = board & 0x55555555;
    uint32_t hi = board & 0xAAAAAAAA;
    return (lo | (hi >> 1)) != 0x55555555;
}

char check_win(unsigned int table)
{
    if (!table)
        return CELL_EMPTY;


    for (int i = 0; i < winpat_len; i++) {
        unsigned int patt = win_patterns[i];
        /* check O is win */
        if ((table & patt) == patt)
            return CELL_O;

        /* check X is win */
        patt <<= 1;
        if ((table & patt) == patt)
            return CELL_X;
    }

    return detect_empty_cell(table) ? CELL_EMPTY : CELL_D;
}

void fill_win_patterns(void)
{
    for (int i_line = 0, w = 0; i_line < BOARD_SIZE; ++i_line) {
        line_t line = lines[i_line];
        for (int i = line.i_lower_bound; i < line.i_upper_bound; ++i) {
            for (int j = line.j_lower_bound; j < line.j_upper_bound; ++j) {
                xo_segment_lines[w][0] = GET_INDEX(i, j);
                for (int k = 1; k < GOAL; k++) {
                    int id =
                        GET_INDEX(i + k * line.i_shift, j + k * line.j_shift);
                    xo_segment_lines[w][k] = id;
                }
                win_patterns[w] = GEN_O_WINMASK(xo_segment_lines[w][0],
                                                xo_segment_lines[w][1],
                                                xo_segment_lines[w][2]);
                w++;
            }
        }
    }
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
