#pragma once

#include <linux/ioctl.h>

#define BOARD_SIZE 4
#define GOAL 3
#define ALLOW_EXCEED 1
#define N_GRIDS (BOARD_SIZE * BOARD_SIZE)
#define N_GAMES 9
#define GET_INDEX(i, j) ((i) * (BOARD_SIZE) + (j))
#define GET_COL(x) ((x) % BOARD_SIZE)
#define GET_ROW(x) ((x) / BOARD_SIZE)
#define WIN_PATT_LEN(n, goal) (2 * (n - goal + 1) * (n + (n - goal + 1)))
#define CELL_EMPTY 0u
#define CELL_O 1u
#define CELL_X 2u
#define CELL_D 3u
#define ATTR_MSK 0xfu
#define XO_ATTR_ID(attr) (attr & ATTR_MSK)
#define XO_ATTR_STEPS(attr) get_bits(attr, 0xf, 4)
#define XO_SET_ATTR_STEPS(attr, type) set_bits(attr, type, 0xf, 0x4)
#define SET_RECORD_CELL(moves, step, n) set_bits64(moves, step, 0xful, n * 4u)
#define GET_RECORD_CELL(moves, id) get_bits64(moves, 0xful, id * 4u)
#define XO_IOCTL_MAGIC 0xbeaf
#define XO_IO_LDAVG _IOR(XO_IOCTL_MAGIC, 1, struct xo_avg[N_GAMES])

#define GEN_O_WINMASK(a, b, c) \
    ((CELL_O << (a * 2)) | (CELL_O << (b * 2)) | (CELL_O << (c * 2)))

#define VAL_SET_CELL(table, pos, cell) set_bits(table, cell, 3, pos * 2)

#define TABLE_GET_CELL(table, pos) get_bits(table, 3, pos * 2)

#define for_each_empty_grid(i, table) \
    for (int i = 0; i < N_GRIDS; i++) \
        if (TABLE_GET_CELL(table, i) == CELL_EMPTY)

typedef struct {
    int i_shift, j_shift;
    int i_lower_bound, j_lower_bound, i_upper_bound, j_upper_bound;
} line_t;

struct xo_table {
    int attr;
    unsigned int table;
    unsigned long moves;
};

struct xo_avg {
    unsigned short avg_x;
    unsigned short avg_o;
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

extern const line_t lines[4];

int *available_moves(unsigned int table);
char check_win(unsigned int t);
fixed_point_t calculate_win_value(char win, unsigned char player);
void fill_win_patterns(void);

static inline int set_bits(int x, int value, int mask, int n)
{
    return (x & ~(mask << n)) | (value << n);
}

static inline unsigned int get_bits(unsigned int x, int mask, int n)
{
    return (x & (mask << n)) >> n;
}

static inline long set_bits64(unsigned long x,
                              unsigned long value,
                              unsigned long mask,
                              int n)
{
    return (x & ~(mask << n)) | (value << n);
}

static inline unsigned int get_bits64(unsigned long x, long mask, int n)
{
    return (x & (mask << n)) >> n;
}
