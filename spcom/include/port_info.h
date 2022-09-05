#ifndef PORT_INFO_INCLUDE_H_
#define PORT_INFO_INCLUDE_H_

/**
 * note this will briefly open the port in read mode and could alter the
 * state(s) of RTS, CTS, DTR and/or DSR depending on OS and configuration.
 */
int port_info_print_osconfig(const char *name);

int port_info_print(const char *name, int verbose);

int port_info_print_list(int verbose);

const char** port_info_complete(const char *s);

#endif
