#include <lib/kernel/print.h>
#include <kernel/interrupt.h>

void init_all() {
    put_str("init_all.\n");
    idt_init();
}