#ifndef KEYBIND_INCLUDE_H_
#define KEYBIND_INCLUDE_H_

enum k_action_e {
    K_ACTION_NONE = 0,
    K_ACTION_FWD1,
    K_ACTION_FWD2,
    K_ACTION_EXIT,
    K_ACTION_TOG_CMD_MODE,
};

/*
 * return keybind action K_ACTION_xxx
 */
int keybind_eval(char c, char *cfwd);

#endif
