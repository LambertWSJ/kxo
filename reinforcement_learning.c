
#include "reinforcement_learning.h"
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ai_game.h"
#include "util.h"


static u8 cells[] = {CELL_EMPTY, CELL_O, CELL_X};

int table_to_hash(unsigned int table)
{
    int ret = 0;
    for (int i = 0; i < N_GRIDS; i++) {
        ret *= 3;
        ret += TABLE_GET_CELL(table, i) % CELL_D;
    }
    return ret;
}

static unsigned int hash_to_table(int hash)
{
    unsigned int table = 0;
    for (int i = N_GRIDS - 1; i >= 0; i--) {
        table = VAL_SET_CELL(table, i, cells[hash % 3]);
        hash /= 3;
    }
    return table;
}

static int get_action_exploit(unsigned int table, const rl_agent_t *agent)
{
    int max_act = -1;
    fixed_point_t max_q = FIXED_MIN;
    const fixed_point_t *state_value = agent->state_value;
    int candidate_count = 1;
    int last_e = 0;

    for_each_empty_grid(i, table)
    {
        last_e = i;
        table = VAL_SET_CELL(table, i, agent->player);
        fixed_point_t new_q = state_value[table_to_hash(table)];
        if (new_q == max_q) {
            ++candidate_count;
            if (get_random_u32() % candidate_count == 0) {
                max_act = i;
            }
        } else if (new_q > max_q) {
            candidate_count = 1;
            max_q = new_q;
            max_act = i;
        }
        table = VAL_SET_CELL(table, i, CELL_EMPTY);
    }
    /* If there is no best action, then choose the last empty cell */
    return max_act == -1 ? last_e : max_act;
}

fixed_point_t update_state_value(int after_state_hash,
                                 fixed_point_t reward,
                                 fixed_point_t next,
                                 rl_agent_t *agent)
{
    fixed_point_t curr =
        reward - fixed_mul(GAMMA, next);  // curr is TD target in TD learning
                                          // and return/gain in MC learning.
    agent->state_value[after_state_hash] =
        fixed_mul((RL_FIXED_1 - LEARNING_RATE),
                  agent->state_value[after_state_hash]) +
        fixed_mul(LEARNING_RATE, curr);
    return agent->state_value[after_state_hash];
}

unsigned int play_rl(unsigned int *table, const rl_agent_t *agent)
{
    unsigned int tlb = *table;
    int move = get_action_exploit(tlb, agent);
    *table = VAL_SET_CELL(tlb, move, agent->player);
    return move;
}

void init_rl_agent(rl_agent_t *agent, unsigned int state_num, char player)
{
    agent->player = player;
    agent->state_value = vmalloc(sizeof(fixed_point_t) * state_num);
    if (!(agent->state_value))
        pr_info("Failed to allocate memory");

    for (unsigned int i = 0; i < state_num; i++) {
        agent->state_value[i] = fixed_mul_s32(
            INITIAL_MUTIPLIER, get_score(hash_to_table(i), player));
    }
}
