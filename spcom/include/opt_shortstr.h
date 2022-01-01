#ifndef OPT_SHORTSTR_INCLUDE_H_
#define OPT_SHORTSTR_INCLUDE_H_
/** returns NULL if @param c i zero, otherwise returns a static allocated and
 * nul terminated string of length 1 with c as first char */
const char *opt_shortstr(char c);

#endif
