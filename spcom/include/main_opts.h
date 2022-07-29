#ifndef MAIN_OPTS_INCLUDE_H_
#define MAIN_OPTS_INCLUDE_H_

struct main_opts_s {
    int show_help;
    int show_version;
    int show_list;
    int verbose;
};

const struct main_opts_s *main_opts_get(void);
#endif
