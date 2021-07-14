
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <libserialport.h>
#include "assert.h"
#include "str.h"
#include "vt_defs.h"
#include "utils.h"

/// unknown parse failure
#define STR_EUNKNOWN (300)
/// parsing did not end at nul byte
#define STR_ENONUL (301)
/// string is not a number (no digits)
#define STR_ENAN (302)
/// invalid format (incorrect delimiter etc)
#define STR_EFMT (303)
/// string do not match 
#define STR_ENOMATCH (304)
// number out of allowed range. (same as ERANGE on most platforms)
#define STR_ERANGE (34)
// string not valid. (same as EINVAL on most platforms)
#define STR_EINVAL (22)
// something did not fit (same as E2BIG on most platforms)
#define STR_E2BIG  (7)

//#define STR_ENO0xPREFIX (305)


static const char *_matchresult[32];

static inline bool _is_termchar(int c)
{
   if (c == '\0' || c == ' ' || c == '\n')
       return true;
   return false;
}

#define _MATCH(S, LIST) _match_list(S, LIST, sizeof(LIST[0]), ARRAY_LEN(LIST))
/// assumes string of interest first in item
static const char **_match_list(const char *s, const void *items, size_t itemsize, size_t numitems)
{
    if (!s)
        s = "";

    int n = 0;
    int slen = strlen(s);

    // need a char pointer as increments of sizeof(char) and not sizeof(char *)
    const char *item = items;

    for (int i = 0; i < numitems; i++) {
        /* string pointer is the dereferenced value at begining at of every item
         but dereferencing it is not pretty. sorry! */
        const char *name = *((const char **)item);

        // have a item at every itemsize chunk
        item += itemsize;

        if (!name)
            break;

        if (strncmp(name, s, slen))
            continue;

        _matchresult[n++] = name;
        if (n >= (ARRAY_LEN(_matchresult) -1))
            break;
    }
    // n = 0 if no matches
    _matchresult[n] = NULL;

    return _matchresult;
}
// str_kv_getv
static int str_kvi_lookup(const char *s, int *res, const struct str_kvi *map, size_t maplen)
{

   for (int i = 0; i < maplen; i++) {
       if (!strcasecmp(s, map[i].key)) {
           *res = map[i].val;
           return 0;
       }
   }

   return STR_ENOMATCH;
}

int _lookup_oddtruelist(const char *s, int *res, const char *list[], size_t listlen)
{
   for (int i = 0; i < listlen; i++) {
       if (!strcasecmp(s, list[i])) {
           // odd words are true 
           *res = (i & 1);
           return 0;
       }
   }

   return STR_ENOMATCH;
}
/** decimal string to int **/
int str_dectoi(const char *s, int *res, const char **ep)
{
    char *tmp_ep;
    // TODO restore errno?
    errno = 0;
    long int val = strtol(s, &tmp_ep, 10);
    if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
        return STR_ERANGE;

    if (errno != 0 && val == 0)
        return STR_EUNKNOWN;

    // no digits found
    if (tmp_ep == s)
        return STR_ENAN;

    if (ep)
        *ep = tmp_ep;
    else if (*tmp_ep != '\0')
        return STR_ENONUL;

    *res = (int) val;

    return 0;
}

// clang-format off
static const char* bool_words[] =  {
   "0",     "1",
   "n",     "y",
   "no",    "yes",
   "false", "true",
   "off",   "on"
};
// clang-format on

int str_to_bool(const char *s, bool *rbool) 
{
    assert(s);
    assert(rbool);
    int tmp  = 0;
    int err = _lookup_oddtruelist(s, &tmp, bool_words, ARRAY_LEN(bool_words));
    if (err)
        return err;
    *rbool = tmp;
    return 0;
}

const char **str_match_bool(const char *s)
{
    return _MATCH(s, bool_words);
}

// clang-format off
static const char* pinstate_words[] =  {
   "0",   "1",
   "low", "high",
   "off", "on"
};
// clang-format on

int str_to_pinstate(const char *s, int *state)
{
    assert(s);
    assert(state);

    return _lookup_oddtruelist(s, state, pinstate_words, ARRAY_LEN(pinstate_words));
}

const char **str_match_pinstate(const char *s)
{
    return _MATCH(s, pinstate_words);
}

int str_to_baud(const char *s, int *baud, const char **ep)
{
    assert(s);
    assert(baud);

    int err = 0;

    const char *tmp_ep;
    int tmp_baud = 0;

    err = str_dectoi(s, &tmp_baud, &tmp_ep);
    if (err)
        return err;

    if (ep)
        *ep = tmp_ep;
    else if (*tmp_ep != '\0')
        return STR_ENONUL;

    if (tmp_baud <= 0)
        return STR_ERANGE;

    *baud = tmp_baud;
    return 0;
}

static const char *common_bauds[] = {
    "1200", "1800", "2400", "4800", "9600", "19200", "38400", "57600",
    "115200", "230400", "460800", "500000", "576000", "921600", "1000000",
    "1152000", "1500000", "2000000", "2500000", "3000000", "3500000", "4000000"
};

const char **str_match_baud(const char *s)
{
    return _MATCH(s, common_bauds);
}

int str_to_databits(const char *s, int *databits, const char **ep)
{
    assert(s);
    assert(databits);

    int val = (int) s[0] - '0';
    if ((val < 5) || (val > 9))
        return STR_ERANGE;
    s++;
    if (ep)
        *ep = s;
    else if (*s != '\0')
        return STR_ENONUL;

    *databits = val;
    return 0;
}

int str_to_parity(const char *s, int *parity, const char **ep)
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
            return STR_EINVAL;
            break;
    }

    s++;

    if (ep)
        *ep = s;
    else if (*s != '\0')
        return STR_ENONUL;

    return 0;
}
//
/* TODO libserialport do not seem to support one and a half stopbits
 * format should be "1.5"? 
 */ 
int str_to_stopbits(const char *s, int *stopbits, const char **ep)
{
    assert(s);
    assert(stopbits);

    int val = (int) s[0] - '0';
    if ((val < 1) || (val > 2))
        return STR_ERANGE;

    s++;

    if (ep)
        *ep = s;
    else if (*s != '\0')
        return STR_ENONUL;

    *stopbits = val;
    return 0;
}


/**
 * result params only set on valid string format, otherwise untouched.
 */
int str_to_baud_dps(const char *s, int *baud, int *databits, int *parity, int *stopbits)
{
    assert(baud);
    assert(databits);
    assert(parity);
    assert(stopbits);

    int err = 0;
    const char *ep;
    int tmp_baud = 0;
    err = str_to_baud(s, &tmp_baud, &ep);
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
        return STR_EFMT;

    int tmp_databits = 0;
    err = str_to_databits(s, &tmp_databits, &ep);
    if (err)
        return err;
    s = ep;

    int tmp_parity = 0;
    err = str_to_parity(s, &tmp_parity, &ep);
    if (err)
        return err;
    s = ep;

    int tmp_stopbits = 0;
    err = str_to_stopbits(s, &tmp_stopbits, &ep);
    if (err)
        return err;

    *baud     = tmp_baud;
    *databits = tmp_databits;
    *stopbits = tmp_stopbits;
    *parity   = tmp_parity;

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
int str_to_xonxoff(const char *s, int *xonxoff)
{
   assert(xonxoff);
   for (int i = 0; i < ARRAY_LEN(_xonoff_map); i++) {
       if (!strcasecmp(s, _xonoff_map[i].s)) {
           *xonxoff = _xonoff_map[i].val;
           return 0;
       }
   }
   return STR_EINVAL;
}
#endif

#define FLOW_XONXOFF_TX_ONLY (SP_FLOWCONTROL_XONXOFF | (SP_XONXOFF_OUT << 8))
#define FLOW_XONXOFF_RX_ONLY (SP_FLOWCONTROL_XONXOFF | (SP_XONXOFF_IN << 8))

static const struct str_kvi flowcontrol_map[] = {
    // No flow control
    { "none", SP_FLOWCONTROL_NONE },
    // Hardware flow control using RTS/CTS. 
    { "rtscts", SP_FLOWCONTROL_RTSCTS },
    // Hardware flow control using DTR/DSR
    { "dtrdsr", SP_FLOWCONTROL_DTRDSR },
    // Software flow control using XON/XOFF characters
    { "xonxoff", SP_FLOWCONTROL_XONXOFF },
    { "xonxoff-tx", FLOW_XONXOFF_TX_ONLY},
    { "xonxoff-out", FLOW_XONXOFF_TX_ONLY},
    { "xonxoff-rx", FLOW_XONXOFF_RX_ONLY},
    { "xonxoff-in", FLOW_XONXOFF_RX_ONLY}
    //{ "xonxoff-txrx", SP_FLOWCONTROL_XONXOFF },
    //{ "xonxoff-inout", SP_FLOWCONTROL_XONXOFF },
};
/* in pyserial flow options named:
 *  xonxoff (bool) – Enable software flow control.
 *  rtscts (bool) – Enable hardware (RTS/CTS) flow control.
 *  dsrdtr (bool) – Enable hardware (DSR/DTR) flow control.
*/
int str_to_flowcontrol(const char *s, int *flowcontrol)
{
    _Static_assert((unsigned int) SP_XONXOFF_IN < 256, "");
    _Static_assert((unsigned int) SP_XONXOFF_OUT < 256, "");
    _Static_assert((unsigned int) SP_XONXOFF_INOUT < 256, "");
    return str_kvi_lookup(s, flowcontrol, flowcontrol_map, ARRAY_LEN(flowcontrol_map));
}

const char **str_match_flowcontrol(const char *s)
{
    return _MATCH(s, flowcontrol_map);
}
/**
 * parse string in format like "C-c" (Control + C) to VT100 compatible value.
 * also accept "ESC"
 */ 
int str_parse_bindkey(const char *s, int *vtkey)
{
    int vtkey_tmp = 0;
    switch (*s) {
        case 'C':
            s++;
            if (*s != '-')
                return STR_EINVAL;

            s++;
            if (!islower(*s))
                return STR_EINVAL;

            vtkey_tmp = VT_CTRL_KEY(*s);

            s++;
            if (*s != '\0')
                return STR_ENONUL;
        break;
#if 0
        case 'e':
        case 'E':
            if (strcasecmp("esc", s)) {
                return STR_EINVAL;
            }
            vtkey_tmp = VT_ESC_KEY;
        break;
#endif
        default:
            return STR_EINVAL;
            break;
    }

    *vtkey = vtkey_tmp;
    return 0;
}


static int str_to_argv(char *s, int *argc, char **argv, unsigned int max_argc)
{
    int err = 0;
    int n = 0;
    bool have_arg = false;
    while (1) {
        char c = *s;
        if (_is_termchar(c)) {
            *s = '\0'; // could be other then '\0'
            break;
        }

        if (isspace(c)) {
            *s++ = '\0';
            have_arg = false;
            continue;
        }

        if (!have_arg) {
            argv[n++] = s;
            if (n >= max_argc) {
                err = STR_E2BIG;
                break;
            }
            have_arg = true;
        }
        s++;
    };

    *argc   = n;
    argv[n] = 0;

    return err;
}

static int str_hex2nib(char sc, uint8_t *b)
{
    uint8_t c = sc;
    if ((c >= '0') && (c <= '9')) {

        *b = (c - '0');
        return 0;
    }

    unsigned char lc = c | (1 << 5); // to lower
    if ((lc >= 'a') && (lc <= 'f')) {
        *b = c - 'a' + 10;
        return 0;
    }

    return -1;
}

int str_0xhextou8(const char *s, uint8_t *res, const char **ep) 
{
    int err;
    uint8_t b = 0;
    if (*s != '0') 
        return EINVAL;
    s++;

    if (*s != 'x') 
        return EINVAL;
    s++;

    err = str_hex2nib(*s, &b);
    if (err)
        return EINVAL;
    b <<= 4;
    s++;

    err = str_hex2nib(*s, &b);
    if (err)
        return EINVAL;
    s++;

    if (isxdigit(*s))
        return EINVAL; // ambigous end

    *res = b;

    if (ep)
        *ep = s;

    return 0;
}



char *str_lstrip(char *s) 
{
    while(isspace(*s)) s++;
    return s;
}

char *str_rstrip(char *s) 
{
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back + 1) = '\0';
    return s;
}

char *str_strip(char *s) 
{
    return str_rstrip(str_lstrip(s)); 
}

char *str_lstripc(char *s, char c)
{
    while(*s == c) s++;
    return s;
}

char *str_rstripc(char *s, char c)
{
    char* back = s + strlen(s);
    while(*--back == c);
    *(back + 1) = '\0';
    return s;
}

char *str_stripc(char *s, char c)
{
    return str_rstripc(str_lstripc(s, c), c);
}

static int str_is0xbyte(const char *s)
{
    if (!s || s[0] == '\0')
        return false;

    if (*s != '0')
        return false;
    s++;

    if (*s != 'x')
        return false;
    s++;


    if (!isxdigit(*s))
        return false;
    s++;

    if (isxdigit(*s))
        s++;

    if (*s != '\0')
        return false;

    return true;
}

int str_split(char *s, const char *delim, char *toks[], int *n)
{
   const int nmax = *n; 
   *n = 0;
    while (1) {
        char *tok = strsep(&s, delim);

        if (!tok)
            break;

        if (tok[0] == '\0')
            continue;

        if (*n >= nmax)
            return E2BIG;

        toks[*n] = tok;
        (*n)++;
    }

    return 0;
}

int str_split_kv(char *s, struct str_kv *kv) 
{
    char *toks[2];
    int n = ARRAY_LEN(toks);
    int err = str_split(s, ":", toks, &n);
    if (err)
        return err;

    if (n != ARRAY_LEN(toks))
        return EINVAL;

    kv->key = str_strip(toks[0]);
    kv->val = str_strip(toks[1]);

    return 0;
}

int str_split_kv_list(char *s, struct str_kv *kvlist, int *n)
{
    int err = 0;
    const int nmax = *n;
    *n = 0;
    while (1) {
        char *tok = strsep(&s, ",");

        if (!tok)
            break;

        if (tok[0] == '\0')
            continue;

        if (*n >= nmax)
            return E2BIG;

        struct str_kv *kv = &kvlist[*n];
        err = str_split_kv(tok, kv);
        if (err)
            break;

        (*n)++;
    }

    return err;
}

const struct str_kv *str_kv_get(const char *s, const struct str_kv *items, int nitems)
{
    for (int i = 0; i < nitems; i++) {
        const struct str_kv *kv = &items[i];
        if (!strcasecmp(s, kv->key))
            return kv;
    }

    return NULL;
}
