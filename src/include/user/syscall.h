#ifndef __USER_SYSCALL_H
#define __USER_SYSCALL_H
#include <lib/kernel/stdint.h>

enum SYSCALL_NR {
    SYS_GETPID
};

uint32_t getpid(void);


#endif