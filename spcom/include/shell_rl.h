
/** lib readline wrapper */

#ifndef SHELL_RL_INCLUDE_H_
#define SHELL_RL_INCLUDE_H_

const void *shell_rl_save();
void shell_rl_restore(const void *x);

void shell_rl_enable(void);
void shell_rl_disable(void);

void shell_rl_cleanup(void);

void shell_rl_insertchar(int c);
int shell_rl_getchar(void);

#endif
