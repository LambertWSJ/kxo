#pragma once

#include <stddef.h>
#include "game.h"

struct frame {
    char *buf;
    size_t len;
};

enum tui_tab {
    XO_TAB_RECORD,
    XO_TAB_LOADAVG,
    TAB_TOTLEN,
};

void tui_init();

void print_now();

void tui_quit(void);

void update_table(const struct xo_table *tlb);

void save_xy();

void restore_xy();

void outbuf_flush(void);

void update_time();

void render_logo(char *logo);

void clean_screen();

char *load_logo(const char *file);

void render_boards_temp(const int n);

void render_test();

void render_board(const struct xo_table *tlb, int n);

void tui_update_tab(enum tui_tab, const struct xo_table *tlb);
