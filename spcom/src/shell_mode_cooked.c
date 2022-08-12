#include <stdlib.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "assert.h"
#include "log.h"
#include "opq.h"
#include "shell.h"
#include "shell_rl.h"

static struct {
    char *prompt;
    int sticky;
} sh_cooked_opts = {
    .prompt = "",
};

static struct {
    struct readline_state rlstate;
} sh_cooked;

static void sh_cooked_rl_on_newline(char *line)
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

/// completer called in cooked mode
static char **sh_cooked_complete(const char *text, int start, int end)
{
    // this must be set or readline will abort application
    rl_attempted_completion_over = 1;
    // TODO get complete from cache/history/config/somewhere
    return NULL;
}
#if 0
static int sh_cooked_init(const struct shell_opts_s *opts)
{
    sh_cooked_opts.sticky = opts->sticky;

    // rl_<globals> will be copied to `rl_state`:s saved
    struct readline_state *rlstate = &sh_cooked.rlstate;

    shell_rl_state_init_default(rlstate);
    rl_callback_handler_install(NULL, sh_cooked_rl_on_newline);
    // Allow conditional parsing of the ~/.inputrc file. 
    rl_readline_name = "spcom";
    rlstate->attemptfunc = sh_cooked_complete;
    using_history();    // initialize history
    rl_save_state(rlstate);

    return 0;
}
#endif

static const struct shell_mode_s sh_mode_cooked = {
    .lock    = shell_rl_save,
    .unlock  = shell_rl_restore,
    .enter   = shell_rl_enable,
    .leave   = shell_rl_disable,
    .insert  = shell_rl_insertchar,
    .getchar = shell_rl_getchar
};

/// exposed const pointer
const struct shell_mode_s *shell_mode_cooked = &sh_mode_cooked;
