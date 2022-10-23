#include <lib/kernel/print.h>
#include <kernel/interrupt.h>
#include <device/timer.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <device/console.h>
#include <device/keyboard.h>
#include <kernel/tss.h>
#include <kernel/init.h>
extern int test_var_a, test_var_b;
void init_all() {
    put_str("init_all.\n");
    test_var_a = test_var_b = 0;
    idt_init();
    mem_init();
    thread_init();
    timer_init();
    console_init();
    keyboard_init();
    tss_init();
}
