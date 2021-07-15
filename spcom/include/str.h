#ifndef STR_INCLUDE_H__
#define STR_INCLUDE_H__
#include <stdbool.h>
#include <stdint.h>

/**
 * str_to functions:
 *    @param ep - endpointer `const char **ep` is optional, if NULL, used
 *    internal endpointer and check that it points to nul byte ('\0').  @return
 *    zero on success
 *
 * string matchers:
 *    @param s if s is a empty string, all possible matches is returned.
 *    @return a NULL terminated list of strings beginning with `s`, or NULL if
 *    none found
 */

/**
 * Decimal (base 10) string to int.
 * handles all checks on errno and endpointer that posix strto family requires.
 */
int str_dectoi(const char *s, int *res, const char **ep);

int str_to_bool(const char *s, bool *rbool);

int str_to_pinstate(const char *s, int *state);
const char **str_match_pinstate(const char *s);

int str_to_baud(const char *s, int *baud, const char **ep);
/// match common baudrates
const char **str_match_baud(const char *s);

int str_to_databits(const char *s, int *databits, const char **ep);

/// @param parity set to value used by libserialport
int str_to_parity(const char *s, int *parity, const char **ep);

int str_to_stopbits(const char *s, int *stopbits, const char **ep);

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
int str_to_baud_dps(const char *s, int *baud, int *databits, int *parity, int *stopbits);

int str_to_xonxoff(const char *s, int *xonxoff);

/// macro to retreive `SP_FLOWCONTROL_...` value from `flowcontrol`
#define FLOW_TO_SP_FLOWCONTROL(X) ((X) & 0xff)
/// macro to retreive `SP_XONOFF_...` value from `flowcontrol`
#define FLOW_TO_SP_XONXOFF(X) (((X) >> 8) & 0xff)

int str_to_flowcontrol(const char *s, int *flowcontrol);
const char **str_match_flowcontrol(const char *s);

// simple destructive to argv parse. will not handle string escaped etc
//int str_to_argv(char *s, int *argc, char **argv, unsigned int max_argc);


int str_0xhextou8(const char *s, uint8_t *res, const char **ep);


char *str_lstrip(char *s);
char *str_rstrip(char *s);
char *str_strip(char *s);

char *str_lstripc(char *s, char c);
char *str_rstripc(char *s, char c);
char *str_stripc(char *s, char c);


/// string to string map item
struct str_kv {
    const char *key;
    const char *val;
};

const struct str_kv *str_kv_lookup(const char *s, const struct str_kv *map, size_t n);

/// string to int map item
struct str_kvi {
    const char *key;
    int val;
};
const struct str_kvi *str_kvi_lookup(const char *s, const struct str_kvi *map, size_t n);
int str_kvi_getval(const char *s, int *val, const struct str_kvi *map, size_t n);

int str_split(char *s, const char *delim, char *toks[], int *n);
int str_split_kv(char *s, struct str_kv *kv);
int str_split_kv_list(char *s, struct str_kv *kvlist, int *n);

int str_split_quoted(char *s, int *argc, char *argv[], int argvmax);
char *str_strcasestr(const char *s, const char *find);
#endif
