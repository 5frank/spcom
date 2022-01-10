#include <stdio.h>
#include "shell.h"
#include "opq.h"
#include "opt.h"

struct {
    int local_echo;
} sh_raw_opts;

static int sh_raw_init(const struct shell_opts_s *opts)
{
    return 0;
}

static void sh_raw_enter(void)
{
    // line buffering off
    setvbuf(stdout, NULL, _IONBF, 0);
}

static void sh_raw_leave(void)
{
    // line buffering on - i.e. back to "normal"
    setvbuf(stdout, NULL, _IOLBF, 0);
}

static int sh_raw_input_putc(char c)
{
    opq_enqueue_val(&opq_rt, OP_PORT_PUTC, c);

    if (sh_raw_opts.local_echo)
        putc(c, stdout);

    return 0;
}

static const struct shell_mode_s sh_mode_raw = {
    .init         = sh_raw_init,
    .enter        = sh_raw_enter,
    .leave        = sh_raw_leave,
    .input_putc   = sh_raw_input_putc,
};

/// exposed const pointer
const struct shell_mode_s *shell_mode_raw = &sh_mode_raw;
