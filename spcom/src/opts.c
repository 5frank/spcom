#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <getopt.h>
#include <assert.h>
#include <stdlib.h>

#include <libserialport.h>

#include "port.h"
#include "utils.h"
#include "shell.h"
#include "str.h"
#include "opts.h"
#include "eol.h"


// clang-format off
enum optno_e {
    /*
     * the following reserved by GNU/POSIX/getopt:
     * 'W', ':', '?'
     *
     * */
    OPT_HELP      = 'h',
    OPT_VERBOSE   = 'v',
    OPT_BAUD      = 'b',
    OPT_LIST      = 'l',
    OPT_FLOW      = 'f',
    OPT_PARITY    = 'p',
    OPT_DATABITS  = 'd',
    OPT_STOPBITS  = 's',
    OPT_CANONICAL = 'C',
    // TODO or 'x' as in gdb? easier to read? "-xsend="  then "-csend"
    OPT_CMD       = 'c',

    // long options only. TODO put first?
    OPT_COLOR = 127,
    OPT_AUTOCOMPLETE,
    OPT_VERSION,
    OPT_WAIT,
    OPT_STAY,
    OPT_TIMEOUT,
    OPT_TIMESTAMP,

    OPT_EOL,
    OPT_EOL_TX,
    OPT_EOL_RX,
    //OPT_WAIT_TIMEOUT,
    OPT_CHARDELAY,
    OPT_STICKY,
    OPT_LOGFILE,
    OPT_LOGLEVEL,
    __OPTNO_MAX,
};
// clang-format on

struct opts_s opts = {
};

static struct option _options[] = {
#if 0 // picocom
    {"receive-cmd", required_argument, 0, 'v'},
    {"send-cmd", required_argument, 0, 's'},
    {"imap", required_argument, 0, 'I' },
    {"omap", required_argument, 0, 'O' },
    {"emap", required_argument, 0, 'E' },
    {"escape", required_argument, 0, 'e'},
    {"no-escape", no_argument, 0, 'n'},
    {"echo", no_argument, 0, 'c'},
    {"noinit", no_argument, 0, 'i'},
    {"noreset", no_argument, 0, 'r'},
    {"hangup", no_argument, 0, 'u'},
    {"nolock", no_argument, 0, 'l'},


    {"logfile", required_argument, 0, 'g'},
    {"initstring", required_argument, 0, 't'},
    {"exit-after", required_argument, 0, 'x'},
    {"exit", no_argument, 0, 'X'},
    {"lower-rts", no_argument, 0, 1},
    {"lower-dtr", no_argument, 0, 2},
    {"raise-rts", no_argument, 0, 3},
    {"raise-dtr", no_argument, 0, 4},
    //{"quiet", no_argument, 0, 'q'},
#endif

    { "help", no_argument, 0, OPT_HELP },
    { "version", no_argument, 0, OPT_VERSION },
    { "ls",     no_argument, 0, OPT_LIST },
    { "autocomplete", required_argument , 0, OPT_AUTOCOMPLETE },

    { "baud", required_argument, 0, OPT_BAUD },
    { "baudrate", required_argument, 0, OPT_BAUD },
    { "flow", required_argument, 0, OPT_FLOW },
    { "parity", required_argument, 0,   OPT_PARITY },
    { "databits", required_argument, 0, OPT_DATABITS },
    { "stopbits", required_argument, 0, OPT_STOPBITS },
    // wait on serial port at startup. also see timeout
    { "wait", no_argument, 0, OPT_WAIT },
     // stay open forever. same as wait, but never exit.
    { "stay", required_argument, 0, OPT_STAY },
    //{ "wait-timeout", required_argument, 0, OPT_WAIT_TIMEOUT },
    { "verbose", no_argument, 0, OPT_VERBOSE },
    { "color", no_argument, 0, OPT_COLOR },
    // application timeout in seconds. useful for batch jobs. 
    { "timeout", required_argument, 0, OPT_TIMEOUT },
    { "timestamp", no_argument, 0, OPT_TIMESTAMP },
    // TODO if canonical set in config file, how to disable with cli arg? optional arg?
    { "canonical", required_argument, 0, OPT_CANONICAL },
    { "cmd", required_argument, 0, OPT_CMD },
    // optional_argument?
    { "sticky", required_argument, 0, OPT_STICKY },
    { "logfile", required_argument, 0, OPT_LOGFILE },
    { "loglevel", required_argument, 0, OPT_LOGLEVEL },
    { "char-delay", required_argument, 0, OPT_CHARDELAY },

    {"eol", required_argument, 0, OPT_EOL},
    {"eol-tx", required_argument, 0, OPT_EOL_TX},
    {"eol-rx", required_argument, 0, OPT_EOL_RX},
#if 0 // TODO
    // send backslash escaped string 
    {"send-e",required_argument, 0, OPT_SEND_ESCAPED },
    { "cmd-on-open", required_argument, 0, OPT_CMD_ON_OPEN },

    {"unbuffered", no_argument, 0, OPT_UNBUFFERED},
    // or ...
    //{"bufsize", required_argument, 0, OPT_BUFSIZE},
    // promt always at last line. unexpected behaviour if combined with unbuffered option
    /*
      Execute a single command.
      May be used multiple times and in conjunction. 
      Note that if combined with `--wait-after-closed`, these commands will be executed again when 
      port is available again. TODO y/n?
     */
    {"cmd-evel", required_argument, 0, OPT_CMD_EVAL},
    {"cmd-file", required_argument, 0, OPT_CMD_FILE},
    // default single backsslash '\'
    {"safe-escape-char", required_argument, 0, OPT_EVAL},
    {"safe-escape-set", required_argument, 0, OPT_EVAL},
#endif
    { 0, 0, 0, 0 }
};

/** premautre optimazation flags :) could also use the `*flag` in getopt_long struct ... */
static struct opts_optno_flags_s {
    unsigned int flags[(__OPTNO_MAX / (sizeof(unsigned int) * 8)) + 1];
} _have;

static inline void _set_optno_flag(unsigned int optno)
{
    const unsigned int nbits = sizeof(_have.flags[0]) * 8;
    unsigned int ary_idx = optno / nbits;
    unsigned int bit_idx = optno & (nbits - 1);
    assert(ary_idx < ARRAY_LEN(_have.flags));
    _have.flags[ary_idx] |= 1 << bit_idx;
}

static inline int _get_optno_flag(unsigned int optno)
{
    const unsigned int nbits = sizeof(_have.flags[0]) * 8;
    unsigned int ary_idx = optno / nbits;
    unsigned int bit_idx = optno & (nbits - 1);
    assert(ary_idx < ARRAY_LEN(_have.flags));
    return _have.flags[ary_idx] & (1 << bit_idx) ? 1 : 0;
}

static int have_opt(unsigned int optno)
{
    return _get_optno_flag(optno);
}

#define NUM_OPTS (sizeof(_options) / sizeof(_options[0]))

static int opt_parse_set_port(const char *s)
{
#ifdef SPCOM_ARGV_COPY
    // TODO remove!?
    // https://stackoverflow.com/questions/10249026/c-why-should-someone-copy-argv-string-to-a-buffer
    port_opts.name = strdup(s);
    if (!port_opts.name)
        return -ENOMEM;
#else
    port_opts.name = s;
#endif
    return 0;
}

static char *opts_mk_optstr(void)
{
    static char optstr[NUM_OPTS * 2] = {};
    char *p = &optstr[0];
    for (int i = 0; i < (NUM_OPTS - 1); i++) {
        int val = _options[i].val;
        if (val >= 256) {
            continue;
        }
        *p++ = (char)val;
        if (_options[i].has_arg == required_argument) {
            *p++ = ':';
        }
    }
    return optstr;
}

static void opts_pretty_argname(int i, char *buf, size_t bufsize)
{
    if ((i < 0) || (i >= NUM_OPTS)) {
        assert(bufsize > 0);
        buf[0] = '\0';
    }

    // static char buf[32] = {0};
    int rc                   = 0;
    const struct option *opt = &_options[i];
    if (isalpha(opt->val))
        rc = snprintf(buf, bufsize, "-%c, --%s", opt->val, opt->name);
    else
        rc = snprintf(buf, bufsize, "%s", opt->name);
    assert(rc > 0);
    assert(rc < bufsize);
}

static int opts_error(const char *msg, const char *argname,
               const char *argval, int err)
{
    fprintf(stderr, "E: %s", msg);
    if (argname)
        fprintf(stderr, " option '%s'", argname);

    if (argval)
        fprintf(stderr, " \"%s\"", argval);
    fprintf(stderr, " (err: %d)\n", err);
    return err;
}

#include "portinfo.h"
#include <string.h>
static void do_autocomplete(const char *s)
{
    if (!s)
        return;

    const char **matches = NULL;
    if (!strcmp(s, "port")) 
        matches = portinfo_match("");
    else
        return;

    for (int i = 0; i < 32; i++) {
        const char *match = matches[i];
        if (!match)
            return;
        fputs(match, stdout);
        fputc(' ', stdout);
    }

    fputc('\n', stdout);
    //exit(0);
}

static int print_help(void) 
{
    printf("help\n");
    return 0;
}

static void check_early_exit_flags(void) 
{
    if (opts.flags.help) {
        print_help();
        exit(0);
    }

    if (opts.flags.version) {
        version_print(opts.verbose);
        exit(0);
    }

    if (opts.flags.list) {
        portinfo_print_list(opts.verbose);
        exit(0);
    }
}

int opt_check_mutual_exclusiv() 
{
    // TODO
    return 0;
}

/* according to freedesktop.org - "If $XDG_CONFIG_HOME is either not set or
 * empty, a default equal to $HOME/.config should be used."
 * "If $XDG_CACHE_HOME is either not set or empty, a default equal to $HOME/.cache should be used. "
 */
int opts_read_conf_file() 
{
    return 0;
}

static void opts_set_defaults(void)
{
    int err;
    if (!have_opt(OPT_EOL) && !have_opt(OPT_EOL_TX)) {
        err = eol_config(EOL_TX, "\n", 1);
        assert(!err);
    }
    if (!have_opt(OPT_EOL) && !have_opt(OPT_EOL_RX)) {
        err = eol_config(EOL_RX, "\n", 1);
        assert(!err);
        err = eol_config(EOL_RX, "\r", 1);
        assert(!err);
    }
}
int opts_parse(int argc, char *argv[])
{
    int err      = 0;
    char *optstr = opts_mk_optstr();
    //printf("%s\n", optstr);

    while (1) {
        // disable error messages printed by getopt
        // opterr = 0;

        int lindex = 0;
        int optno = getopt_long(argc, argv, optstr, _options, &lindex);

        if (optno < 0)
            break; // done

        switch (optno) {
 
            case OPT_AUTOCOMPLETE:
                opts.flags.autocomplete = 1;
                do_autocomplete(optarg);
                exit(0);
                break;
            // ---- "global"/shared opts --------------
            case OPT_HELP:
                opts.flags.help = 1;
                break;
            case OPT_LIST:
                opts.flags.list = 1;
                break;
            case OPT_VERBOSE:
                opts.verbose++;
                break;
            case OPT_VERSION:
                opts.flags.version = 1;
                break;
            case OPT_TIMEOUT:
                err = str_dectoi(optarg, &opts.timeout, NULL); // TODO check not negative
                break;
            // ---- port opts --------------
            case OPT_BAUD:
                err = str_to_baud(optarg, &port_opts.baudrate, NULL);
                break;
            case OPT_DATABITS:
                err = str_to_databits(optarg, &port_opts.databits, NULL);
                break;
            case OPT_STOPBITS:
                err = str_to_stopbits(optarg, &port_opts.stopbits, NULL);
                break;
            case OPT_PARITY:
                err = str_to_parity(optarg, &port_opts.parity, NULL);
                break;
            case OPT_FLOW:
                err = str_to_flowcontrol(optarg, &port_opts.flowcontrol);
                break;
            case OPT_WAIT:
                err = str_to_bool(optarg ? optarg : "y", &port_opts.wait);
                break;
            case OPT_STAY:
                err = str_to_bool(optarg ? optarg : "y", &port_opts.stay);
                break;
            case OPT_CHARDELAY:
                err = str_dectoi(optarg, &port_opts.chardelay, NULL);
                break;

            // ---- eol opts --------------
            case OPT_EOL:
                err = eol_parse_opts(EOL_TX | EOL_RX, optarg);
                break;
            case OPT_EOL_TX:
                err = eol_parse_opts(EOL_TX, optarg);
                break;
            case OPT_EOL_RX:
                err = eol_parse_opts(EOL_RX, optarg);
                break;

            // ---- log opts --------------
            case OPT_LOGFILE:
                opts.logfile = strdup(optarg);
                assert(opts.logfile);
                break;
            case OPT_LOGLEVEL:
                err = str_dectoi(optarg, &opts.loglevel, NULL);
                break;

            // ---- shell opts --------------
            case OPT_CANONICAL:
                err = str_to_bool(optarg, &shell_opts.canonical);
                break;
            case OPT_STICKY:
                err = str_to_bool(optarg, &shell_opts.sticky);
                break;

            case ':': //
            case '?': // error when opterr=0
            default:
                fprintf(stderr, "unrecognized option \"%s\"\n", argv[optind]);
                // return opts_error("invalid value", NULL, optarg, -1);
                // argv[optind]); err = -1;
                return -EINVAL;
                break;
        };

        if (err) {
            char name[32] = { 0 };
            opts_pretty_argname(lindex, name, sizeof(name));
            return opts_error("invalid value", name, optarg, err);
            break;
        }
        else {
            _set_optno_flag(optno);
        }
    }

    check_early_exit_flags();

    const char *arg = NULL;

    if (optind < argc) {
        // TODO check not already set _set_optno_flag(
        arg = argv[optind++];
        err = opt_parse_set_port(arg);
        if (err)
            return opts_error("invalid format", "port", arg, err);
    }

    if (optind < argc) {
        // TODO check not already set _set_optno_flag(
        arg = argv[optind++];
        err = str_to_baud_dps(arg, &port_opts.baudrate, &port_opts.databits, &port_opts.parity, &port_opts.stopbits);
        if (err)
            return opts_error("invalid format", "baud", arg, err);
    }

    if (optind < argc) {
        arg = argv[optind++];
        return opts_error("unrecognized option", arg, NULL, -EINVAL);
    }

    // TODO read config file here XDG_CONFIG_HOME

    if (!port_opts.name)
        return opts_error("No serial device given", NULL, NULL, -1);

    opts_set_defaults();

    return 0;
}
