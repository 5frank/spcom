// std
#include <stdlib.h>
// deps
#include <readline/readline.h>
#include <readline/history.h>
// local
#include "common.h"
#include "port_opts.h"
#include "log.h"
#include "opq.h"
#include "opt.h"
#include "shell.h"

static struct {
    const char *prompt;
    int sticky;
} sh_cooked_opts = { 0 };

static struct {
    bool initialized;
    struct readline_state rlstate;
    const char *prompt;

    struct shell_rl_state {
        int restore;
        int point;
        char *line;
    } rl_state;
} sh_cooked_data;

/// completer called in cooked mode
static char **sh_cooked_complete(const char *text, int start, int end)
{
    // this must be set or readline will abort application
    rl_attempted_completion_over = 1;
    // TODO get complete from cache/history/config/somewhere
    return NULL;
}

/**
 * called by libreadline when line completed. only one
 * callback should be used as `rl_linefunc` is only accessible through 
 * `rl_callback_handler_install` that also does other things and we do not want
 * to change the callback from the callback (causes some problems side effects).
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
    // no free!
}

static bool _rl_state_save(void)
{
    // warning can not use log module here - recursion

    typeof(sh_cooked_data) *data = &sh_cooked_data;

    if (!data->initialized)
        return false;

    if (!sh_cooked_opts.sticky)
        return false;

    if (RL_ISSTATE(RL_STATE_DONE))
        return false;

    struct shell_rl_state *state = &data->rl_state;

    state->point = rl_point;
    state->line = rl_copy_text(0, rl_end);

    /* note: using `rl_clear_visible_line()` here causes some charachters
     * printed to stdout elsewhere to be lost */

    rl_save_prompt();
    rl_replace_line("", 0);
    rl_redisplay();

    return true;
}

static void _rl_state_restore(bool was_saved)
{
    // warning can not use log module here - recursion

    if (!was_saved)
        return;

    typeof(sh_cooked_data) *data = &sh_cooked_data;
    struct shell_rl_state *state = &data->rl_state;

    rl_restore_prompt();
    rl_replace_line(state->line, 0);
    rl_point = state->point;
    rl_forced_update_display();
    free(state->line);
    state->line = NULL;
}


static void sh_cooked_init(void)
{
    typeof(sh_cooked_data) *data = &sh_cooked_data;

    /* first use of libreadline */
    if (data->initialized) {
        return;
    }

    data->prompt = (sh_cooked_opts.prompt)
        ? sh_cooked_opts.prompt
        : port_opts->name;

    /* most rl_<globals> will be copied to `rlstate` when calling
     * `rl_save_state(rlstate)`
     * i.e. this will implicitly initalize rlstate */
    rl_catch_signals = 0;
    rl_catch_sigwinch = 0;
    rl_erase_empty_line = 1;

#if 0

    // Allow conditional parsing of the ~/.inputrc file. 
    rl_readline_name = "spcom";
    rlstate->attemptfunc = sh_cooked_complete;
    using_history();    // initialize history
#endif

    /* rl_callback_handler_install will call rl_initalize and a bunch of other
     * functions to confiugre the tty.
     * note these changes will also be copied to on later when rl_state_save() called
     */
    ___TERMIOS_DEBUG_BEFORE();
    rl_callback_handler_install(data->prompt, _on_rl_line);
    ___TERMIOS_DEBUG_AFTER("rl_callback_handler_install");


    // clear. needed?
    //rl_replace_line("", 0);
    //rl_redisplay();
    //rl_reset_line_state();
    //rl_set_prompt(data->prompt);

    //rl_redisplay(); // promt not shown if not redisplayed
    //rl_on_new_line(); // must be last!
    data->initialized = true;
}

static void sh_cooked_exit(void)
{
    typeof(sh_cooked_data) *data = &sh_cooked_data;

    if (!data->initialized)
        return;

    /* TODO 
     * if (data->history[0] != '\0')
     *      write_history(data->history); */

    //save globals, if we want to re-enter this mode
    //rl_save_state(&data->rlstate);

    // clear
    rl_replace_line("", 0);
    rl_redisplay();

    rl_message("");

    ___TERMIOS_DEBUG_BEFORE();
    rl_callback_handler_remove();
    ___TERMIOS_DEBUG_AFTER("rl_callback_handler_remove");

    data->initialized = false;
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
static void sh_cooked_insertchar(int c)
{
    rl_pending_input = c;
    // will read pending
    rl_callback_read_char();
}

/**
 * it seems that if libreadline initalized in any way, stdin should _only_ be
 * read by libreadline. i.e. must use `rl_getc()`
 *
 * libreadline "remaps" '\n' to '\r'. this is done through some
 * tty settings, so even if `fgetc(stdin)` "normaly" returns '\n' for enter
 * key - after libreadline internaly have called rl_prep_term(), the return
 * value will be '\r'.
 * */
static int sh_cooked_getchar(void)
{
    return rl_getc(stdin);
}

void sh_cooked_write(int fd, const void *data, size_t size)
{
    // can not use log module here - recursion
    //
    bool status = _rl_state_save();

    write_all_or_die(fd, data, size);

    _rl_state_restore(status);
}

static const struct shell_mode_s sh_mode_cooked = {
    .init    = sh_cooked_init,
    .exit    = sh_cooked_exit,
    .write   = sh_cooked_write,
    .insert  = sh_cooked_insertchar,
    .getchar = sh_cooked_getchar
};

/// exposed const pointer
const struct shell_mode_s *shell_mode_cooked = &sh_mode_cooked;


static const struct opt_conf sh_cooked_opts_conf[] = {
    {
        .name = "sticky",
        .dest = &sh_cooked_opts.sticky,
        .parse = opt_parse_flag_true,
        .descr = "sticky prompt that keep input characters on same line "
            "(only applies to coocked mode)"
    },
    {
        .name = "prompt",
        .dest = &sh_cooked_opts.prompt,
        .parse = opt_parse_str,
        .descr = "prompt displayed before input."
    }
};

OPT_SECTION_ADD(sh_cooked,
                sh_cooked_opts_conf,
                ARRAY_LEN(sh_cooked_opts_conf),
                NULL);
