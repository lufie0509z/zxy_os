#include <kernel/thread.h>
#include <kernel/string.h>
#include <lib/kernel/print.h>
#include <device/console.h>
#include <fs/fs.h>
#include <user/fork.h>
#include <user/syscall.h>
#include <user/syscall-init.h>

#define syscall_nr 32 // 最大支持的系统调用子功能个数
typedef void* syscall;
syscall syscall_table[syscall_nr];


uint32_t sys_getpid() {
    return running_thread()->pid;
}

// uint32_t sys_write(char* str) {
//     console_put_str(str);
//     return strlen(str);
// }

void syscall_init() {
    put_str("syscall_init start.\n");
    syscall_table[SYS_GETPID] = sys_getpid;
    syscall_table[SYS_WRITE]  = sys_write;
    syscall_table[SYS_MALLOC] = sys_malloc;
    syscall_table[SYS_FREE]   = sys_free;
    syscall_table[SYS_FORK]   = sys_fork;
    put_str("syscall_init done.\n");
}

