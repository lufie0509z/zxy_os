#include <lib/kernel/print.h>
#include <kernel/init.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/interrupt.h>
#include <device/console.h>
#include <user/process.h>
#include <kernel/global.h>
#include <kernel/memory.h>
#include <user/syscall.h>
#include <user/syscall-init.h>
#include <lib/stdio.h>

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int main(void) {
   put_str("I am kernel\n");
   init_all();

   // process_execute(u_prog_a, "user_prog_a");
   // process_execute(u_prog_b, "user_prog_b");

   intr_enable();
   // console_put_str(" main_pid:0x");
   // console_put_int(sys_getpid());
   // console_put_char('\n');
   thread_start("k_thread_a", 31, k_thread_a, "I am thread_a ");
   thread_start("k_thread_b", 31, k_thread_b, "I am thread_b ");
   while(1);
   return 0;
}

void k_thread_a(void* arg) {     
   char* para = arg;
   void* addr = sys_malloc(33);
   console_put_str(" I am thread_a, sys_malloc(33), addr is 0x");
   console_put_int((int)addr);
   console_put_char('\n');
   while(1);
}


void k_thread_b(void* arg) {     
   char* para = arg;
   void* addr = sys_malloc(63);
   console_put_str(" I am thread_b, sys_malloc(63), addr is 0x");
   console_put_int((int)addr);
   console_put_char('\n');
   while(1);
}

/* 测试用户进程 */
void u_prog_a(void) {
   char* name = "prog_a";
   printf(" I am %s, my pid:%d%c", name, getpid(),'\n');
   while(1);
}

/* 测试用户进程 */
void u_prog_b(void) {
   char* name = "prog_b";
   printf(" I am %s, my pid:%d%c", name, getpid(), '\n');
   while(1);
}

