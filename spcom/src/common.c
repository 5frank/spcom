#include "common.h"
#include "assert.h"

static int _write_all(int fd, const void *data, size_t size)
{
    const char *p = data;

    while (1) {
        int rc = write(fd, p, size);
        if (rc < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }

            return -errno;
        }

        if (rc >= size) {
            return 0;
        }

        p += rc;
        size -= rc;
    }
}

void write_all_or_die(int fd, const void *data, size_t size)
{
    int err = _write_all(fd, data, size);
    assert(!err);
}
