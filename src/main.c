#include <lib/kernel/print.h>
#include <kernel/init.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/interrupt.h>
#include <device/console.h>

void k_thread_a(void*);
void k_thread_b(void*);
int main(void) {
   put_str("I am kernel\n");
   init_all();

   // thread_start("k_thread_a", 31, k_thread_a, "argA ");
   // thread_start("k_thread_b", 8, k_thread_b, "ar_gB ");

   intr_enable();	// 打开中断,使时钟中断起作用
   while (1);

   // while(1) {
   //    console_put_str("Main: ");
   // };
   return 0;
}


void k_thread_a(void* arg) {     
   char* para = arg;
   while(1) {
      console_put_str(para);
   }
}

void k_thread_b(void* arg) {     
   char* para = arg;
   while(1) {
      console_put_str(para);
   }
}
