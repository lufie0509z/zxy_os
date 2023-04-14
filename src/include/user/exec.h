#ifndef __USER_EXEC_H
#define __USER_EXEC_H
#include <lib/kernel/stdint.h>

int32_t sys_execv(const char* pathname, const char* argv[]);
#endif