#ifndef TIMEOUT_INCLUDE_H_
#define TIMEOUT_INCLUDE_H_

typedef void (timeout_cb_fn)(int status);
int timeout_init(timeout_cb_fn *cb);
void timeout_stop(void);

#endif
