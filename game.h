#pragma once

#define BOARD_SIZE 4
#define GOAL 3
#define ALLOW_EXCEED 1
#define N_GRIDS (BOARD_SIZE * BOARD_SIZE)
#define N_GAMES 8
#define GET_INDEX(i, j) ((i) * (BOARD_SIZE) + (j))
#define GET_COL(x) ((x) % BOARD_SIZE)
#define GET_ROW(x) ((x) / BOARD_SIZE)
#define CELL_EMPTY 0u
#define CELL_O 1u
#define CELL_X 2u
#define CELL_D 3u

#define GEN_O_WINMASK(a, b, c) \
    ((CELL_O << (a * 2)) | (CELL_O << (b * 2)) | (CELL_O << (c * 2)))

#define VAL_SET_CELL(table, pos, cell) \
    ((table & ~((3u) << ((pos) * 2))) | ((cell) << ((pos) * 2)))

#define TABLE_GET_CELL(table, pos)          \
    ({                                      \
        __typeof__(pos) _pos = (pos) * 2;   \
        ((table & (0b11 << _pos)) >> _pos); \
    })

#define for_each_empty_grid(i, table) \
    for (int i = 0; i < N_GRIDS; i++) \
        if (TABLE_GET_CELL(table, i) == CELL_EMPTY)

typedef struct {
    int i_shift, j_shift;
    int i_lower_bound, j_lower_bound, i_upper_bound, j_upper_bound;
} line_t;

struct xo_table {
    int id;
    unsigned int table;
};

/* Self-defined fixed-point type, using last 10 bits as fractional bits,
 * starting from lsb */
#define FIXED_SCALE_BITS 8
#define FIXED_MAX (~0U)
#define FIXED_MIN (0U)
#define GET_SIGN(x) ((x) & (1U << 31))
#define SET_SIGN(x) ((x) | (1U << 31))
#define CLR_SIGN(x) ((x) & ((1U << 31) - 1U))
typedef unsigned fixed_point_t;

#define DRAW_SIZE (N_GRIDS + BOARD_SIZE)
#define DRAWBUFFER_SIZE                                                 \
    ((BOARD_SIZE * (BOARD_SIZE + 1) << 1) + (BOARD_SIZE * BOARD_SIZE) + \
     ((BOARD_SIZE << 1) + 1) + 1)

extern const line_t lines[4];

int *available_moves(unsigned int table);
char check_win(unsigned int t);
fixed_point_t calculate_win_value(char win, unsigned char player);
