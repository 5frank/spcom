#ifndef PORT_INCLUDE_H_
#define PORT_INCLUDE_H_

/** data only valid in callback and do not need to be freed */
typedef void (port_rx_cb_fn)(const void *data, size_t size);
void port_init(port_rx_cb_fn *rx_cb);
void port_cleanup(void);
// TODO
int port_write(const void *data, size_t size);
int port_putc(int c);
/// write string and EOL
int port_write_line(const char *line);

#endif
