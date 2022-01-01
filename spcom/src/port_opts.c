#include "opt.h"
#include "str.h"

struct port_opts_s {
    /* i.e. device path on unix */
    const char *name; 
    int baudrate;
    int databits;
    int stopbits;
    int parity;
    int rts;
    int cts;
    int dtr;
    int dsr;
    int flowcontrol;
    int signal;
    int chardelay;
    int wait;
    int stay;
};

static struct port_opts_s port_opts;
//extern const struct port_opts_s *g_port_opts = &port_opts;

static int parse_devname(struct opt_context *ctx,
                         const struct opt_conf *conf,
                         char *sval)
{
    port_opts.name = sval;
    return 0;
}

static int parse_baud_dps(struct opt_context *ctx,
                         const struct opt_conf *conf,
                         char *sval)
{
    return str_to_baud_dps(sval,
                           &port_opts.baudrate,
                           &port_opts.databits,
                           &port_opts.parity,
                           &port_opts.stopbits);
}

static int parse_baud(struct opt_context *ctx,
                         const struct opt_conf *conf,
                         char *sval)
{
    return str_to_baud(sval, &port_opts.baudrate, NULL);
}


static int parse_databits(struct opt_context *ctx,
                         const struct opt_conf *conf,
                         char *sval)
{
    return str_to_databits(sval, &port_opts.databits, NULL);
}

static int parse_stopbits(struct opt_context *ctx,
                         const struct opt_conf *conf,
                         char *sval)
{
    return str_to_stopbits(sval, &port_opts.stopbits, NULL);
}

static int parse_parity(struct opt_context *ctx,
                         const struct opt_conf *conf,
                         char *sval)
{
    return str_to_parity(sval, &port_opts.parity, NULL);
}

static int parse_flowcontrol(struct opt_context *ctx,
                         const struct opt_conf *conf,
                         char *sval)
{
    return str_to_flowcontrol(sval, &port_opts.flowcontrol);
}

static const struct opt_conf port_opts_conf[] = {
    {
        .positional = 1,
        .parse = parse_devname,
    },
    {
        .positional = 2,
        .parse = parse_baud_dps,
    },
    {
        .name = "device",
        .alias = "port",
        .parse = parse_devname,
    },
    {
        .name = "baudrate",
        .shortname = 'b',
        .alias = "baud",
        .parse = parse_baud,
        .complete = str_match_baud,
    },
    {
        .name = "databits",
        .shortname  = 'd',
        .parse = parse_databits,
    },
    {
        .name = "stopbits",
        .shortname  = 's',
        .parse = parse_stopbits,
    },
    {
        .name = "flow",
        .shortname = 'f',
        .alias = "flowcontrol",
        .parse = parse_flowcontrol,
    },
    {
        .name = "parity",
        .shortname = 'p',
        .parse = parse_parity,
    },
    {
        .name = "char-delay",
        .dest = &port_opts.chardelay,
        .parse = opt_ap_int,
    },
    {
        .name = "wait",
        .dest = &port_opts.wait,
        .parse = opt_ap_flag_true,
    },
    {
        .name = "stay",
        .dest = &port_opts.stay,
        .parse = opt_ap_flag_true,
    },
};

OPT_SECTION_ADD(port, port_opts_conf, ARRAY_LEN(port_opts_conf), NULL);

