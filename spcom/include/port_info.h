#ifndef PORT_INFO_INCLUDE_H__
#define PORT_INFO_INCLUDE_H__

/**
 * note this will briefly open the port in read mode and could alter the
 * state(s) of RTS, CTS, DTR and/or DSR depending on OS and configuration.
 */
int port_info_print_osconfig(const char *name);


int port_info_print(const char *name, int verbose);

int port_info_print_list(int verbose);

const char** port_info_match(const char *s);

#endif
