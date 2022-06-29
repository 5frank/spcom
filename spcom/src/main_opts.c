#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "assert.h"
#include "opt.h"
#include "port_info.h"

static struct global_opts_s _global_opts;

const struct global_opts_s *global_opts = &_global_opts;

static struct main_opts_s {
    int show_help;
    int show_version;
    int show_list;
} main_oflags;

static int main_opts_post_parse(const struct opt_section_entry *entry)
{
    if (main_oflags.show_help) {
        int err = opt_show_help();
        exit(err ? EXIT_FAILURE : 0);
    }

    if (main_oflags.show_version) {
        version_print(_global_opts.verbose);
        exit(0);
    }

    if (main_oflags.show_list) {
        port_info_print_list(_global_opts.verbose);
        exit(0);
    }

    return 0;
}

static int parse_autocomplete(const struct opt_conf *conf, char *s)
{
    assert(s);

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
        .name = "version",
        .dest = &main_oflags.show_version,
        .parse = opt_ap_flag_true,
        .descr = "show version and exit",
    },
    {
        .name = "ls",
        .alias = "list",
        .shortname = 'l',
        .dest = &main_oflags.show_list,
        .parse = opt_ap_flag_true,
        .descr = "list serial port devices. "
            "Combine with verbose option for more detailes",
    },
    {
        .name = "verbose",
        .shortname = 'v',
        .dest = &_global_opts.verbose,
        .parse = opt_ap_flag_count,
        .descr = "verbose output",
    },
    {
        .name = "help",
        .shortname = 'h',
        .dest = &main_oflags.show_help,
        .parse = opt_ap_flag_true,
        .descr = "show help and exit",
    },
    {
        .name = "autocomplete",
        .parse = parse_autocomplete,
        .descr = "Genereate autocomplete list",
    },
};

OPT_SECTION_ADD(main, main_opts_conf, ARRAY_LEN(main_opts_conf), main_opts_post_parse);

