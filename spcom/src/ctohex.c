#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <stdio.h>

#include "opt.h"
#include "assert.h"
#include "ctohex.h"

#define HEX_CHR_LUT_UPPER "0123456789ABCDEF"
#define HEX_CHR_LUT_LOWER "0123456789abcdef"

#define CTOHEX_DEFAULT_FMT "\\%02x"

struct ctohex_table_s {
    struct {
        char str[CTOHEX_BUF_SIZE];
    } c[UCHAR_MAX + 1];
};

static struct ctohex_table_s *ctohex_table = NULL;

static struct {
    char hexfmt[16];
} ctohex_opts = {0};

static int ctohex_default(char c, char *buf)
{
    char *p = buf;
    const char *lut = "0123456789ABCDEF";
    unsigned char b = c;

#if 1
    // should match CTOHEX_DEFAULT_FMT 
    *p++ = '\\';
    *p++ = 'x';
    *p++ = lut[b >> 4];
    *p++ = lut[b & 0x0f];
    *p = '\0';
#else
    *p++ = '[';
    *p++ = lut[b >> 4];
    *p++ = lut[b & 0x0f];
    *p++ = ']';
    *p = '\0';
#endif
    return 4;
}



static void ctohex_init_table(const char *fmt)
{
    if (!ctohex_table) {
        ctohex_table = malloc(sizeof(*ctohex_table));
        assert(ctohex_table);
    }

    const int maxlen = CTOHEX_BUF_SIZE;
    for (unsigned int i = 0; i <= UCHAR_MAX; i++) {

        char *dest = ctohex_table->c[i].str;
        int rc = snprintf(dest, maxlen, fmt, i);
        assert(rc > 0);
        //printf("val=%u, rc='%d' -- '%s'\n", i, rc, ctohex_table[i]);
        assert(rc < maxlen);
    }
}

int ctohex(char c, char *dest)
{
    if (!ctohex_table)
        return ctohex_default(c, dest);

    unsigned char tblidx = c;
    const char *src = ctohex_table->c[tblidx].str;

    for (int len = 0; len < CTOHEX_BUF_SIZE; len++) {
        dest[len] = src[len];

        if (src[len] == '\0') {
            return len;
        }
    }

    assert(0);
    return 0; // never?
}

/// @return zero if valid format
static int check_valid_hexfmt(const char *fmt)
{
    if (!fmt)
        return 1;

    const char *sp = fmt;
    int len = 0;
    int percent_count = 0;

    for (; *sp != '\0'; len++) {
        char c = *sp++;
        if (c == '*')
            return -EINVAL;

        if (c == '%')
            percent_count++;

        /* isprint() returns
         * ... false (zero) for: '\n', '\r', '\t'
         * ... true (non-zero) for: ' ' */ 
        if (!isprint(c))
            return -EINVAL;
    }
    if (len < 2)
        return -EINVAL;

    size_t maxlen = sizeof(ctohex_opts.hexfmt) - 1;
    if (len >= maxlen)
        return -EINVAL;

    if (percent_count != 1)
        return -EINVAL;

    char buf[CTOHEX_BUF_SIZE];

    int rc = snprintf(buf, sizeof(buf), fmt, 0xAB);
    if (rc < 1)
        return -EINVAL;

    if (rc >= sizeof(buf))
        return -ERANGE;

    if (!strstr(buf, "ab") && !strstr(buf, "AB") && !strstr(buf, "171"))
        return -EINVAL;

    return 0;
}

/// only initialize table once on post parse
static int ctohex_opt_post_parse(const struct opt_section_entry *entry)
{
    if (!ctohex_opts.hexfmt[0])
        return 0; // use default

    ctohex_init_table(ctohex_opts.hexfmt);

    return 0;
}

static int parse_hexfmt(const struct opt_conf *conf, char *s)
{
    if (check_valid_hexfmt(s))
        return opt_error(conf, "invalid format");
    // assume check len in _is_valid_hexfmt()
    strcpy(ctohex_opts.hexfmt, s);
    return 0;
}

static const struct opt_conf ctohex_opts_conf[] = {
    {
        .name = "hexformat",
        .alias = "hexfmt",
        .parse = parse_hexfmt,
        .descr = "hex printf format for byte values."
            "Default '" CTOHEX_DEFAULT_FMT "'",
    }
};

OPT_SECTION_ADD(outfmt,
                ctohex_opts_conf,
                ARRAY_LEN(ctohex_opts_conf),
                ctohex_opt_post_parse);

