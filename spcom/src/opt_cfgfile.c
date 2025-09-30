/* parse config file */
#include <dirent.h>
#include <stdio.h>
#include <sys/types.h>

static struct {
    const char *config;
    int no_config;
} opt_config_opts;


__attribute__((format(printf, 1, 2)))
static int _parse_error(const char *fmt, ...);
{
    //fprintf(stderr, "error: %s\n", msg);
    fputs("error: ", stderr);
    va_start(args, fmt);
    // or vfdprintf(STDERR_FILENO, fmt, args);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputs("\n", stderr);
    return -1;
}

/* According to freedesktop.org:
 * "If $XDG_CONFIG_HOME is either not set or empty, a default equal to
 *   $HOME/.config should be used."
 */
static const char *_dot_config_fpath(const char *fname)
{
    const char *s0 = NULL;
    const char *s1 = "";

    s0 = getenv("XDG_CONFIG_HOME");
    if (!s0) {

        s0 = getenv("HOME");
        if (!s0) {
            fprintf(stderr, "HOME not set");
            return NULL;
        }

        s1 = "/.config";
    }

    const char *s2 = "/spcom/";

    size_t tot_len = strlen(s0)
            + strlen(s1)
            + strlen(s2)
            + strlen(fname)
            + 1;

    char *fpath = malloc(tot_len);
    assert(fpath);

    int rc = snprintf(fpath, tot_len, "%s%s%s%s", s0, s1, s2, fname);
    assert(rc > 0 && rc < tot_len);

    return fpath;
}

static int _parse_keyval(char *key, char *val)
{
    // TODO
    printf("  val='%s'\n", val);
    return 0;
}

static int _parse_line(char *line)
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
            return _parse_error("invalid bytes or characters");

        key = sp;
        break;
    }

    if (!key)
        return _parse_error("no key");

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
        return _parse_error("missing equal or comma");

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
            return _parse_error("invalid bytes or characters");

        if (!val) {
            val = sp;
        }
    }

    if (!val)
        return _parse_error("no value");

    return _parse_keyval(key, val);
}

static int _parse_file(FILE* fp)
{
    int err = 0;
    char buf[255];

    for (int i = 0; i < 1024; i++) {
        char *line = fgets(buf, sizeof(buf), fp);
        if (!line) {
            break;
        }

        // empty line
        if (!line[0]) {
            continue;
        }
#if 1
        char *ep = strchr(line, '\n');
        if (!ep) {
            _parse_error("line to long or invalid new line");
            break;
        }
        *ep = '\0';
#endif

        printf("line='%s'\n", line);
        err = _parse_line(line);
        if (err) {
            _parse_error("on line %d", i);
            break;
        }
    }


    return err;
}

int opt_config_run(void)
{
    int err = 0;
    char *fpath = NULL;
    bool is_dot_config = false;

    if (opt_config.config) {
        fpath = opt_config.config;
    }
    else if (opt_config.no_config) {
        return 0;
    }
    else {
        fpath = _dot_config_fpath("default.conf");
        if (!fpath) {
            return -1;
        }
        is_dot_config = true;
    }

    FILE* fp = fopen(fpath, "r");
    if (!fp) {
        // ignore optional file in .config not found
        if (errno == ENOENT && is_dot_config) {
            //LOG_DBG("No config at %s", fpath);
            goto done;
        }

        const char *errnomsg = errno ? strerror(errno) : "";
        _parse_error("failed to open %s - %s\n", fpath, errnomsg);
        err = errno ? errno : -1;
        goto done;
    }

    err = _parse_file(fp);

done:

    if (is_dot_config) {
        free(fpath);
    }

    if (fp) {
        fclose(fp);
    }

    return err;
}

static const struct opt_conf opt_config_opts_conf[] = {
    {
        .name = "config",
        .dest = &opt_config.config,
        .parse = opt_parse_str,
        .descr = "config file path. Will override the default config file"
    }.
    {
        .name = "no-config",
        .dest = &opt_config.no_config,
        .parse = opt_parse_true,
        .descr = "do not read default config on startup."
    }
};

OPT_SECTION_ADD(opt_config,
                opt_config_opts_conf,
                ARRAY_LEN(opt_config_opts_conf),
                NULL);
