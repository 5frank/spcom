
#ifndef PORT_INCLUDE_H__
#define PORT_INCLUDE_H__

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
    //int xonxoff;
    int flowcontrol;
    int signal;
};

extern struct port_opts_s port_opts;

int port_open(void);
void port_close(void);
// TODO
int port_write(const void *data, size_t size);
int port_putc(int c);
/// write string and EOL
int port_write_line(const char *line);

#endif
