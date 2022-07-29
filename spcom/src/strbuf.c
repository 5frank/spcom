#include <string.h>
#include <stdlib.h>

#include "strbuf.h"
#include "common.h"
#include "assert.h"

// TODO using assert here might cause recursion loop on exit


void strbuf_write(struct strbuf *sb, const char *src, size_t size)
{
    while (1) {
        // max_size likley changed after strbuf_flush()
        size_t max_size = strbuf_remains(sb);
        size_t chunk_size = (size <= max_size) ? size : max_size;

        memcpy(&sb->buf[sb->len], src, size);
        sb->len += chunk_size;

        if (chunk_size >= size) {
            break;
        }

        size -= chunk_size;
        src += chunk_size;

        strbuf_flush(sb);
    }
}

static void strbuf_vprintf_retry_with_malloc(struct strbuf *sb, size_t size,
                                             const char *fmt, va_list args)
{
    char *buf = malloc(size);
    assert(buf);

    int len = vsnprintf(buf, size, fmt, args);

    assert(len >= 0);
    assert(len < size);

    strbuf_write(sb, buf, len);

    free(buf);
}

static void strbuf_vprintf_retry_with_flush(struct strbuf *sb, size_t size,
                                            const char *fmt, va_list args)
{
    strbuf_flush(sb);

    char *dst = &sb->buf[sb->len];
    size_t dstsize = strbuf_remains(sb);
    assert(dstsize >= size);

    int len = vsnprintf(dst, dstsize, fmt, args);
    assert(len >= 0);
    assert(len < dstsize);

    sb->len += len;
}

/**
 * note: can not use `va_list` twice.
 * see: https://stackoverflow.com/questions/55274350
 * args should only be used once as vsnprintf (and vfprintf, vprintf, vsprintf)
 * can invalidate args.  need to use va_copy in that case.
 */
void strbuf_vprintf(struct strbuf *sb, const char *fmt, va_list args)
{
    char *dst = &sb->buf[sb->len];
    size_t dstsize = strbuf_remains(sb);

    va_list args2;
    va_copy(args2, args);

    int len = vsnprintf(dst, dstsize, fmt, args);
    assert(len >= 0);

    if (len < dstsize) {
        // success
        sb->len += len;
        goto done;
    }

    size_t size_needed = len + 1;

    if (size_needed > sb->bufsize) {
        // large string
        strbuf_vprintf_retry_with_malloc(sb, size_needed, fmt, args2);
    }
    else if (size_needed > dstsize) {
        strbuf_vprintf_retry_with_flush(sb, size_needed, fmt, args2);
    }

done:
    va_end(args2); // free
}

