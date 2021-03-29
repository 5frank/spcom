
#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <uv.h>

#include "assert.h"
#include "cmd.h"
#include "main.h"
#include "opts.h"
#include "port.h"
#include "utils.h"
#include "opq.h"
#include "vt_defs.h"
#include "shell.h"

struct shell_opts_s shell_opts = {
    .canonical = true,
    .keybind = {
        .cmd_enter = VT_CTRL_KEY('B'),
        .cmd_exit = VT_CTRL_KEY('B'),
        .detach = VT_CTRL_KEY('D')
    },
    // TODO from opts
    .prompt = "> ",
    .cmd_prompt = "[CMD] ",
};

static struct {
    bool init;
    char *name;
    char history[256];

    enum shell_mode_e mode;
    struct shell_mode_data_s {
        struct readline_state rlstate;
        const char *prompt;
    } mode_data[__NUM_MODES];

    uv_tty_t stdin_tty;
    char stdin_rd_buf[32];

    uv_tty_t stdout_tty;
    uv_tty_t stderr_tty;
    int prev_c;
} _shell = { 
    .prev_c = -1
};

#if 0
static int shell_error(const char *msg, int rc)
{
   fprintf(stderr, "E: %s (rc: %d) - %s\n", msg, rc, strerror(errno));
   return errno ? -errno : -1;
}

static int uv_isatty(uv_file file)
{
    return (uv_guess_handle(file) == UV_TTY);
}
#endif



static char *_complete_generator(const char *text, int state)
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
static char **_complete_m_cmd(const char *text, int start, int end)
{
    // this must be set or readline will abort application
    rl_attempted_completion_over = 1;
    rl_filename_completion_desired = 0;
    return rl_completion_matches(text, _complete_generator);
}

/// completer called when in canonical mode
static char **_complete_m_canon(const char *text, int start, int end)
{
    // this must be set or readline will abort application
    rl_attempted_completion_over = 1;
    // TODO get complete from cache/history/config/somewhere
    return NULL;
}

static void shell_set_mode(enum shell_mode_e new_m)
{
    // TODO set/unset some rl_callback?
    enum shell_mode_e prev_m = _shell.mode;
    struct shell_mode_data_s *mdp = NULL;
   
    switch (prev_m) {
        case SHELL_M_NONE:
            break;
        case SHELL_M_RAW:
            break;
        
        case SHELL_M_CANON:
        case SHELL_M_CMD:
            mdp = &_shell.mode_data[prev_m];
            rl_save_state(&mdp->rlstate);
            break;
        default:
            assert(0);
            break;
    }

    mdp = NULL;
    switch (new_m) {
        case SHELL_M_RAW:
            rl_replace_line("", 0);
            rl_redisplay();
            break;
        case SHELL_M_CANON:
        case SHELL_M_CMD:
            mdp = &_shell.mode_data[new_m];
            rl_restore_state(&mdp->rlstate);
            // clear. needed?
            rl_replace_line("", 0);
            rl_redisplay();

            //rl_reset_line_state();
            rl_set_prompt(mdp->prompt);
            rl_redisplay();
            break;
        default:
            assert(0);
            break;
    }

    _shell.mode = new_m;
}


static void shell_set_mode_default(void) 
{
    int mode;
    if (shell_opts.canonical)
        mode = SHELL_M_CANON;
    else
        mode = SHELL_M_RAW;
    shell_set_mode(mode);
}


/** as unbuffered stdin and single threaded - use single small static buffer */
static void _uvcb_stdin_stalloc(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
    buf->base = _shell.stdin_rd_buf;
    buf->len  = sizeof(_shell.stdin_rd_buf);
}

static void _uvcb_stdin_read(uv_stream_t *tty_in, ssize_t nread,
                         const uv_buf_t *buf)
{
    if (nread < 0) {
        printf("error \n"); // TODO
        // uv_close((uv_handle_t*) tty_in, NULL);
    } else if (nread == 0) {
        return; // not an error
        // nread might be 0, which does not indicate an error or EOF. This is equivalent
        // to EAGAIN or EWOULDBLOCK under read(2)
    }
    shell_update(buf->base, nread);
}

static void _rlcb_on_newline(char *line)
{
    // TODO sen eol or ignore empty line 
    
    // ctrl-C and other control keys trigger this if we do not catch
    // them before readline
    if (!line) {
        LOG_ERR("null string - uncaught ctrl key not handled by readline?");
        return; // TODO do what?
    }
    switch (_shell.mode) {
        case SHELL_M_RAW:
            LOG_ERR("readline callback in raw mode");
            break;

        case SHELL_M_CANON: 
#if 0
            {
            // TODO move this to port or other module
            int n = strlen(line);
            port_write(line, n);
            // TODO where eol config
            char eol[] = "\n";
            port_write(eol, sizeof(eol));
            }
#else
            opq_push_heapdata(&opq_rt, line, strlen(line));
            opq_push_value(&opq_rt, OP_PORT_PUT_EOL, 1);
            LOG_DBG("'%s'", line);
            // no free!
#endif
            break;

        case SHELL_M_CMD:
            cmd_parse(CMD_SRC_SHELL, line);
            free(line);
            break;

        default:
            assert(0);
            break;
    }
}

/** 
 * TODO would one of these work?:
 *    extern int rl_read_key (void);
 *    extern int rl_getc (FILE *);
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
static inline void _rl_putc(int c)
{
    rl_pending_input = c;
    rl_callback_read_char();
    rl_redisplay(); //needed?
}


static void shell_update_c(int prev_c, int c)
{
    // need to catch ctrl keys before libreadline

    //LOG_DBG("c %d", c);
    /* capture input before readline as it will make it's own iterpretation. also
     * not possible to bind the escape */
    // TODO forward some ctrl:s
    /** UP: 91 65
     * DOWN: 91, 66
     */ 
    switch (c) {
        case VT_CTRL_KEY('D'):
        case VT_CTRL_KEY('C'):
            LOG_DBG("received (CTRL-%c)", VT_CTRL_CHR(c));
            main_exit(EXIT_SUCCESS);
            break;

        default:
            break;
    }

     switch (_shell.mode) {

         case SHELL_M_CMD:
            if (c == shell_opts.keybind.cmd_exit) {
                LOG_DBG("cmd mode exit key=%d", c);
                shell_set_mode_default();
            }
            else
                _rl_putc(c);
            break;

        case SHELL_M_CANON:
            if (c == shell_opts.keybind.cmd_enter) {
                LOG_DBG("cmd mode enter key=%d", c);
                shell_set_mode(SHELL_M_CMD);
            }
            else
                _rl_putc(c);
            break;

        case SHELL_M_RAW:
            if (c == shell_opts.keybind.cmd_enter) {
                LOG_DBG("cmd mode enter key=%d", c);
                shell_set_mode(SHELL_M_CMD);
            }
            else
                //port_putc(c);
                opq_push_value(&opq_rt, OP_PORT_PUTC, c);
            break;

        default:
            assert(0);
            break;
    }
}

int shell_update(const void *user_input, size_t size)
{
    assert(user_input);

    if (!size)
        return 0;
    // TODO prev_c shared between modes --> bug?
    int prev_c = _shell.prev_c;
    const char *p = user_input;
    for (size_t i = 0; i < size; i++) {
        int c = *p++;
        shell_update_c(prev_c, c);
        prev_c = c;
    }

    _shell.prev_c = prev_c;
    return 0;
}

void shell_cleanup(void)
{
    if (!_shell.init)
        return;

    int err = 0;
    // if (_shell.history[0] != '\0')
    //write_history(_shell.history);

#if 0
    // this returns EBADF if tty(s) already closed
    err = uv_tty_reset_mode();
    if (err)
        LOG_UV_ERR(err, "tty mode reset");

#else
    uv_tty_t *p_tty = &_shell.stdin_tty;
    if (uv_is_active((uv_handle_t *)p_tty)) {
        err = uv_tty_set_mode(p_tty, UV_TTY_MODE_NORMAL);
        if (err)
            LOG_UV_ERR(err, "tty mode normal stdin");
        uv_close((uv_handle_t*) p_tty, NULL);
    }
#endif

    rl_message("");
    rl_callback_handler_remove();
    _shell.init = false;
}


struct shell_rls_s {
    struct readline_state rlstate;
};

struct shell_rls_s *shell_rls_save(void)
{
    if (!shell_opts.sticky) 
        return NULL;

    int mode = _shell.mode;
    // no need in raw mode
    if (mode == SHELL_M_RAW)
        return NULL;

    if (RL_ISSTATE(RL_STATE_DONE))
        return NULL;

    struct readline_state *rlstate = &_shell.mode_data[mode].rlstate;
    rl_save_state(rlstate);
    rl_set_prompt("");
    rl_replace_line("", 0);
    rl_redisplay();

    return (struct shell_rls_s *) rlstate;
}

void shell_rls_restore(struct shell_rls_s *rls)
{
    // assume nothing restore
    if (!rls)
        return;

    struct readline_state *rlstate = (struct readline_state *) rls;
    rl_restore_state(rlstate);
    rl_forced_update_display();
}

void shell_printf(int fd, const char *fmt, ...)
{
    va_list args;
    struct shell_rls_s *rls = shell_rls_save();

    va_start(args, fmt);
    vdprintf(fd, fmt, args);
    va_end(args);

    shell_rls_restore(rls);
}
/**
 * relevant part from  gnu/readline `rl_save_state()`:
 * ```
 *   int rl_save_state (struct readline_state *sp) {
 *       ...
 *       sp->entryfunc = rl_completion_entry_function;
 *       sp->menuentryfunc = rl_menu_completion_entry_function;
 *       sp->ignorefunc = rl_ignore_some_completions_function;
 *       sp->attemptfunc = rl_attempted_completion_function;
 *       sp->wordbreakchars = rl_completer_word_break_characters;
 * ```
 */ 
static void shell_init_modes(void)
{
    struct shell_mode_data_s *mdp;

    //  TODO rl_init_history();
    
    // always initalize readline even if raw. need it for command mode
    int err = rl_initialize();
    assert_z(err, "rl_initialize");

    // rl_<globals> will be copied to `rl_state`:s saved
    rl_callback_handler_install(NULL, _rlcb_on_newline);
    rl_catch_signals = 0;
    rl_catch_sigwinch = 0;
    rl_erase_empty_line = 1;
    // Allow conditional parsing of the ~/.inputrc file. 
    rl_readline_name = "spcom";

    for (int i = 0; i < ARRAY_LEN(_shell.mode_data); i++) {
        mdp = &_shell.mode_data[i];
        //rl_attempted_completion_function = _complete_cmd;
        rl_save_state(&mdp->rlstate);

        assert(!mdp->rlstate.catchsigs);
        assert(!mdp->rlstate.catchsigwinch);
    }

    // TODO use rlstate.prompt (not const, need buf anyways)
    mdp = &_shell.mode_data[SHELL_M_RAW];
    mdp->prompt = "";
    //mdp->rlstate.prompt = "";
    //mdp->rlstate.attemptfunc = NULL;

    mdp = &_shell.mode_data[SHELL_M_CANON];
    mdp->prompt = shell_opts.prompt;
    //mdp->rlstate.prompt = _shell.prompt;
    mdp->rlstate.attemptfunc = _complete_m_canon;

    mdp = &_shell.mode_data[SHELL_M_CMD];
    mdp->prompt = shell_opts.cmd_prompt;
    //mdp->rlstate.prompt = shell_opts.cmd_prompt;
    mdp->rlstate.attemptfunc = _complete_m_cmd;


    rl_on_new_line();
    shell_set_mode_default();
}

int shell_init(void)
{
    int err = 0;


    shell_init_modes();

    setlinebuf(stdout);
    //rl_callback_handler_install(NULL, _rlcb_on_newline);

    //rl_attempted_completion_function = _complete_cmd;
    // TODO
    // rl_attempted_completion_function = shell_completion;
    /*_shell.promts[0] = opts.promt;
     *_shell.promts[1] = opts.cmd_promt;
     *shell_set_active_promt(0);*/
    //rl_forced_update_display();

    
    _Static_assert(STDIN_FILENO == 0, "conflicting stdin fileno w libuv");
    
    assert(isatty(STDIN_FILENO)); // should already be checked
    // TODO only tty raw if (isatty(fd)) ...

    // fd = STDOUT_FILENO;
    // fd = STDERR_FILENO;
    
    uv_loop_t *loop = uv_default_loop();
    uv_tty_t *p_tty = &_shell.stdin_tty;

    err = uv_tty_init(loop, p_tty, STDIN_FILENO, 0);
    assert_uv_z(err, "uv_tty_init");

    err = uv_tty_set_mode(p_tty, UV_TTY_MODE_RAW);
    assert_uv_z(err, "uv_tty_set_mode raw");

    assert(uv_is_readable((uv_stream_t *)p_tty));

    err = uv_read_start((uv_stream_t *)p_tty, _uvcb_stdin_stalloc, _uvcb_stdin_read);
    assert_uv_z(err, "uv_read_start");

#if UV_VERSION_GT_OR_EQ(1, 33)
    // enable ansi escape sequence(s) on some windows shells
    uv_tty_set_vterm_state(UV_TTY_SUPPORTED);
    uv_tty_vtermstate_t vtermstate;
    uv_tty_get_vterm_state(&vtermstate);
    (void) vtermstate;
    // fallback to SetConsoleMode(handle, ENABLE_VIRTUAL_TERMINAL_PROCESSING);?
#endif

    _shell.init = true;
    return 0;
}

