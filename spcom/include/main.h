#ifndef __MAIN_INCLUDE_H__
#define __MAIN_INCLUDE_H__

struct global_opts_s {
    int verbose;
};

extern const struct global_opts_s *global_opts;
void main_exit(int status);

#endif
