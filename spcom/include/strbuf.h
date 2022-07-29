#ifndef STRBUF_INCLUDE_H_
#define STRBUF_INCLUDE_H_
#include <stddef.h>
#include <stdarg.h>

 /* premature optimization buffer (mabye) -
 * if shell is async/sticky we need to copy the readline state for
 * every write to stdout. also fputc is slow...
 * must ensure a flush after strbuf operation(s).
 */

struct strbuf {
    char *buf;
    size_t bufsize;
    size_t len;
    void (*flush_func)(const struct strbuf *sb);
};

#define STRBUF_STATIC_INIT(NAME, SIZE, FLUSH_FUNC)                             \
static char ___strbuf_buf_##NAME[SIZE] = {0};                                  \
static struct strbuf NAME = {                                                  \
    .buf = ___strbuf_buf_##NAME,                                               \
    .bufsize = SIZE,                                                           \
    .flush_func = FLUSH_FUNC                                                   \
}

static void strbuf_flush(struct strbuf *sb)
{
    if (!sb->len) {
        return;
    }

    if (sb->flush_func) {
        sb->flush_func(sb);
    }

    // always reset
    sb->len = 0;
}

static inline void strbuf_reset(struct strbuf *sb)
{
    sb->len = 0;
}

static inline size_t strbuf_remains(const struct strbuf *sb)
{
    return sb->bufsize - sb->len;
}

static inline void strbuf_putc(struct strbuf *sb, char c)
{
    if (strbuf_remains(sb) < 1) {
        strbuf_flush(sb);
    }

    sb->buf[sb->len] = c;
    sb->len++;
}


void strbuf_write(struct strbuf *sb, const char *src, size_t size);

void strbuf_vprintf(struct strbuf *sb, const char *fmt, va_list args);

static inline void strbuf_puts(struct strbuf *sb, const char *s)
{
#if 1
    strbuf_write(sb, s, strlen(s));
#else
    while (*s) {
        strbuf_putc(sb, *s++);
    }
#endif
}

__attribute__((format(printf, 2, 3)))
static inline void strbuf_printf(struct strbuf *sb, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    strbuf_vprintf(sb, fmt, args);
    va_end(args);
}

#if 0
/**
 * Get end pointer to buffer with at least @param size of space.
 * caller should remember to increase `sb->len` if data written
 * to this pointer
 */ 
static inline char *strbuf_endptr(struct strbuf *sb, int size)
{
    if (size > strbuf_remains(sb)) {
        return &sb->buf[sb->len];
    }

    strbuf_flush(sb);

    if (size > strbuf_remains(sb)) {
        return NULL;
    }

    return &sb->buf[0];

}
#endif

#endif
