#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "game.h"
#include "tui.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define DIVBY(n, b) !(n & (b - 1))
#define ESC "\033"
#define CLEAR_SCREEN ESC "[2J" ESC "[1;1H"
#define CLEAR_LINE ESC "[2K"
#define ALT_BUF_ENABLE "\033[?1049h"
#define ALT_BUF_DISABLE "\033[?1049l"
#define HIDE_CURSOR ESC "[?25l"
#define SHOW_CURSOR ESC "[?25h"
#define SAVE_XY ESC "[s"
#define RESTORE_XY ESC "[u"
#define OUTBUF_SIZE 4096
#define FLUSH_THRESHOLD 2048 /* Flush when half-full for optimal latency */
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define o_ch COLOR_GREEN "O" COLOR_RESET
#define x_ch COLOR_RED "X" COLOR_RESET


#define MIN_COLS 55
#define MIN_ROWS 21

#define UI_COLS 3
#define BOXCH_LEN 3

#define TAB_CTX_BASEY 52
#define TAB_LABEL_BASEY 49
#define BOARD_W 35
#define BOARD_H 13
#define BOARD_BASEY 10

static struct {
    char buf[OUTBUF_SIZE];
    size_t len;
    bool disabled; /* Emergency fallback when buffer management fails */
} outbuf = {0};

static int tab_maxh = -1;

struct xo_tab {
    char *title;
    int high;
};

static struct xo_tab tui_tabs[TAB_TOTLEN] = {
    [XO_TAB_RECORD] = {.title = "Records", .high = 11},
    [XO_TAB_LOADAVG] = {.title = "Load avg", .high = 12},
};

static struct termios orig_termios;

/* Write-combining buffer for low-latency terminal output */
#define OUTBUF_SIZE 4096
#define FLUSH_THRESHOLD 2048 /* Flush when half-full for optimal latency */

static void raw_mode_enable(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);

    struct termios raw = orig_termios;
    raw.c_iflag &= ~IXON;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void raw_mode_disable(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void safe_write(int fd, const void *buf, size_t count)
{
    ssize_t result = write(fd, buf, count);
    /* In terminal context, write failures are usually due to
     * broken pipes or terminal issues - not critical for game logic
     */
    if (result == -1)
        return;
}

/* Write-combining buffer management with automatic flushing */
static void outbuf_write(const char *data, size_t data_len)
{
    /* Emergency fallback: direct write if buffering disabled */
    if (outbuf.disabled) {
        safe_write(STDOUT_FILENO, data, data_len);
        return;
    }

    /* Handle writes larger than buffer */
    if (data_len >= OUTBUF_SIZE) {
        /* Flush current buffer first */
        if (outbuf.len > 0) {
            safe_write(STDOUT_FILENO, outbuf.buf, outbuf.len);
            outbuf.len = 0;
        }
        /* Write large data directly */
        safe_write(STDOUT_FILENO, data, data_len);
        return;
    }

    /* Check if this write would exceed buffer capacity */
    if (outbuf.len + data_len > OUTBUF_SIZE) {
        /* Flush current buffer to make room */
        safe_write(STDOUT_FILENO, outbuf.buf, outbuf.len);
        outbuf.len = 0;
    }

    /* Add data to buffer */
    memcpy(outbuf.buf + outbuf.len, data, data_len);
    outbuf.len += data_len;

    /* Auto-flush when reaching threshold for optimal latency */
    if (outbuf.len >= FLUSH_THRESHOLD) {
        safe_write(STDOUT_FILENO, outbuf.buf, outbuf.len);
        outbuf.len = 0;
    }
}

/* Formatted write to output buffer with bounds checking */
static void outbuf_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    /* Calculate space remaining in buffer */
    size_t remaining = OUTBUF_SIZE - outbuf.len;

    /* Try to format into remaining buffer space */
    int written = vsnprintf(outbuf.buf + outbuf.len, remaining, format, args);
    va_end(args);

    if (written < 0) {
        /* Formatting error - use emergency fallback */
        assert(0);
        outbuf.disabled = true;
        return;
    }

    if ((size_t) written >= remaining) {
        /* Output was truncated - flush buffer and retry */
        if (outbuf.len > 0) {
            safe_write(STDOUT_FILENO, outbuf.buf, outbuf.len);
            outbuf.len = 0;
        }

        /* Retry formatting with full buffer */
        va_start(args, format);
        written = vsnprintf(outbuf.buf, OUTBUF_SIZE, format, args);
        va_end(args);

        if (written < 0 || (size_t) written >= OUTBUF_SIZE) {
            /* Still doesn't fit or error - use emergency fallback */
            outbuf.disabled = true;
            return;
        }
    }

    /* Update buffer length */
    outbuf.len += written;

    /* Auto-flush when reaching threshold */
    if (outbuf.len >= FLUSH_THRESHOLD) {
        safe_write(STDOUT_FILENO, outbuf.buf, outbuf.len);
        outbuf.len = 0;
    }
}

/* Force flush of output buffer */
void outbuf_flush(void)
{
    if (outbuf.len > 0) {
        safe_write(STDOUT_FILENO, outbuf.buf, outbuf.len);
        outbuf.len = 0;
    }
    /* Keep stdio in sync for any legacy printf() calls */
    fflush(stdout);
}

void save_xy()
{
    outbuf_write(SAVE_XY, strlen(SAVE_XY));
}

void restore_xy()
{
    outbuf_write(RESTORE_XY, strlen(RESTORE_XY));
}

static void gotoxy(int x, int y)
{
    outbuf_printf("\033[%d;%dH", y, x);
}

void disable_raw()
{
    outbuf_flush();
    safe_write(STDOUT_FILENO, ALT_BUF_DISABLE, sizeof(ALT_BUF_DISABLE) - 1);
    raw_mode_disable();
}

void tui_init()
{
    raw_mode_enable();
    outbuf_write(ALT_BUF_ENABLE, strlen(ALT_BUF_ENABLE));
    outbuf_write(CLEAR_SCREEN HIDE_CURSOR, strlen(CLEAR_SCREEN HIDE_CURSOR));
    outbuf_printf(HIDE_CURSOR);
    outbuf_flush();

    for (int i = 0; i < TAB_TOTLEN; i++)
        tab_maxh = max(tab_maxh, tui_tabs[i].high);

    atexit(disable_raw);
}

void tui_quit(void)
{
    safe_write(STDOUT_FILENO, SHOW_CURSOR, sizeof(SHOW_CURSOR) - 1);
    safe_write(STDOUT_FILENO, COLOR_RESET, sizeof(COLOR_RESET) - 1);
    disable_raw();
}

void clean_screen()
{
    outbuf_printf("\033[2J\033[H");
}

static void clean_line()
{
    outbuf_printf(CLEAR_LINE);
}

char *load_logo(const char *file)
{
    int fd = open(file, O_RDONLY);

    if (fd < 0) {
        printf("not found logo\n");
        return NULL;
    }
    struct stat st;
    fstat(fd, &st);
    size_t sz = st.st_size;

    char *buf = malloc(sz);
    read(fd, buf, sz);
    close(fd);
    return buf;
}

void render_logo(char *logo)
{
    char *beg = logo;
    const char *pos;
    int ln = '\n';
    int k = 0;
    int x = 38;

    do {
        pos = strchr(beg, ln);
        k++;
        gotoxy(x, k);
        size_t len = pos - beg;
        outbuf_write(beg, len);
        beg += len + 1;
    } while (pos);

    gotoxy(x, k);
    outbuf_flush();
}

void print_now()
{
    static time_t timer;
    const struct tm *tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    gotoxy(50, 48);
    outbuf_printf("⏰%02d:%02d:%02d\n", tm_info->tm_hour, tm_info->tm_min,
                  tm_info->tm_sec);
}

/* n boards */
void render_boards_temp(const int n)
{
    if (n <= 0)
        return;

    int rows = (n - 1) / UI_COLS;
    int rem_bods = n % UI_COLS;
    int base_y = 10;
    int bod_w = 35;
    int bod_h = 11;

    for (int i = 0; i < n; i++) {
        int r = i / UI_COLS;
        bool last_col = ((i + 1) % UI_COLS) == 0;
        bool first_col = (i % UI_COLS) == 0;
        bool last_row = (r) == rows;

        int x = (i % UI_COLS) * bod_w;
        int y = base_y + ((i / UI_COLS) * (bod_h + 1));
        gotoxy(x, y + 1);

        /* horizontal line */
        for (int j = 0; j < bod_w && i < UI_COLS; j++)
            outbuf_write("─", BOXCH_LEN);

        if (n == 1 && i < UI_COLS)
            outbuf_write("\b┐", 1 + BOXCH_LEN);
        else if (i + 1 == UI_COLS || i + 1 == n)
            outbuf_write("┐", BOXCH_LEN);


        /* bottom horizontal line */
        for (int j = 0; j < bod_w; j++) {
            gotoxy(x + j, base_y + y + 3);
            outbuf_write("─", BOXCH_LEN);
        }

        /* vertical line */
        for (int j = 0; j < bod_h + 2; j++) {
            /* fill vertical */
            gotoxy(x, y + j + 1);
            if (first_col) {
                if (j == bod_h + 1 && i + 1 == n) {
                    outbuf_write("└", BOXCH_LEN);
                    gotoxy(x + bod_w, y + j + 1);
                    outbuf_write("┘", BOXCH_LEN);
                    continue;
                } else if (i == 0 && j == 0) {
                    outbuf_write("┌", BOXCH_LEN);
                    continue;
                } else if (j == (bod_h + 1) && last_row) {
                    outbuf_write("└", BOXCH_LEN);
                    continue;
                }
            }

            if (i < UI_COLS && j == 0)
                outbuf_write("┬", BOXCH_LEN);
            else if (j == bod_h + 1 && i + UI_COLS > n)
                outbuf_write("┴", BOXCH_LEN);
            else if (j == 0 && !first_col)
                outbuf_write("┼", BOXCH_LEN);
            else if (j == 0 && first_col)
                outbuf_write("├", BOXCH_LEN);
            else
                outbuf_write("│", BOXCH_LEN);


            /* last vertical line */
            if (last_col || i + 1 == n) {
                gotoxy(x + bod_w, y + j + 1);
                if (j == bod_h + 1 && i + 1 == n - rem_bods ||
                    j == bod_h + 1 && i + 1 == n)
                    outbuf_write("┘", BOXCH_LEN);
                else if (j == 0 && (i + 1 == UI_COLS || i < UI_COLS))
                    outbuf_write("┐", BOXCH_LEN);
                else if (j == 0 && last_col)
                    outbuf_write("┤", BOXCH_LEN);
                else if (j == 0 && i > UI_COLS && i + UI_COLS > n)
                    outbuf_write("┼", BOXCH_LEN);
                else
                    outbuf_write("│", BOXCH_LEN);
            }
        }
    }

    outbuf_printf("\n");
    outbuf_flush();
}

void update_table(const struct xo_table *xo_tlb)
{
    const char *cell_tlb[] = {" ", o_ch, x_ch};
    int id = xo_tlb->id;
    unsigned int table = xo_tlb->table;
    int y = BOARD_BASEY + (id / UI_COLS) * (BOARD_H - 1);

    int x = (id % UI_COLS) * BOARD_W + 1;
    int tlb_x = x + 9;
    int tlb_y = y + 3;
    const int tlb_w = 17;
    const int tlb_h = 9;
    const int cell_x = tlb_x + 2;
    const int cell_y = tlb_y + 1;
    const int stepx = 4;
    const int stepy = 2;

    gotoxy(x + 15, y + 2);
    outbuf_printf("Geme-%d\n", id);

    for (int i = 0; i < tlb_h; i++) {
        gotoxy(tlb_x, tlb_y + i);
        bool last_h = i + 1 == tlb_h;

        for (int j = 0; j < tlb_w; j++) {
            bool last_w = j + 1 == tlb_w;

            if (i & 1) {
                /* odd row */
                if (DIVBY(j, 4))
                    outbuf_write("│", BOXCH_LEN);
                else
                    outbuf_write(" ", 1);
            } else {
                /* even row */
                if (j == 0) {
                    if (i == 0)
                        outbuf_write("┌", BOXCH_LEN);
                    else if (last_h)
                        outbuf_write("└", BOXCH_LEN);
                    else
                        outbuf_write("├", BOXCH_LEN);

                } else if (i == 0 && last_w) {
                    outbuf_write("┐", BOXCH_LEN);
                } else if (DIVBY(j, 4)) {
                    if (i == 0)
                        outbuf_write("┬", BOXCH_LEN);
                    else if (i != tlb_h - 1 && !last_w)
                        outbuf_write("┼", BOXCH_LEN);
                    else if (last_h && !last_w)
                        outbuf_write("┴", BOXCH_LEN);
                    else if (last_h && last_w)
                        outbuf_write("┘", BOXCH_LEN);
                    else
                        outbuf_write("┤", BOXCH_LEN);

                } else
                    outbuf_write("─", BOXCH_LEN);
            }
        }
    }


    for (int i = 0; i < N_GRIDS; i++) {
        /* gotoxy(cell_x + (i % 4) * stepx, cell_y + (i / 4) * stepy); */
        const int pos_x = cell_x + (i & (BOARD_SIZE - 1)) * stepx;
        const int pos_y = cell_y + (i / BOARD_SIZE) * stepy;
        gotoxy(pos_x, pos_y);
        outbuf_printf("%s", cell_tlb[TABLE_GET_CELL(table, i)]);
    }

    gotoxy(x + 12, y + 12);
    outbuf_printf("MCTS vs NEGA\n");
    outbuf_flush();
}

static void draw_tab_border(const int high)
{
    int x = 1;
    const int w = 106;
    for (int i = 0; i < tab_maxh + 1; i++) {
        gotoxy(x, TAB_CTX_BASEY + i);
        clean_line();
    }

    gotoxy(x, TAB_CTX_BASEY + high);
    outbuf_write("╰", BOXCH_LEN);
    gotoxy(x + w - 2, TAB_CTX_BASEY + high);
    outbuf_write("╯", BOXCH_LEN);

    for (int i = 0; i < high; i++) {
        gotoxy(x, TAB_CTX_BASEY + i);
        outbuf_write("│", BOXCH_LEN);
        gotoxy(x + w - 2, TAB_CTX_BASEY + i);
        outbuf_write("│", BOXCH_LEN);
    }

    gotoxy(x + 1, TAB_CTX_BASEY + high);
    for (int i = 0; i < w - 3; i++)
        outbuf_write("─", BOXCH_LEN);
}

static void xo_record(const enum tui_tab tab)
{
    draw_tab_border(tui_tabs[tab].high);
}

static void xo_loadavg(const enum tui_tab tab)
{
    draw_tab_border(tui_tabs[tab].high);
}

static void draw_tab_label(enum tui_tab tab)
{
    int x = 0;
    int y = TAB_LABEL_BASEY;

    gotoxy(x, y);
    int n = 106;
    int labelh = 3;
    int tablen = x + 1;
    /* draw tab label */
    for (int i = 0; i < TAB_TOTLEN; i++) {
        struct xo_tab *xtab = &tui_tabs[i];
        size_t len = strlen(xtab->title);
        len += 4;

        for (int k = 0; k < labelh; k++) {
            bool first_row = k == 0;
            gotoxy(x + tablen, y + k);
            if (k == 1) {
                outbuf_printf("│ %s │", xtab->title);
                continue;
            }

            for (int j = 0; j < len; j++) {
                bool first_col = j == 0;
                bool last_col = j == len - 1;
                if (first_row) {
                    if (first_col)
                        outbuf_write("╭", BOXCH_LEN);
                    else if (last_col)
                        outbuf_write("╮", BOXCH_LEN);
                    else
                        outbuf_write("─", BOXCH_LEN);

                } else {
                    if (first_col) {
                        if (i == 0)
                            outbuf_write(i == tab ? "│" : "├", BOXCH_LEN);
                        else
                            outbuf_write(i == tab ? "┘" : "┴", BOXCH_LEN);


                    } else if (last_col)
                        outbuf_write(i == tab ? "└" : "┴", BOXCH_LEN);
                    else {
                        if (i == tab)
                            outbuf_write(" ", 1);
                        else
                            outbuf_write("─", BOXCH_LEN);
                    }
                }
            }
        }
        tablen += len;
    }

    /* draw last line border */
    gotoxy(x + tablen, y + labelh - 1);
    n -= tablen;
    for (int i = 0; i < n; i++)
        outbuf_write(i == n - 1 ? "╮" : "─", BOXCH_LEN);
}

void tui_update_tab(enum tui_tab tab)
{
    assert(tab < TAB_TOTLEN);
    draw_tab_label(tab);
    /* request data and render */

    switch (tab) {
    case XO_TAB_RECORD:
        xo_record(tab);
        break;
    case XO_TAB_LOADAVG:
        xo_loadavg(tab);
        break;
    default:
        assert(0);
    }
}
