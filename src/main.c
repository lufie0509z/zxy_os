#include <lib/kernel/print.h>
#include <kernel/init.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/thread.h>

void k_thread_a(void*);

void main(void) {
    put_str("I am kernel.\n");
    init_all();
    // ASSERT(1 == 2);
    // put_str("Init finished.\n");
    // void* vaddr = get_kernel_pages(3);
    // put_str("\n get_kernel_page start vaddr is "); 
    // put_int((uint32_t)vaddr);  
    // put_str("\n");
    // asm volatile ("sti");
    // put_str("Turn on the interrupt.\n");
    thread_start("k_thread_a", 31, k_thread_a, "agr");
    while (1);
    return 0;
}

void k_thread_a(void* arg) {
    char* para = arg;
    while (1)
    {
        put_str(para);
    }
    
}