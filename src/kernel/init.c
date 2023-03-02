#include <lib/kernel/print.h>
#include <kernel/interrupt.h>
#include <device/timer.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <device/console.h>
#include <device/keyboard.h>
#include <device/ide.h>
#include <kernel/tss.h>
#include <kernel/init.h>
#include <fs/fs.h>

// extern int prog_a_pid, prog_b_pid;
void init_all() {
    put_str("init_all.\n");
   
    idt_init();
    mem_init();
    thread_init();
    timer_init();
    console_init();
    keyboard_init();
    tss_init();
    syscall_init();   // 初始化系统调用

    intr_enable();    // 后面的ide_init需要打开中断
    ide_init();	      // 初始化硬盘
    filesys_init();   // 初始化文件系统
}
