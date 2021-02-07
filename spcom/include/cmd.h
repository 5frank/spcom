#ifndef CMD_INCLUDE_H__
#define CMD_INCLUDE_H__

enum cmd_src {
    /// command string from startup cli or possibly config file
    CMD_SRC_OPT = 1,
    /// from config file
    CMD_SRC_SHELL = 2
};

int cmd_run(int cmdsrc, const char *cmdstr);

/// return NULL terminated list of strings beginning with `s` 
const char **cmd_match(const char *s);
#endif
