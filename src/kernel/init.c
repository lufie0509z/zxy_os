#include <lib/kernel/print.h>
#include <kernel/interrupt.h>
#include <device/timer.h>
#include <kernel/memory.h>
#include <kernel/thread.h>

void init_all() {
    put_str("init_all.\n");
    idt_init();
    timer_init();
    mem_init();
    //thread_init();
}