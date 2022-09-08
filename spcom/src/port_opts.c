#include <libserialport.h>

#include "common.h"
#include "opt.h"
#include "str.h"
#include "strto_r.h"
#include "port.h"
#include "port_info.h"
#include "port_opts.h"

// TODO remove this
#ifndef EENDC
#define EENDC 256
#endif

static struct port_opts_s _port_opts = {
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

// exposed const "read" pointer
const struct port_opts_s *port_opts = &_port_opts;

// clang-format off
static const char* pinstate_words[] =  {
   "0",   "1",
   "low", "high",
   "off", "on"
};
// clang-format on

int port_opts_parse_pinstate(const char *s, int *state)
{
    assert(s);
    assert(state);

    int i = str_list_indexof(s, pinstate_words, ARRAY_LEN(pinstate_words));
    if (i < 0) {
        return -ENOENT;
    }

    // odd words are true
    *state = (i & 1);
    return 0;
}

const char **port_opts_complete_pinstate(const char *s)
{
    return STR_MATCH_LIST(s, pinstate_words);
}

int port_opts_parse_baud(const char *s, int *baud, const char **ep)
{
    assert(s);
    assert(baud);

    int err = 0;

    const char *tmp_ep;
    int tmp_baud = 0;

    err = strto_i(s, &tmp_ep, 10, &tmp_baud);
    if (err)
        return err;

    if (ep)
        *ep = tmp_ep;
    else if (*tmp_ep != '\0')
        return -EENDC;

    if (tmp_baud <= 0)
        return -ERANGE;

    *baud = tmp_baud;
    return 0;
}

static const char *common_bauds[] = {
    "1200",    "1800",    "2400",    "4800",    "9600",    "19200",
    "38400",   "57600",   "115200",  "230400",  "460800",  "500000",
    "576000",  "921600",  "1000000", "1152000", "1500000", "2000000",
    "2500000", "3000000", "3500000", "4000000"
};

const char **port_opts_complete_baud(const char *s)
{
    return STR_MATCH_LIST(s, common_bauds);
}

int port_opts_parse_databits(const char *s, int *databits, const char **ep)
{
    assert(s);
    assert(databits);

    int val = (int)s[0] - '0';
    if ((val < 5) || (val > 9))
        return -ERANGE;
    s++;
    if (ep)
        *ep = s;
    else if (*s != '\0')
        return -EENDC;

    *databits = val;
    return 0;
}

int port_opts_parse_parity(const char *s, int *parity, const char **ep)
{
    assert(s);
    assert(parity);

    switch (toupper(s[0])) {
        case 'E':
            *parity = SP_PARITY_EVEN;
            break;
        case 'O':
            *parity = SP_PARITY_ODD;
            break;
        case 'N':
            *parity = SP_PARITY_NONE;
            break;
        case 'M':
            *parity = SP_PARITY_MARK;
            break;
        case 'S':
            *parity = SP_PARITY_SPACE;
            break;
        default:
            return -EINVAL;
            break;
    }

    s++;

    if (ep)
        *ep = s;
    else if (*s != '\0')
        return -EENDC;

    return 0;
}
//
/* TODO libserialport do not seem to support one and a half stopbits
 * format should be "1.5"? 
 */ 
int port_opts_parse_stopbits(const char *s, int *stopbits, const char **ep)
{
    assert(s);
    assert(stopbits);

    int val = (int)s[0] - '0';
    if ((val < 1) || (val > 2))
        return -ERANGE;

    s++;

    if (ep)
        *ep = s;
    else if (*s != '\0')
        return -EENDC;

    *stopbits = val;
    return 0;
}


/**
 * parse baud and optional databits, parity and stopbits.
 *
 * example:
 * "115200/8N1" --> baud=115200, databits=8, parity=NONE, stopbits=1
 * "9600" --> baud=9600 (remaning not set)
 * "10000:8E1" --> baud=10000, databits=8, parity=EVEN, stopbits=1
 * "115200/" --> error invalid format
 *
 *  result params only set on valid string format, otherwise untouched.
 *
 *  @param parity set to value used by libserialport
 */
int port_opts_parse_baud_dps(const char *s, int *baud, int *databits,
                             int *parity, int *stopbits)
{
    assert(baud);
    assert(databits);
    assert(parity);
    assert(stopbits);

    int err = 0;
    const char *ep;
    int tmp_baud = 0;
    err = port_opts_parse_baud(s, &tmp_baud, &ep);
    if (err)
        return err;
    // have nul
    if (*ep == '\0') {
        *baud = tmp_baud;
        return 0;
    }

    if ((*ep == '/') || (*ep == ':'))
        s = ep + 1;
    else
        return -1; // EFORMAT

    int tmp_databits = 0;
    err = port_opts_parse_databits(s, &tmp_databits, &ep);
    if (err)
        return err;
    s = ep;

    int tmp_parity = 0;
    err = port_opts_parse_parity(s, &tmp_parity, &ep);
    if (err)
        return err;
    s = ep;

    int tmp_stopbits = 0;
    err = port_opts_parse_stopbits(s, &tmp_stopbits, &ep);
    if (err)
        return err;

    *baud = tmp_baud;
    *databits = tmp_databits;
    *stopbits = tmp_stopbits;
    *parity = tmp_parity;

    return 0;
}

#if 0
// clang-format off
static const struct str_kvi _xonoff_map[] = {
    // { "off",     SP_XONXOFF_DISABLED },
    // { "i",     SP_XONXOFF_IN },
    { "in",    SP_XONXOFF_IN },
    { "rx",    SP_XONXOFF_IN },
    // { "o",     SP_XONXOFF_OUT },
    { "out",   SP_XONXOFF_OUT },
    { "tx",    SP_XONXOFF_OUT },
    { "io",    SP_XONXOFF_INOUT },
    { "inout", SP_XONXOFF_INOUT },
    { "txrx",  SP_XONXOFF_INOUT }
};
// clang-format on

/** in libserialport - default is "txrx"=`INOUT` if flowcontrol set to SP_FLOWCONTROL_XONXOFF
 */
int port_opts_parse_xonxoff(const char *s, int *xonxoff)
{
   assert(xonxoff);
   for (int i = 0; i < ARRAY_LEN(_xonoff_map); i++) {
       if (!strcasecmp(s, _xonoff_map[i].s)) {
           *xonxoff = _xonoff_map[i].val;
           return 0;
       }
   }
   return -ENOENT;
}
#endif

#define FLOW_XONXOFF_TX_ONLY (SP_FLOWCONTROL_XONXOFF | (SP_XONXOFF_OUT << 8))
#define FLOW_XONXOFF_RX_ONLY (SP_FLOWCONTROL_XONXOFF | (SP_XONXOFF_IN << 8))

static const struct str_map flowcontrol_map[] = {
    // No flow control
    STR_MAP_INT("none",        SP_FLOWCONTROL_NONE),
    // Hardware flow control using RTS/CTS.
    STR_MAP_INT("rtscts",      SP_FLOWCONTROL_RTSCTS ),
    // Hardware flow control using DTR/DSR
    STR_MAP_INT("dtrdsr",      SP_FLOWCONTROL_DTRDSR ),
    // Software flow control using XON/XOFF characters
    STR_MAP_INT("xonxoff",     SP_FLOWCONTROL_XONXOFF ),
    STR_MAP_INT("xonxoff-tx",  FLOW_XONXOFF_TX_ONLY),
    STR_MAP_INT("xonxoff-out", FLOW_XONXOFF_TX_ONLY),
    STR_MAP_INT("xonxoff-rx",  FLOW_XONXOFF_RX_ONLY),
    STR_MAP_INT("xonxoff-in",  FLOW_XONXOFF_RX_ONLY)
    //{ "xonxoff-txrx", SP_FLOWCONTROL_XONXOFF },
    //{ "xonxoff-inout", SP_FLOWCONTROL_XONXOFF },
};
/* in pyserial flow options named:
 *  xonxoff (bool) – Enable software flow control.
 *  rtscts (bool) – Enable hardware (RTS/CTS) flow control.
 *  dsrdtr (bool) – Enable hardware (DSR/DTR) flow control.
*/
int port_opts_parse_flowcontrol(const char *s, int *flowcontrol)
{
    _Static_assert((unsigned int)SP_XONXOFF_IN < 256, "");
    _Static_assert((unsigned int)SP_XONXOFF_OUT < 256, "");
    _Static_assert((unsigned int)SP_XONXOFF_INOUT < 256, "");
    return str_map_to_int(s,
                          flowcontrol_map,
                          ARRAY_LEN(flowcontrol_map),
                          flowcontrol);
}

const char **port_opts_complete_flowcontrol(const char *s)
{
    return STR_MATCH_LIST(s, flowcontrol_map);
}

static int _parse_cb_devname(const struct opt_conf *conf, char *sval)
{
    _port_opts.name = sval;
    return 0;
}

// positional
static int _parse_cb_baud_dps(const struct opt_conf *conf, char *sval)
{
    int err = 0;
    const char *s = sval;
    const char *ep;
    int tmp_baud = 0;
    err = port_opts_parse_baud(s, &tmp_baud, &ep);
    if (err)
        return opt_perror(conf, "invalid baudrate");

    // have nul
    if (*ep == '\0') {
        _port_opts.baudrate = tmp_baud;
        return 0;
    }

    if ((*ep == '/') || (*ep == ':'))
        s = ep + 1;
    else
        return opt_perror(conf, "invalid baudrate");

    int tmp_databits = 0;
    err = port_opts_parse_databits(s, &tmp_databits, &ep);
    if (err)
        return opt_perror(conf, "invalid databits");
    s = ep;

    int tmp_parity = 0;
    err = port_opts_parse_parity(s, &tmp_parity, &ep);
    if (err)
        return opt_perror(conf, "invalid parity");
    s = ep;

    int tmp_stopbits = 0;
    err = port_opts_parse_stopbits(s, &tmp_stopbits, &ep);
    if (err)
        return opt_perror(conf, "invalid stopbits");

    _port_opts.baudrate = tmp_baud;
    _port_opts.databits = tmp_databits;
    _port_opts.parity = tmp_parity;
    _port_opts.stopbits = tmp_stopbits;

    return 0;
}

static int _parse_cb_baud(const struct opt_conf *conf, char *sval)
{
    return port_opts_parse_baud(sval, &_port_opts.baudrate, NULL);
}

static int _parse_cb_databits(const struct opt_conf *conf, char *sval)
{
    return port_opts_parse_databits(sval, &_port_opts.databits, NULL);
}

static int _parse_cb_stopbits(const struct opt_conf *conf, char *sval)
{
    return port_opts_parse_stopbits(sval, &_port_opts.stopbits, NULL);
}

static int _parse_cb_parity(const struct opt_conf *conf, char *sval)
{
    return port_opts_parse_parity(sval, &_port_opts.parity, NULL);
}

static int _parse_cb_flowcontrol(const struct opt_conf *conf, char *sval)
{
    return port_opts_parse_flowcontrol(sval, &_port_opts.flowcontrol);
}

static int port_opts_post_parse(const struct opt_section_entry *entry)
{
    // note: do not use LOG here
    if (_port_opts.stay) {
        _port_opts.wait = true;
    }

    return 0;
}

static const struct opt_conf port_opts_conf[] = {
    {
        .positional = 1,
        .parse = _parse_cb_devname,
    },
    {
        .positional = 2,
        .parse = _parse_cb_baud_dps,
    },
    {
        .name = "port",
        //.shortname = 'x', // TODO use what?
        .alias = "device",
        .parse = _parse_cb_devname,
        .complete = port_info_complete,
    },
    {
        .name = "baudrate",
        .shortname = 'b',
        .alias = "baud",
        .parse = _parse_cb_baud,
        .complete = port_opts_complete_baud,
    },
    {
        .name = "databits",
        .shortname = 'd',
        .parse = _parse_cb_databits,
    },
    {
        .name = "stopbits",
        .shortname = 's',
        .parse = _parse_cb_stopbits,
    },
    {
        .name = "flow",
        .shortname = 'f',
        .alias = "flowcontrol",
        .parse = _parse_cb_flowcontrol,
    },
    {
        .name = "parity",
        .shortname = 'p',
        .parse = _parse_cb_parity,
    },
    {
        .name = "char-delay",
        .dest = &_port_opts.chardelay,
        .parse = opt_parse_int,
    },
    {
        .name = "wait",
        .dest = &_port_opts.wait,
        .parse = opt_parse_flag_true,
        .descr = "Wait on specified serial port to show up if it do not exists"
    },
    {
        .name = "stay",
        .dest = &_port_opts.stay,
        .parse = opt_parse_flag_true,
        .descr = "Similar to --wait but will never exit if the serial port "
                 "disappears and wait for it to reappear instead"
    },
};

OPT_SECTION_ADD(port,
                port_opts_conf,
                ARRAY_LEN(port_opts_conf),
                port_opts_post_parse);

