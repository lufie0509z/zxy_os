#ifndef __USER_SYSTEMCALLINIT_H
#define __USER_SYSTEMCALLINIT_H
#include <lib/kernel/stdint.h>

void syscall_init();
uint32_t sys_getpid();

#endif