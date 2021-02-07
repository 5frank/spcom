#ifndef PORTINFO_INCLUDE_H__
#define PORTINFO_INCLUDE_H__

/**
 * note this will briefly open the port in read mode and could alter the
 * state(s) of RTS, CTS, DTR and/or DSR depending on OS and configuration.
 */
int portinfo_print_osconfig(const char *name);


int portinfo_print(const char *name, int verbose);

int portinfo_print_list(int verbose);

const char** portinfo_match(const char *s);

#endif
