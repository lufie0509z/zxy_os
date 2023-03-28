#ifndef __USER_SYSCALL_H
#define __USER_SYSCALL_H
#include <lib/kernel/stdint.h>
#include <kernel/global.h>
#include <kernel/thread.h>

enum SYSCALL_NR {
    SYS_GETPID,
    SYS_WRITE,
    SYS_MALLOC,
    SYS_FREE,
    SYS_FORK
};

uint32_t getpid(void);
uint32_t write(int32_t fd, const void* buf, uint32_t cnt);

void* malloc(uint32_t size);
void free(void* ptr);

pid_t fork();
#endif