#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "common.h"
#include "shell.h"
#include "shell_rl.h"
#include "opq.h"
#include "port.h"

#define TERMIOS_DEBUG_ENABLE 0
#include "termios_debug.h"

struct shell_rl {
    bool initialized;
    bool enabled;
    struct readline_state rlstate;
#if 0
    struct {
        rl_vintfunc_t *prep_term_function;
        rl_voidfunc_t *deprep_term_function;
    } org;
#endif
};

static struct shell_rl shell_rl_data;
/**
 * called by libreadline when line completed. seems like only one
 * callback should be used(?) as `rl_linefunc` is only accessible through 
 * `rl_callback_handler_install` that also does other things
 */
static void _on_rl_line(char *line)
{
    /* ctrl-C and other control keys trigger this if we do not catch
     * them before readline */
    if (!line) {
        LOG_ERR("null string - uncaught ctrl key not handled by readline?");
        return; // TODO do what?
    }

    size_t len = strlen(line);
    if (len) {
        opq_enqueue_write(&opq_rt, line, len);
        add_history(line);
    }
    else {
        free(line);
    }
    // always send EOL on enter
    opq_enqueue_val(&opq_rt, OP_PORT_PUT_EOL, 1);
    LOG_DBG("'%s'", line);
    // no free!
}
/**
 * insert char when in libreadline "callback mode" (RL_STATE_CALLBACK set).
 * note that the following will _not_ work with RL_STATE_CALLBACK as enter key
 * not recognized correctly.
 *
 * ```
 *      int c = rl_getc(stdin);
 *      rl_insert(1, c);
 *      rl_redisplay();
 * ```
 */
void shell_rl_insertchar(int c)
{
    rl_pending_input = c;
    // will read pending and call rl_redisplay();
    rl_callback_read_char();
}

const void *shell_rl_save(void)
{
    struct shell_rl *data = &shell_rl_data;

    if (!data->enabled)
        return NULL;

    if (!shell_opts.sticky)
        return NULL;

    if (RL_ISSTATE(RL_STATE_DONE))
        return NULL;

    struct readline_state *rlstate = &data->rlstate;

    rl_save_state(rlstate);
    rl_set_prompt("");
    rl_replace_line("", 0);
    rl_redisplay();

    return rlstate;
}

void shell_rl_restore(const void *x)
{
    struct shell_rl *data = &shell_rl_data;


    const struct readline_state *rlstate_x = x;
    if (!rlstate_x)
        return; // shell not initialized yet or lock not needed in this mode

    struct readline_state *rlstate = &data->rlstate;
#if 0

    if (rlstate_x != rlstate) {
        LOG_WRN("rl state lock from unexpected source");
    }
#endif
    rl_restore_state(rlstate);
    rl_forced_update_display();
}
#if 0
void shell_rl_init(void)
{
    struct shell_rl *data = &shell_rl_data;
    // save some rl defaults before anything else
#if 0
    assert(rl_prep_term_function);
    assert(rl_deprep_term_function);

    data->org.prep_term_function = rl_prep_term_function;
    data->org.deprep_term_function = rl_deprep_term_function;
#endif

    /* most rl_<globals> will be copied to `rlstate` when calling
     * `rl_save_state(rlstate)` */

    rl_catch_signals = 0;
    rl_catch_sigwinch = 0;
    rl_erase_empty_line = 1;

    data->initialized = true;
}
#endif

void shell_rl_enable(void)
{
    struct shell_rl *data = &shell_rl_data;

    /** TODO? prompt not supported as libreadline have trouble to display it
     * correctly (somtimes on previous line, somtimes not shown until first key
     * press etc)
     */
    const char *prompt = NULL; // port_opts.name;

    if (data->enabled)
        return;

    /* first use of libreadline */
    if (!data->initialized) {

        /* most rl_<globals> will be copied to `rlstate` when calling
         * `rl_save_state(rlstate)` */
        rl_catch_signals = 0;
        rl_catch_sigwinch = 0;
        rl_erase_empty_line = 1;

        data->initialized = true;
    }
    else {

        // restore whatever was there when last enabled
        rl_restore_state(&data->rlstate);
    }

    /* rl_callback_handler_install will call rl_initalize and a bunch of other
     * functions to confiugre the tty.
     * note these changes will also be copied to on later when rl_state_save() called
     */
    ___TERMIOS_DEBUG_BEFORE();
    rl_callback_handler_install(prompt, _on_rl_line);
    ___TERMIOS_DEBUG_AFTER("rl_callback_handler_install");


    // clear. needed?
    //rl_replace_line("", 0);
    //rl_redisplay();
    rl_reset_line_state();
    //rl_set_prompt(prompt);
    //rl_on_new_line();

    rl_redisplay(); // promt not shown if not redisplayed

    data->enabled = true;
}

void shell_rl_disable(void)
{
    struct shell_rl *data = &shell_rl_data;

    if (!data->enabled)
        return;

    /// save globals
    rl_save_state(&data->rlstate);

    // clear
    rl_replace_line("", 0);
    rl_redisplay();

    rl_message("");
    rl_callback_handler_remove();
    data->enabled = false;

}

void shell_rl_cleanup(void)
{
    struct shell_rl *data = &shell_rl_data;

    if (!data->initialized)
        return;

    /* TODO 
     * if (shell_rl_data.history[0] != '\0')
     *      write_history(shell_rl_data.history);
     */ 

    rl_message("");

    ___TERMIOS_DEBUG_BEFORE();
    rl_callback_handler_remove();
    ___TERMIOS_DEBUG_AFTER("rl_callback_handler_remove");
}

/** it seems that if libreadline initalized in any way, stdin should _only_ be read
 * by libreadline. i.e. must use `rl_getc()`
 * 
 * libreadline "remaps" '\n' to '\r'. this is done through some
 * tty settings, so even if `fgetc(stdin)` "normaly" returns '\n' for enter
 * key - after libreadline internaly have called rl_prep_term(), the return
 * value will be '\r'.
 * */
int shell_rl_getchar(void)
{
    return rl_getc(stdin);
}

