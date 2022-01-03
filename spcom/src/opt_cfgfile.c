
#define _XOPEN_SOURCE 700
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>

static int parse_error(const char *msg)
{
    fprintf(stderr, "error: %s\n", msg);
    return -1;
}

static int parse_line(char *line)
{

    if (!line[0])
        return 0;

    char *sp = line;

    char *key = NULL;
    char *val = NULL;

    // ==== parse key ========
    for (; *sp != '\0'; sp++) {
        char c = *sp;

        if (c == '#')
            return 0; // only comment on this line

        if (isspace(c))
            continue;

        if (!isprint(c))
            return parse_error("invalid bytes or characters");

        key = sp;
        break;
    }

    if (!key)
        return parse_error("no key");

    // ==== parse equal ========
    char have_equal = 0;
    for (; *sp != '\0'; sp++) {
        char c = *sp;
        if (isspace(c)) {
            *sp = 0;
            continue;
        }

        if (c == ':'  || c == '=') {
            *sp = 0;
            sp++;
            have_equal = c;
            break;
        }
    }
    if (!have_equal)
        return parse_error("missing equal or comma");

    printf("  key='%s'\n", key);
    // ==== parse val ========
    char in_quote = 0;
    char prev_c = 0;
    char c = 0;
    for (; *sp != '\0'; sp++) {

        prev_c = c;
        c = *sp;


        if (!in_quote && (c == '\'' || c == '"')) {
            in_quote = c; // start quote
            *sp = '\0';
            continue;
        }

        if (in_quote && (c == in_quote) && (prev_c != '\\')) {
            in_quote = 0; // end of quote
            *sp = '\0';
            continue;
        }

        if (!in_quote && !val && isspace(c)) {
            *sp = 0;
            continue;
        }

        if (!in_quote && ((c == '#') || isspace(c))) {
            *sp = '\0';
            break;
        }

        if (!isprint(c))
            return parse_error("invalid bytes or characters");

        if (!val) {
            val = sp;
        }
    }

    if (!val)
        return parse_error("no value");

    printf("  val='%s'\n", val);

    return 0;
}

int opt_cfgfile_read(const char *fpath)
{
    int err = 0;

    FILE* fp = fopen(fpath, "r");
    if (!fp) {
        fprintf(stderr, "failed to open %s\n", fpath);
        return -1;
    }

    char buf[255];

    for (int i = 0; i < 1024; i++) {
        char *line = fgets(buf, sizeof(buf), fp);
        if (!line)
            break;

#if 1
        char *ep = strchr(line, '\n');
        if (!ep) {
            fprintf(stderr, "line to long or invalid new line\n");
            break;
        }
        *ep = '\0';
#endif

        if (!line[0])
            continue;

        printf("line='%s'\n", line);
        err = parse_line(line);
        if (err) {
            fprintf(stderr, "on line %d\n", i);
            break;
        }
    }

    fclose(fp);

    return err;
}


/* according to freedesktop.org - "If $XDG_CONFIG_HOME is either not set or
 * empty, a default equal to $HOME/.config should be used."
 * "If $XDG_CACHE_HOME is either not set or empty, a default equal to $HOME/.cache should be used. "
 */
static int opts_cfgfile_locate(void) 
{
    return 0;
}
