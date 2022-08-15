#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "opt.h"
#include "main_opts.h"

static struct main_opts_s _main_opts = { 0 };
// exposed const pointer
const struct main_opts_s *main_opts = &_main_opts;


static int parse_autocomplete(const struct opt_conf *conf, char *s)
{
    if (!s) {
        exit(-1);
    }

    char *name = s;
    char *start = NULL;

    char *sp = strchr(s, ',');
    if (sp) {
        *sp = '\0';
        sp++;
        start = (sp[0] != '\0') ? sp : NULL;
    }

    const char **list = opt_autocomplete(name, start);
    if (!list)
        goto done;

    while(1) {
        const char *word = *list++;
        if (!word)
            break;

        fputs(word, stdout);
        fputc(' ', stdout);
    }
done:
    exit(0);
    return 0;
}

static const struct opt_conf main_opts_conf[] = {
    {
        .name = "help",
        .shortname = 'h',
        .dest = &_main_opts.show_help,
        .parse = opt_parse_flag_true,
        .descr = "show help and exit",
    },
    {
        .name = "version",
        .dest = &_main_opts.show_version,
        .parse = opt_parse_flag_true,
        .descr = "show version and exit",
    },
    {
        .name = "list",
        .shortname = 'l',
        .dest = &_main_opts.show_list,
        .parse = opt_parse_flag_true,
        .descr = "list serial port devices. "
            "Combine with verbose option for more detailes",
    },
#if 0
    {
        .name = "info",
        //.shortname = 'i',
        .dest = &_main_opts.show_info,
        .parse = opt_parse_flag_true,
        .descr = "show serial port info and exit. "
            "Will briefly open the port to retrive some info",
    },
#endif
    {
        .name = "verbose",
        .shortname = 'v',
        .dest = &_main_opts.verbose,
        .parse = opt_parse_flag_count,
        .descr = "verbose output",
    },
    {
        .name = "autocomplete",
        .parse = parse_autocomplete,
        .descr = "Genereate autocomplete list",
    },
};

OPT_SECTION_ADD(main, main_opts_conf, ARRAY_LEN(main_opts_conf), NULL);

