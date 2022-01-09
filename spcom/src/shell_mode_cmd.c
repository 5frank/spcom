#include <stdlib.h>
#include "cmd.h"
#include "log.h"
#include "opq.h"
#include "assert.h"
#include "shell.h"
#include "shell_rl_utils.h"

static struct {
    char *prompt;
} sh_cmd_opts = {
    .prompt = "[CMD] ",
};

static struct {
    struct readline_state rlstate;
} sh_cmd;

static void sh_cmd_rl_on_newline(char *line)
{
    /* ctrl-C and other control keys trigger this if we do not catch
     * them before readline */
    if (!line) {
        LOG_ERR("null string - uncaught ctrl key not handled by readline?");
        return; // TODO do what?
    }

    size_t len = strlen(line);
    if (len)
        opq_enqueue_hdata(&opq_rt, OP_PORT_WRITE, line, len);
    else
        free(line);

    opq_enqueue_val(&opq_rt, OP_PORT_PUT_EOL, 1);
    LOG_DBG("'%s'", line);
    // no free!

}

static char *sh_cmd_complete_generator(const char *text, int state)
{
    static const char **list = NULL;
    static int index = 0;

    if (!state) {
        list = cmd_match(rl_line_buffer); //text);
        index = 0;
    }
    if (!list)
        return NULL;

    const char *match = list[index++];
    if (!match) {
        // not realy needed
        list = NULL;
        index = 0;
        return NULL;
    }
    // gnureadline will free it
    return strdup(match);
}

/// completer called when in command mode
static char **sh_cmd_complete(const char *text, int start, int end)
{
    // this must be set or readline will abort application
    rl_attempted_completion_over = 1;
    rl_filename_completion_desired = 0;
    return rl_completion_matches(text, sh_cmd_complete_generator);
}

static int sh_cmd_init(const struct shell_opts_s *opts)
{
    struct readline_state *rlstate = &sh_cmd.rlstate;
    shell_rl_state_init_default(rlstate);
    // rl_<globals> will be copied to `rl_state`:s saved
    rl_callback_handler_install(NULL, sh_cmd_rl_on_newline);
    rlstate->attemptfunc = sh_cmd_complete;
    rl_save_state(rlstate);

    return 0;
}

static void sh_cmd_lock(void)
{
    shell_rl_state_save(&sh_cmd.rlstate);
}

static void sh_cmd_unlock(void)
{
    shell_rl_state_restore(&sh_cmd.rlstate);
}

static void sh_cmd_enter(void)
{
    shell_rl_mode_enter(&sh_cmd.rlstate, sh_cmd_opts.prompt);
}

static void sh_cmd_leave(void)
{
    shell_rl_mode_leave(&sh_cmd.rlstate);
}

static int sh_cmd_input_putc(char c)
{
    shell_rl_putc(c);
    return 0;
}

static const struct shell_mode_s sh_mode_cmd = {
    .init         = sh_cmd_init,
    .lock         = sh_cmd_lock,
    .unlock       = sh_cmd_unlock,
    .enter        = sh_cmd_enter,
    .leave        = sh_cmd_leave,
    .input_putc   = sh_cmd_input_putc,
};

/// exposed const pointer
const struct shell_mode_s *shell_mode_cmd = &sh_mode_cmd;

