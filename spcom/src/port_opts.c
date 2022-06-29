#include "opt.h"
#include "str.h"
#include "port.h"
#include "port_info.h"


struct port_opts_s port_opts = {
    /* assume negative values invalid. -1 used to check if options provided and 
     should be set or untouched. */
    .baudrate = -1,
    .databits = -1,
    .stopbits = -1,
    .parity = -1,
    .rts = -1,
    .cts = -1,
    .dtr = -1,
    .dsr = -1,
    //.xonxoff = -1,
    .flowcontrol = -1,
    .signal = -1
};

static int parse_devname(const struct opt_conf *conf, char *sval)
{
    port_opts.name = sval;
    return 0;
}

static int parse_baud_dps(const struct opt_conf *conf, char *sval)
{
    return str_to_baud_dps(sval,
                           &port_opts.baudrate,
                           &port_opts.databits,
                           &port_opts.parity,
                           &port_opts.stopbits);
}

static int parse_baud(const struct opt_conf *conf, char *sval)
{
    return str_to_baud(sval, &port_opts.baudrate, NULL);
}


static int parse_databits(const struct opt_conf *conf, char *sval)
{
    return str_to_databits(sval, &port_opts.databits, NULL);
}

static int parse_stopbits(const struct opt_conf *conf, char *sval)
{
    return str_to_stopbits(sval, &port_opts.stopbits, NULL);
}

static int parse_parity(const struct opt_conf *conf, char *sval)
{
    return str_to_parity(sval, &port_opts.parity, NULL);
}

static int parse_flowcontrol(const struct opt_conf *conf, char *sval)
{
    return str_to_flowcontrol(sval, &port_opts.flowcontrol);
}

static int port_opts_post_parse(const struct opt_section_entry *entry)
{
    // TODO set defaults?
    return 0;
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
        .name = "port",
        //.shortname = 'x', // TODO use what?
        .alias = "device",
        .parse = parse_devname,
        .complete = port_info_complete,
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
        .shortname = 'd',
        .parse = parse_databits,
    },
    {
        .name = "stopbits",
        .shortname = 's',
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
        .descr = "wait on specified serial port to show up if it do not exists"
    },
    {
        .name = "stay",
        .dest = &port_opts.stay,
        .parse = opt_ap_flag_true,
        .descr = "Similar to --wait but will never exit if the serial port disappears and wait for it to reappear instead"
    },
};

OPT_SECTION_ADD(port,
                port_opts_conf,
                ARRAY_LEN(port_opts_conf),
                port_opts_post_parse);

