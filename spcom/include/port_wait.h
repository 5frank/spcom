#ifndef PORT_WAIT_INCLUDE_H__
#define PORT_WAIT_INCLUDE_H__

typedef void(port_wait_cb)(int err);

int port_wait_init(const char *name);

void port_wait_cleanup(void);

void port_wait_start(port_wait_cb *cb);

#endif
