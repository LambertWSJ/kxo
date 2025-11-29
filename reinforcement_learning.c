
#include "reinforcement_learning.h"
#include <linux/bitmap.h>
#include <linux/list.h>
#include <linux/random.h>
#include <linux/rbtree.h>
#include <linux/sizes.h>
#include <linux/spinlock_types.h>
#include <linux/vmalloc.h>
#include "ai_game.h"
#include "util.h"

#define AGENT_O (CELL_O - 1)
#define AGENT_X (CELL_X - 1)
#define MAX_STATES 256

static struct rb_root states;
static LIST_HEAD(orders);

struct xo_state {
    u32 table;
    rl_fxp scores[2];
    struct rb_node link;
    struct list_head list;
};

static int cnt_o;
static size_t cnt;
static void *buff;
static long *st_map;
static struct xo_state *st_buff;
static DEFINE_SPINLOCK(rl_lock);

/*
TODO:
    - implement bitmap and manage st_buff available position
    - add linked list to xo_state
*/
#define DLABEL "[ooxx]"
static int find_empty_next(void)
{
    // int pos;
    // for (pos = find_first_zero_bit(st_map, MAX_STATES); pos == MAX_STATES;
    //      pos = find_next_zero_bit(st_map, MAX_STATES, pos + 1)) {
    // }
    return find_first_zero_bit(st_map, MAX_STATES);
}

static void clear_st_buff(void)
{
    struct xo_state *ord, *safe;

    int pos;
    int i = 0;
    list_for_each_entry_safe(ord, safe, &orders, list) {
        if (i > MAX_STATES / 3) {
            break;
        }
        pos = ((uintptr_t) ord - (uintptr_t) st_buff) / sizeof(struct xo_state);
        rb_erase(&ord->link, &states);
        list_del(&ord->list);
        bitmap_clear(st_map, pos, 1);
        pr_info(DLABEL "clear %x at [%d], i = %d\n", ord->table, pos, i);
        i++;
    }
}
/*
    if find hen return node, if not find, then create new node
    and return new node
*/
static struct xo_state *find_rl_state(struct rb_root *root, const u32 table)
{
    struct rb_node **new = &(root->rb_node), *parent = NULL;

    while (*new) {
        struct xo_state *this = rb_entry(*new, struct xo_state, link);
        int pos_xo =
            ((uintptr_t) this - (uintptr_t) st_buff) / sizeof(struct xo_state);
        parent = *new;
        if (this->table == table) {
            pr_info(DLABEL "find %x in buff at [%d]", table, pos_xo);
            list_move_tail(&this->list, &orders);
            return this;
        } else if (this->table > table)
            new = &((*new)->rb_left);
        else if (this->table < table)
            new = &((*new)->rb_right);
    }

    // int next = bitmap_find_lsb(bitmaps); /* you should use for loop */
    // struct xo_state *st = &st_buff[next];
    int pos = find_empty_next();
    if (pos == MAX_STATES) {
        /* TODO: clear @st_buff */
        pr_info(DLABEL "clear state buff!");
        clear_st_buff();
        pos = find_empty_next();
    }

    struct xo_state *st = &st_buff[pos];
    st->scores[AGENT_O] =
        fixed_mul_s32(INITIAL_MUTIPLIER, get_score(table, CELL_O));
    st->scores[AGENT_X] =
        fixed_mul_s32(INITIAL_MUTIPLIER, get_score(table, CELL_X));

    st->table = table;
    smp_mb();
    rb_link_node(&st->link, parent, new);
    rb_insert_color(&st->link, root);
    list_add_tail(&st->list, &orders);
    bitmap_set(st_map, pos, 1);
    pr_info(DLABEL "insert %x at [%d]\n", table, pos);
    ++cnt_o;
    // pr_info("[RL]cnt => %d", cnt_o);
    return st;
}

int play_rl(unsigned int table, char player)
{
    int max_act = -1;
    rl_fxp max_q = RL_FIXED_MIN;
    int candidate_count = 1;
    int id = player - 1;
    rl_fxp new_q = 0;
    spin_lock_bh(&rl_lock);
    for_each_empty_grid(i, table)
    {
        table = VAL_SET_CELL(table, i, player);
        new_q = find_rl_state(&states, table)->scores[id];
        if (new_q == max_q) {
            ++candidate_count;
            if (get_random_u32() % candidate_count == 0)
                max_act = i;
        } else if (new_q > max_q) {
            candidate_count = 1;
            max_q = new_q;
            max_act = i;
        }
        table = VAL_SET_CELL(table, i, CELL_EMPTY);
    }

    if (max_act == -1) {
        pr_err("error:[max_act] new_q=%8x, table=%8x\n", new_q, table);
    }
    spin_unlock_bh(&rl_lock);
    return max_act;
}

/* player assume always AGENT_O or AGENT_X */
static inline rl_fxp step_update_state_value(int after_state_hash,
                                             rl_fxp reward,
                                             rl_fxp next,
                                             u8 player)
{
    rl_fxp curr = reward - fixed_mul(GAMMA, next);
    struct xo_state *st = find_rl_state(&states, after_state_hash);
    st->scores[player] =
        fixed_mul((RL_FIXED_1 - LEARNING_RATE), st->scores[player]) +
        fixed_mul(LEARNING_RATE, curr);
    return st->scores[player];
}

void update_state_value(const int *after_state_hash,
                        const rl_fxp *reward,
                        int steps,
                        char player)
{
    spin_lock_bh(&rl_lock);
    rl_fxp next = 0;
    for (int j = steps - 1; j >= 0; j--)
        next = step_update_state_value(after_state_hash[j], reward[j], next,
                                       player - 1);
    spin_unlock_bh(&rl_lock);
}

void free_rl_agent(void)
{
    vfree(buff);
}

void init_rl_agent(void)
{
    states = RB_ROOT;
    LIST_HEAD(order);
    size_t nodes_sz = MAX_STATES * sizeof(struct xo_state);
    buff = vmalloc(ALIGN(nodes_sz + MAX_STATES, PAGE_SIZE));
    st_buff = buff;
    st_map = buff + nodes_sz;
    bitmap_zero(st_map, MAX_STATES);

    cnt_o = 0;
    cnt = 0;
}
