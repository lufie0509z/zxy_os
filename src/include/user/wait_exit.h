#ifndef __USER_WAIT_EXIT_H
#define __USER_WAIT_EXIT_H
#include <kernel/thread.h>
#include <lib/kernel/stdint.h>

pid_t sys_wait(int32_t* status);
void sys_exit(int32_t status);

#endif