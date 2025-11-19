
#include "reinforcement_learning.h"
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "ai_game.h"
#include "util.h"

static struct mutex rl_locks[2];
static u8 cells[] = {CELL_EMPTY, CELL_O, CELL_X};
static rl_agent_t rl_agents[] = {
    [CELL_O - 1] = {.player = CELL_O},
    [CELL_X - 1] = {.player = CELL_X},
};

static unsigned int hash_to_table(int hash)
{
    int table = 0;
    for (int i = N_GRIDS - 1; i >= 0; i--) {
        table = VAL_SET_CELL(table, i, cells[hash % 3]);
        hash /= 3;
    }
    return table;
}

int table_to_hash(unsigned int table)
{
    int ret = 0;
    for (int i = 0; i < N_GRIDS; i++) {
        ret *= 3;
        ret += TABLE_GET_CELL(table, i) % CELL_D;
    }
    return ret;
}

int play_rl(unsigned int table, char player)
{
    int max_act = -1;
    fixed_point_t max_q = FIXED_MIN;
    const rl_agent_t *agent = &rl_agents[player - 1];
    const fixed_point_t *state_value = agent->state_value;
    int candidate_count = 1;

    mutex_lock(&rl_locks[player - 1]);
    for_each_empty_grid(i, table)
    {
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
    mutex_unlock(&rl_locks[player - 1]);
    return max_act;
}

static inline fixed_point_t step_update_state_value(int after_state_hash,
                                                    fixed_point_t reward,
                                                    fixed_point_t next,
                                                    char player)
{
    rl_agent_t *agent = &rl_agents[player - 1];
    fixed_point_t curr =
        reward - fixed_mul(GAMMA, next);  // curr is TD target in TD learning
                                          // and return/gain in MC learning.
    agent->state_value[after_state_hash] =
        fixed_mul((RL_FIXED_1 - LEARNING_RATE),
                  agent->state_value[after_state_hash]) +
        fixed_mul(LEARNING_RATE, curr);
    return agent->state_value[after_state_hash];
}

void update_state_value(const int *after_state_hash,
                        const fixed_point_t *reward,
                        int steps,
                        char player)
{
    mutex_lock(&rl_locks[player - 1]);
    fixed_point_t next = 0;
    for (int j = steps - 1; j >= 0; j--)
        next = step_update_state_value(after_state_hash[j], reward[j], next,
                                       player);
    mutex_unlock(&rl_locks[player - 1]);
}

void free_rl_agent(unsigned char player)
{
    vfree(rl_agents[player - 1].state_value);
}

void init_rl_agent(unsigned int state_num, char player)
{
    rl_agent_t *agent = &rl_agents[player - 1];
    mutex_init(&rl_locks[player - 1]);
    agent->state_value = vmalloc(sizeof(fixed_point_t) * state_num);
    if (!(agent->state_value))
        pr_info("Failed to allocate memory");

    for (unsigned int i = 0; i < state_num; i++) {
        agent->state_value[i] = fixed_mul_s32(
            INITIAL_MUTIPLIER, get_score(hash_to_table(i), player));
    }
}
