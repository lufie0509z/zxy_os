#ifndef __USER_SHELL_H
#define __USER_SHELL_H
#include <fs/fs.h>

extern char final_path[MAX_PATH_LEN];

void print_prompt();
void my_shell();

#endif