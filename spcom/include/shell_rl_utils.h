/** lib readline helpers */

#ifndef SHELL_RL_UTILS_INCLUDE_H__
#define SHELL_RL_UTILS_INCLUDE_H__

#include <stdbool.h>
#include "assert.h"

#include <readline/readline.h>
#include <readline/history.h>

static inline void shell_rl_state_init_default(struct readline_state *rlstate)
{
    // rl_<globals> will be copied to `rl_state`:s saved
    rl_catch_signals = 0;
    rl_catch_sigwinch = 0;
    rl_erase_empty_line = 1;

    rl_save_state(rlstate);

    assert(!rlstate->catchsigs);
    assert(!rlstate->catchsigwinch);
}

/**
 * TODO would one of these work?:
 *    extern int rl_stuff_char (int);
 *    extern int rl_execute_next (int
 *
 * implementing a "rl_putc()" - alternatives:
 *
 * 1. Setting `rl_pending_input` to a value makes it the next keystroke read.
 * sort of a "rl_putc()" also need to update reader with
 * rl_callback_read_char();!?
 *
 * 2.Set `FILE * rl_instream` to `fmemopen` or a pipe, then write to it.
 *
 * 3. modify `rl_line_buffer` - not recomended as it is also modifed by input
 */
static inline void shell_rl_putc(int c)
{
    rl_pending_input = c;
    rl_callback_read_char();
    rl_redisplay(); //needed?
}

static inline void shell_rl_state_save(struct readline_state *rlstate)
{
    if (RL_ISSTATE(RL_STATE_DONE))
        return;

    rl_save_state(rlstate);
    rl_set_prompt("");
    rl_replace_line("", 0);
    rl_redisplay();
}

static inline void shell_rl_state_restore(struct readline_state *rlstate)
{
    rl_restore_state(rlstate);
    rl_forced_update_display();
}

static inline void shell_rl_mode_enter(struct readline_state *rlstate,
                                       const char *prompt)
{
    if (!prompt)
        prompt = "";

    rl_on_new_line();

    rl_restore_state(rlstate);
    // clear. needed?
    rl_replace_line("", 0);
    rl_redisplay();
    //rl_reset_line_state();
    rl_set_prompt(prompt);
    rl_redisplay();
}

static void shell_rl_mode_leave(struct readline_state *rlstate)
{
    /// save globals
    rl_save_state(rlstate);
    // clear
    rl_replace_line("", 0);
    rl_redisplay();
}
#endif
