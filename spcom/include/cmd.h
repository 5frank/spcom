#ifndef CMD_INCLUDE_H_
#define CMD_INCLUDE_H_

enum cmd_src_e {
    /// command string from startup cli or possibly config file
    CMD_SRC_OPT = 1,
    /// from config file
    CMD_SRC_SHELL = 2
};

int cmd_parse(enum cmd_src_e cmdsrc, char *cmdstr);

/// return NULL terminated list of strings beginning with `s` 
const char **cmd_match(const char *s);

#endif
