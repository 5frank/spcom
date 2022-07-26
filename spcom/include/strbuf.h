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

static inline void strbuf_puts(struct strbuf *sb, const char *s)
{
    while (*s) {
        strbuf_putc(sb, *s++);
    }
}

void strbuf_vprintf(struct strbuf *sb, const char *fmt, va_list args);

/**
 * workaround as can not use `va_list` twice.
 * see: https://stackoverflow.com/questions/55274350
 * args should only be used once as vsnprintf (and vfprintf, vprintf, vsprintf)
 * can invalidate args.  need to use va_copy in that case.
 *
    possible (premature) optimizations: 
    // FMT lilkley to be compile time constant, if it isnt it doesnt matter
        size_t _fmtsize = sizeof(FMT); 
        size_t _vaargc = COUNT_ARGUMENTS(__VA__ARGS__);
        if (strbuf_remains(sb) < (_fmtsize + 2*_vaargc))
            strbuf_flush(sb)
 */
#define strbuf_printf(SB, FMT, ...)  do {                                      \
                                                                               \
    struct strbuf *_sb = SB;                                                   \
                                                                               \
    for (int _i = 0; _i < 2; _i++) {                                           \
                                                                               \
        char *_dst = &_sb->buf[sb->len];                                       \
        size_t _remains = strbuf_remains(_sb);                                 \
        int _rc = snprintf(_dst, _remains, FMT, __VA_ARGS__);                  \
                                                                               \
        if (_rc < 0) {                                                         \
            _rc = 0; /* TODO fmt error */                                      \
            break;                                                             \
        }                                                                      \
                                                                               \
        if (_rc >= _remains) {                                                 \
            /* didn't fit - try again after flush */                           \
            strbuf_flush(_sb);                                                 \
            continue;                                                          \
        }                                                                      \
                                                                               \
        _sb->len += _rc;                                                       \
        break; /* success if we get here */                                    \
    }                                                                          \
                                                                               \
} while (0)

#endif
