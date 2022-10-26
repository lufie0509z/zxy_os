#include <lib/kernel/print.h>
#include <kernel/interrupt.h>
#include <device/timer.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <device/console.h>
#include <device/keyboard.h>
#include <kernel/tss.h>
#include <kernel/init.h>

extern int prog_a_pid, prog_b_pid;
void init_all() {
    put_str("init_all.\n");
    prog_a_pid = 0;
    prog_b_pid = 0;
    idt_init();
    mem_init();
    thread_init();
    timer_init();
    console_init();
    keyboard_init();
    tss_init();
    syscall_init();   // 初始化系统调用
}
