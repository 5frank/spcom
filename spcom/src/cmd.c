
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assert.h"
#include "log.h"
#include "utils.h"
#include "cmd.h"
#include "str.h"

struct cmd_ap_s {
    char name[16];
    const char *argstr;
    char *buf;
    size_t bufsize;
};

struct cmd_s;

#define CMD_FUNC(NAME) int NAME (const struct cmd_s *cmd, struct cmd_ap_s *ap, int flags)
typedef int(cmd_callback_fn)(const struct cmd_s *cmd, struct cmd_ap_s *ap, int flags);

// TODO what args?

typedef const char **(cmd_complete_fn)(const char *s);

struct cmd_s {
    const char *name;
    cmd_callback_fn *callback;
    cmd_complete_fn *complete;
    const char *usage;
};


static const char *_matchlist[32];

static struct cmd_ap_s _cmd_ap;

static inline bool _is_termchar(int c, const char* termchars)
{
   if (c == '\0' || c == ' ' || c == '\n')
       return true;

   if (termchars)
        return strchr(termchars, c) ? true : false;

   return false;
}

static CMD_FUNC(_cmd_help)
{
    LOG_DBG("help");
    return 0;
}
static CMD_FUNC(_cmd_flush)
{
    // TODO
    return 0;
}
static CMD_FUNC(_cmd_drain)
{
    // TODO
    return 0;
}
static CMD_FUNC(_cmd_break)
{
    // TODO
    return 0;
}
static CMD_FUNC(_cmd_parity)
{
    // TODO
    return 0;
}
static CMD_FUNC(_cmd_flow)
{
    // TODO
    return 0;
}
static CMD_FUNC(_cmd_set_pinstate) 
{
    const char *s = cmd->name;

#define IS_CMD(A, B, C) ((s[0] == A) && (s[1] == B) && (s[2] == C))

    if (IS_CMD('r', 't', 's')) {
    }
    else if (IS_CMD('c', 't', 's')) {
    }
    else if (IS_CMD('d', 't', 'r')) {
    }
    else if (IS_CMD('d', 's', 'r')) {
    }
    else {
        assert(0);
    }

#undef IS_CMD
    return 0;
}

static CMD_FUNC(_cmd_baud)
{
    return 0;
}

static CMD_FUNC(_cmd_send_w_flags)
{
    // parse remaining commandname here. have "send-e" for example
    const char *s = ap->argstr;
    bool escaped = false;
    bool hexfmt = false;
    bool sendeol = true; // 'n' disabels it
    if (*s == '-') {
        s++;
        while (*s) {
            char c = *s++;
            switch(c) {
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
                case ' ':
                case '\0':
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
    }
    // TODO
    return 0;
}

static CMD_FUNC(_cmd_send) 
{
    LOG_DBG("TODO"); // TODO
    return 0;
}

static CMD_FUNC(_cmd_exit) {
    main_exit(EXIT_SUCCESS);
    return 0;
}

static struct cmd_s _cmds[] = {
    {
        .name = "help",
        .callback = _cmd_help
    },
    {
        .name = "baud",
        .callback = _cmd_baud,
        .complete = str_match_baud,
    },
    {
        .name = "flush",
        .callback = _cmd_flush
    },
    {
        .name = "drain",
        .callback = _cmd_drain
    },
    {
        .name = "break", //"break",
        .callback = _cmd_break
    },
    {
        .name = "rts",
        .callback = _cmd_set_pinstate,
        .complete = str_match_pinstate,
    },
    {
        .name = "cts",
        .callback = _cmd_set_pinstate,
        .complete = str_match_pinstate,
    },
    {
        .name = "dtr",
        .callback = _cmd_set_pinstate,
        .complete = str_match_pinstate,
    },
    {
        .name = "dsr",
        .callback = _cmd_set_pinstate,
        .complete = str_match_pinstate,
    },
    {
        .name = "parity",
        .callback = _cmd_parity
    },
    {
        .name = "send",
        .callback = _cmd_send_w_flags,
        // TODO with or without eol?
        .usage = "send data to serial port. \n"\
        "send[-flags] [data] note ther is no space before flag options. Every"\
        "character after first word , except for first space is sent. i.e."\
        "`send-n  ` will send a single space"\
        "Flags are the same as for common shell `echo`.\n"\
        "`-e` enable backslash escaped string interpretation\n"\
        "`-n` do not output the trailing eol (as specified by `--eol`)\n"\
        "`-x`, `-h` hexadecimal input\n"\
        "example: `send-ne \\t` will send tab"
    },
    {
        //.name = "xonxoff",
        .name = "flow",
        .callback = _cmd_flow
    },
    {
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

static int _copy_argstr(struct cmd_ap_s *ap, const char *s)
{
    const size_t defsize = 256;
    const size_t maxsize = 4096;

    int len = (*s == '\0') ? 0 : strlen(s);
    size_t strsize = len + 1;

    if (strsize > ap->bufsize) {

        char *buf = NULL;
        // multiple of defsize
        size_t bufsize = defsize * ((strsize + defsize) / defsize);

        if (bufsize >= maxsize) {
            LOG_ERR("command string len=%d exceeds max", len);
            return -E2BIG;
        }

        if (!ap->buf) {
            buf = malloc(bufsize);
            if (!buf) {
                LOG_ERR("malloc(%zu) failed", bufsize);
                return -ENOMEM;
            }
        }
        else {
            buf = realloc(ap->buf, bufsize);
            if (!buf) {
                LOG_ERR("realloc(p, %zu) failed", bufsize);
                return -ENOMEM;
            }
        }

        ap->buf = buf;
        ap->bufsize = bufsize;
    }

    strcpy(ap->buf, s);
    ap->argstr = ap->buf;

    return 0;
}

static int cmd_parse_name(struct cmd_ap_s *ap, const char *s)
{
    ap->name[0] = '\0';
    ap->argstr = NULL;
    int err = 0;
    int i = 0;
    const int maxlen = sizeof(ap->name) - 1;
    char *dst = &ap->name[0];

    for (i = 0; i < maxlen; i++) {
        char c = *s++;
        if (_is_termchar(c, "-=")) {
            *dst = '\0';
            break;
        }
        *dst++ = c;
    }

    if (i >= maxlen) {
        LOG_ERR("command name to long");
        return -E2BIG;
    }

    err = _copy_argstr(ap, s);
    if (err) {
        LOG_ERR("copy arg string failed");
        return err;
    }

    return 0;
}

int cmd_run(int cmdsrc, const char *s)
{
    int err = 0;
    struct cmd_ap_s *ap = &_cmd_ap;

    err = cmd_parse_name(ap, s);
    if (err)
        return err;

    struct cmd_s *cmd = cmd_find(ap->name);
    if (!cmd) {
        LOG_ERR("no such command '%s'", ap->name);
        return -ENOEXEC;
    }

    // this should not occur
    assert(cmd->callback);
    err = cmd->callback(cmd, ap, 0); // TODO

    return err;
}


const char **cmd_match(const char *s)
{
    if (!s)
        s = "";

    int err = 0;
    struct cmd_ap_s *ap = &_cmd_ap;

    // TODO? allow cmdname=x
#if 0
    const char *ep = strstr(s, " ");
    // no ep also when empty string
    if (!ep)
        return cmd_match_cmdnames(s);
#else
    if (*s == '\0')
        return cmd_match_cmdnames("");
#endif
    err = cmd_parse_name(ap, s);
    if (err)
        return NULL;
    struct cmd_s *cmd = cmd_find(ap->name);
    if (!cmd)
        return cmd_match_cmdnames(s);
     //   return NULL;
    
    if (!cmd->complete)
        return NULL;

    return cmd->complete(ap->argstr);
}
