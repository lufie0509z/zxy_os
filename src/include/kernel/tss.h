#ifndef __KERNEL_TSS_H
#define __KERNEL_TSS_H
#include <kernel/thread.h>

void update_tss_esp(struct task_thread* pthread);
void tss_init();

#endif