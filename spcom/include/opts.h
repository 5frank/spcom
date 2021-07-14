
#ifndef __OPTS_INCLUDE__
#define __OPTS_INCLUDE__

int opts_parse(int argc, char *argv[]);

struct opts_s {

    struct opt_flags_s {
        unsigned int help : 1;
        unsigned int list : 1;
        unsigned int version : 1;
        unsigned int autocomplete : 1;
        //unsigned int info : 1;
    } flags;

    int verbose;
    int timeout;
    //const char *logfile;
    //int loglevel;
};
extern struct opts_s opts;

#endif
