#pragma once

#include <stddef.h>
#include "game.h"

struct frame {
    char *buf;
    size_t len;
};

struct xo_tab {
    char *title;
    void *data;
    void (*update_ctx)(void *data);
};

void tui_init();

void print_now();

void tui_quit(void);

void update_table(const struct xo_table *tlb);

void save_xy();

void restore_xy();

void outbuf_flush(void);

void update_time();

void update_tabs(struct xo_tab **tabs);

void render_logo(char *logo);

void clean_screen();

char *load_logo(const char *file);

void render_boards_temp(const int n);

void render_test();

void render_board(const struct xo_table *tlb, int n);
