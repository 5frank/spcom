
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <libserialport.h>
#include "assert.h"
#include "common.h"
#include "str.h"
#include "vt_defs.h"

/// unknown parse failure
#define STR_EUNKNOWN (300)
/// unexpected character at end of input string
#define STR_EEND (301)
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

/**
 * @return true if c is a "human visible" character.
 * space, tabs and line breaks not considerd as "visible".  */
static inline int _isvisible(char c)
{
    // same as (c >= '!' && c <= '~')
    return (c >= 0x21 && c <= 0x7E);
}
#if 0
static int _have_0x_prefix(const char *s)
{
    return (s[0] == '0' && s[1] == 'x');
}
#endif

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
#if 1

const struct str_map *str_map_lookup(const char *s,
                                     const struct str_map *map,
                                     size_t map_len)
{
    for (unsigned int i = 0; i < map_len; i++) {
        const struct str_map *itm = &map[i];
        if (!strcasecmp(s, itm->key)) {
            return itm;
        }
    }

   return NULL;
}
#else
const struct str_kvi *str_kvi_lookup(const char *s, const struct str_kvi *map, size_t n)
{
   for (int i = 0; i < n; i++) {
        const struct str_kvi *kvi = &map[i];
       if (!strcasecmp(s, kvi->key)) {
           return kvi;
       }
   }

   return NULL;
}

int str_kvi_getval(const char *s, int *val, const struct str_kvi *map, size_t n)
{
    const struct str_kvi *kvi = str_kvi_lookup(s, map, n);
    if (!kvi)
        return STR_ENOMATCH;

    *val = kvi->val;
    return 0;
}

const struct str_kv *str_kv_lookup(const char *s, const struct str_kv *map, size_t n)
{
   for (int i = 0; i < n; i++) {
        const struct str_kv *kv = &map[i];
       if (!strcasecmp(s, kv->key)) {
           return kv;
       }
   }

   return NULL;
}
#endif
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

int str_dectol(const char *s, long int *res, const char **ep)
{
    if (!s || !res) {
        return -EINVAL;
    }
    // TODO? restore errno
    char *tmp_ep;
    errno = 0;
    long int val = strtol(s, &tmp_ep, 10);
    if (errno) {
        return STR_ERANGE;
    }

    if (tmp_ep == s) {
        return STR_ENAN;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (_isvisible(*tmp_ep)) {
        return STR_EEND;
    }

    *res = val;

    return 0;
}

int str_hextoul(const char *s, unsigned long int *res, const char **ep)
{
    if (!s || !res) {
        return -EINVAL;
    }

    char *tmp_ep = NULL;
    errno = 0;
    unsigned long int val = strtoul(s, &tmp_ep, 16);
    if (errno) {
        return STR_ERANGE;
    }

    if (tmp_ep == s) {
        return STR_ENAN;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (_isvisible(*tmp_ep)) {
        return STR_EEND;
    }

    *res = val;

    return 0;
}

/** decimal string to int **/
int str_dectoi(const char *s, int *res, const char **ep)
{
    long int tmp = 0;
    int err = str_dectol(s, &tmp, ep);
    if (err) {
        return err;
    }

    if (tmp < INT_MIN || tmp > INT_MAX) {
        return STR_ERANGE;
    }

    *res = tmp;

    return 0;
}

int str_hextou8(const char *s, uint8_t *res, const char **ep)
{
    unsigned long int tmp = 0;
    int err = str_hextoul(s, &tmp, ep);
    if (err) {
        return err;
    }

    if (tmp > UINT8_MAX) {
        return STR_ERANGE;
    }

    *res = tmp;

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
        return STR_EEND;

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
        return STR_EEND;

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
        return STR_EEND;

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
        return STR_EEND;

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
int str_to_flowcontrol(const char *s, int *flowcontrol)
{
    _Static_assert((unsigned int) SP_XONXOFF_IN < 256, "");
    _Static_assert((unsigned int) SP_XONXOFF_OUT < 256, "");
    _Static_assert((unsigned int) SP_XONXOFF_INOUT < 256, "");
    return str_map_to_int(s,
                          flowcontrol_map,
                          ARRAY_LEN(flowcontrol_map),
                          flowcontrol);
}

const char **str_match_flowcontrol(const char *s)
{
    return _MATCH(s, flowcontrol_map);
}

int str_0xhextou8(const char *s, uint8_t *res, const char **ep)
{
    if (*s != '0')
        return EINVAL;
    s++;

    if (*s != 'x')
        return EINVAL;
    s++;

    return str_hextou8(s, res, ep);
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

#if 0
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
#endif

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

/// note: does not nul terminate
static int _escape_nonprint_char(char c, char *buf)
{
    static const char * hexlut = "0123456789ABCDEF";

    char *p = &buf[0];

    if (isprint(c) && c != '\\') {
        *p++ = c;
        return 1;
    }

    switch (c) {
        case '\a':
            *p++ = '\\';
            *p++ = 'a';
            return 2;
        case '\b':
            *p++ = '\\';
            *p++ = 'b';
            return 2;
        case '\t':
            *p++ = '\\';
            *p++ = 't';
            return 2;
        case '\n':
            *p++ = '\\';
            *p++ = 'n';
            return 2;
        case '\v':
            *p++ = '\\';
            *p++ = 'v';
            return 2;
        case '\f':
            *p++ = '\\';
            *p++ = 'f';
            return 2;
        case '\r':
            *p++ = '\\';
            *p++ = 'r';
            return 2;
        case '\\':
            *p++ = '\\';
            *p++ = '\\';
            return 2;
    }

    unsigned char byte = c;
    *p++ = '\\';
    *p++ = hexlut[(byte >> 4) & 0x0F];
    *p++ = hexlut[(byte >> 0) & 0x0F];
    return 3;
}

int str_escape_nonprint(char *dst, size_t dstsize,
                        const char *src, size_t srcsize)
{
    int totlen = 0;
    while (1) {

        if (!srcsize)
            break;

        if (dstsize < 4)
            break;

        int len = _escape_nonprint_char(*src, dst);
        dst += len;
        dstsize -= len;

        src += 1;
        srcsize -= 1;

        totlen += len;

    }

    *dst = '\0';
    return totlen;
}

