#ifndef PORT_OPTS_INCLUDE_H_
#define PORT_OPTS_INCLUDE_H_

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

/// exposed const "getter" pointer
extern const struct port_opts_s *port_opts;

int port_opts_parse_pinstate(const char *s, int *state);

const char **port_opts_complete_pinstate(const char *s);

int port_opts_parse_baud(const char *s, int *baud, const char **ep);

/// match common baudrates
const char **port_opts_complete_baud(const char *s);

int port_opts_parse_databits(const char *s, int *databits, const char **ep);

/// @param parity set to value used by libserialport
int port_opts_parse_parity(const char *s, int *parity, const char **ep);

int port_opts_parse_stopbits(const char *s, int *stopbits, const char **ep);

int port_opts_parse_flowcontrol(const char *s, int *flowcontrol);

/**
 * parse baud and optional databits, parity and stopbits.
 *
 * result params only set on valid string format, otherwise untouched.
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
                            int *parity, int *stopbits);

/// macro to retreive `SP_FLOWCONTROL_...` value from `flowcontrol`
#define FLOW_TO_SP_FLOWCONTROL(X) ((X) & 0xff)
/// macro to retreive `SP_XONOFF_...` value from `flowcontrol`
#define FLOW_TO_SP_XONXOFF(X) (((X) >> 8) & 0xff)

const char **port_opts_complete_flowcontrol(const char *s);

#endif

