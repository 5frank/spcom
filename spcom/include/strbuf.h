#ifndef STRBUF_INCLUDE_H_
#define STRBUF_INCLUDE_H_
#include <stddef.h>
#include <stdarg.h>
#include <string.h>

 /* premature optimization buffer (mabye) -
 * if shell is async/sticky we need to copy the readline state for
 * every write to stdout. also fputc is slow...
 */

struct strbuf {
    char *buf;
    size_t bufsize;
    size_t len;
    /** callback could either write data in buf to file and set len=0 or realloc by N
     * ans increse bufsize by N */
    void (*make_space_cb)(struct strbuf *sb);
};

#define STRBUF_STATIC_INIT(NAME, SIZE, MAKE_SPACE_CB)                          \
static char ___strbuf_buf_##NAME[SIZE] = {0};                                  \
static struct strbuf NAME = {                                                  \
    .buf = ___strbuf_buf_##NAME,                                               \
    .bufsize = SIZE,                                                           \
    .make_space_cb = MAKE_SPACE_CB                                             \
}

static void strbuf_make_space(struct strbuf *sb)
{
    if (!sb->len) {
        return;
    }

    if (sb->make_space_cb) {
        sb->make_space_cb(sb);
    }
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
        strbuf_make_space(sb);
    }

    sb->buf[sb->len] = c;
    sb->len++;
}

void strbuf_write(struct strbuf *sb, const char *src, size_t size);

void strbuf_vprintf(struct strbuf *sb, const char *fmt, va_list args);

static inline void strbuf_puts(struct strbuf *sb, const char *s)
{
    strbuf_write(sb, s, strlen(s));
}

__attribute__((format(printf, 2, 3)))
void strbuf_printf(struct strbuf *sb, const char *fmt, ...);

#if 1
/**
 * Get end pointer to buffer with at least @param size of space.
 * caller should remember to increase `sb->len` if data written
 * to this pointer
 */ 
static inline char *strbuf_endptr(struct strbuf *sb, int size)
{
    if (size <= strbuf_remains(sb)) {
        return &sb->buf[sb->len];
    }

    strbuf_make_space(sb);

    if (size <= strbuf_remains(sb)) {
        return &sb->buf[sb->len];
    }

    return NULL;

}
#endif

#endif
