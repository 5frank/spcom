
#ifndef PORT_INCLUDE_H__
#define PORT_INCLUDE_H__
#include <stdbool.h>

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

int port_init(void);
void port_cleanup(void);
// TODO
int port_write(const void *data, size_t size);
int port_putc(int c);
/// write string and EOL
int port_write_line(const char *line);

#endif
