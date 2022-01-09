#ifndef KEYBIND_INCLUDE_H_
#define KEYBIND_INCLUDE_H_
#include <stdint.h>

enum k_action_e {
    K_ACTION_UNCACHE = -1,
    K_ACTION_CACHE = 0,
    K_ACTION_PUTC = 1,
    K_ACTION_EXIT,
    K_ACTION_TOG_CMD_MODE,
};

struct keybind_state {
    uint8_t _seq0;
    char cache[1];
};

/*
 * return keybind action K_ACTION_xxx
 */
int keybind_eval(struct keybind_state *st, char c);
#endif

