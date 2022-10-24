#ifndef __USER_PROCESS_H
#define __USER_PROCESS_H
#include <lib/kernel/stdint.h>
#include <kernel/thread.h>

#define USER_STACK3_VADDR  (0xc0000000 - 0x1000)   // 3 特权级栈所在页的下边界
#define USER_VADDR_START   0x8048000               //用户进程的入口地址
#define default_prio       31

void start_process(void* filename_);
uint32_t* create_page_dir();
void page_dir_activate(struct task_struct* pthread);
void process_activate(struct task_struct* pthread);
void create_user_vaddr_bitmap(struct task_struct* user_prog);
void process_execute(void* filename, char* name);

#endif
