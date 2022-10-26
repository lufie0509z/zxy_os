#include <user/syscall.h>
#include <user/syscall-init.h>
#include <kernel/thread.h>
#include <lib/kernel/print.h>


#define syscall_nr 32 // 最大支持的系统调用子功能个数
typedef void* syscall;
syscall syscall_table[syscall_nr];


uint32_t sys_getpid() {
    return running_thread()->pid;
}

void syscall_init() {
    put_str("syscall_init start.\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    put_str("syscall_init done.\n");
}

