
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "assert.h"
#include "opt.h"
#include "log.h"
#include "common.h"
#include "opq.h"
#include "str.h"
#include "cmd.h"
#include "port_opts.h"

struct cmd_ap_s {
    char matchbuf[32];
    enum cmd_src_e cmdsrc;
    int argc;
    char *argv[32]; 
    struct opq *q;
};

struct cmd_s;

#define CMD_FUNC(NAME) int NAME (const struct cmd_s *cmd, struct cmd_ap_s *ap)
typedef int(cmd_callback_fn)(const struct cmd_s *cmd, struct cmd_ap_s *ap);

// TODO what args?

typedef const char **(cmd_complete_fn)(const char *s);

struct cmd_s {
    const char *name;
    int opcode;
    cmd_callback_fn *callback;
    cmd_complete_fn *complete;
    const char *usage;
};

static const char *_matchlist[32];

static struct cmd_ap_s _cmd_ap;

static CMD_FUNC(_cmd_help)
{
    // TODO
    LOG_DBG("help");
    return 0;
}
static CMD_FUNC(_cmd_flush)
{
    // TODO parse i/o
    return opq_enqueue_val(ap->q, OP_PORT_FLUSH, 0);
}
static CMD_FUNC(_cmd_drain)
{
    return opq_enqueue_val(ap->q, OP_PORT_DRAIN, 0);
}
static CMD_FUNC(_cmd_break)
{
    // TODO
    return 0;
}
static CMD_FUNC(_cmd_parity)
{
    // TODO port_opts_parse_parity
    return 0;
}
static CMD_FUNC(_cmd_flow)
{
    // TODO port_opts_parse_flowcontrol
    return 0;
}
static CMD_FUNC(_cmd_set_pinstate) 
{
    return 0;
}

static CMD_FUNC(_cmd_baud)
{
    return 0;
}

static CMD_FUNC(_cmd_send)
{
    bool escaped = false;
    bool hexfmt = false;
    bool sendeol = true; // 'n' disabels it
    int opt;
    // optind not reseted by getopt
    optind = 1;
    while ((opt = getopt(ap->argc, ap->argv, "nehx")) != -1) {
        switch(opt) {
            case 'e':
                escaped = true;
                break;
            case 'n':
                sendeol = false;
                break;
            case 'h':
            case 'x':
                hexfmt = true;
                break;
            default:
                LOG_ERR("invalid send flags");
                return -EINVAL;
        }
    }

    if (escaped && hexfmt) {
        LOG_ERR("invalid combination of send flags");
        return -EINVAL;
    }

    if (optind >= ap->argc) {
        LOG_ERR("missing string argument");
        return -EINVAL;
    }

    while (optind < ap->argc) {
        const char *arg  = ap->argv[optind++];
        //LOG_DBG("optind:%d, argv:%s", optind, arg);
        // TODO 

    }

    if (sendeol)
        opq_enqueue_val(ap->q, OP_PORT_PUT_EOL, 1);

    return 0;
}


static CMD_FUNC(_cmd_sleep) 
{
    LOG_DBG("TODO"); // TODO
    return 0;
}

static CMD_FUNC(_cmd_exit) 
{
    // TODO exit depending on cmd src
    SPCOM_EXIT(0, "user cmd");
    return 0;
}

static struct cmd_s _cmds[] = {
    {
        .name = "help",
        .callback = _cmd_help
    },
    {
        .opcode = OP_PORT_BAUD,
        .name = "baud",
        .callback = _cmd_baud,
        .complete = port_opts_complete_baud,
    },
    {
        .opcode = OP_PORT_FLUSH,
        .name = "flush",
        .callback = _cmd_flush
    },
    {
        .opcode = OP_PORT_DRAIN,
        .name = "drain",
        .callback = _cmd_drain
    },
    {
        .opcode = OP_PORT_BREAK,
        .name = "break", //"break",
        .callback = _cmd_break
    },
    {
        .opcode = OP_PORT_SET_RTS,
        .name = "rts",
        .callback = _cmd_set_pinstate,
        .complete = port_opts_complete_pinstate,
    },
    {
        .opcode = OP_PORT_SET_CTS,
        .name = "cts",
        .callback = _cmd_set_pinstate,
        .complete = port_opts_complete_pinstate,
    },
    {
        .opcode = OP_PORT_SET_DTR,
        .name = "dtr",
        .callback = _cmd_set_pinstate,
        .complete = port_opts_complete_pinstate,
    },
    {
        .opcode = OP_PORT_SET_DSR,
        .name = "dsr",
        .callback = _cmd_set_pinstate,
        .complete = port_opts_complete_pinstate,
    },
    {
        .opcode = OP_PORT_PARITY,
        .name = "parity",
        .callback = _cmd_parity
    },
    {
        .opcode = OP_PORT_WRITE,
        .name = "send",
        .callback = _cmd_send,
        // TODO with or without eol?
        .usage = "send data to serial port. \n"\
        "send [-nexh] STRING\n"\
        "Flags are the same as for common shell `echo`.\n"\
        "`-e` enable backslash escaped string interpretation\n"\
        "`-n` do not output the trailing eol (as specified by `--eol`)\n"\
        "`-x`, `-h` hexadecimal input\n"
    },
    {
        .opcode = OP_PORT_FLOW,
        .name = "flow",
        .callback = _cmd_flow
    },
    {
        .opcode = OP_SLEEP,
        .name = "sleep",
        .callback = _cmd_sleep
    },
    {
        .opcode = OP_EXIT,
        .name = "exit",
        .callback = _cmd_exit
    }
};

static struct cmd_s *cmd_find(const char *name)
{
    for (int i = 0; i < ARRAY_LEN(_cmds); i++) {
        struct cmd_s *cmd = &_cmds[i];
        if (!strcmp(cmd->name, name))
            return cmd;
    }

    return NULL;
}

const char **cmd_match_cmdnames(const char *s)
{
    int slen = strlen(s);
    int n = 0;
    for (int i = 0; i < ARRAY_LEN(_cmds); i++) {

        struct cmd_s *cmd = &_cmds[i];
        if (strncmp(cmd->name, s, slen))
            continue;

        _matchlist[n++] = cmd->name;
        if (n >= (ARRAY_LEN(_matchlist) -1))
            break;
    }
    // n == 0 if no matches - always NULL terminated list
    _matchlist[n] = NULL;
    return _matchlist;
}

const char **cmd_match(const char *s)
{
    int err = 0;
    struct cmd_ap_s *ap = &_cmd_ap;

    if (!s || (*s == '\0'))
        return cmd_match_cmdnames("");

    int n = sizeof(ap->matchbuf) - 1;
    char *mb = strncpy(ap->matchbuf, s, n);
    mb[n] = '\0';


    err = str_split_quoted(mb, &ap->argc, ap->argv, ARRAY_LEN(ap->argv));
    if (err)
        return NULL;
    // ignore argvmax limit. 
    if (!ap->argc)
        return cmd_match_cmdnames("");

    struct cmd_s *cmd = cmd_find(ap->argv[0]);
    if (!cmd)
        return cmd_match_cmdnames(ap->argv[0]); // match parital

    if (!cmd->complete)
        return NULL;

    const char *argstr = ""; // match all
    if (ap->argc > 1)
        argstr = ap->argv[1];

    return cmd->complete(argstr);
}

int cmd_parse(enum cmd_src_e cmdsrc, char *s)
{
    int err = 0;
    struct cmd_ap_s *ap = &_cmd_ap;
    ap->cmdsrc = cmdsrc;

    err = str_split_quoted(s, &ap->argc, ap->argv, ARRAY_LEN(ap->argv));
    if (err)
        return err;

    if (ap->argc <= 0) {
        LOG_ERR("no command name");
        return -1;
    }

    if (ap->argc > (ARRAY_LEN(ap->argv) - 1)) {
        LOG_ERR("too many args");
        return -1;
    }

    struct cmd_s *cmd = cmd_find(ap->argv[0]);
    if (!cmd) {
        LOG_ERR("no such command '%s'", ap->argv[0]);
        return -ENOEXEC;
    }

    switch (cmdsrc) {
        case CMD_SRC_SHELL:
            ap->q = &opq_rt;
            break;
        case CMD_SRC_OPT:
            ap->q = &opq_oo;
            break;
        default:
            assert(0);
            break;
    }

    assert(cmd->callback);
    err = cmd->callback(cmd, ap);

    return err;
}

static int cmd_opt_parse(const struct opt_conf *conf, char *s)
{
    return cmd_parse(CMD_SRC_OPT, s);
}

static const struct opt_conf cmd_opts_conf[] = {
    {
        .name = "cmd",
        .shortname = 'c',
        .parse = cmd_opt_parse,
        .descr = ""
    },
};

OPT_SECTION_ADD(cmd, cmd_opts_conf, ARRAY_LEN(cmd_opts_conf), NULL);

