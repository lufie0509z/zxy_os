# include <lib/kernel/print.h>
# include "kernel/init.h"

void main(void) {
    put_str("I am kernel.\n");
    init_all();
    put_str("Init finished.\n");
    asm volatile ("sti");
    put_str("Turn on the interrupt.\n");
    while (1);
}