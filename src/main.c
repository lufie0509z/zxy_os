#include <lib/kernel/print.h>
#include <kernel/init.h>
#include <kernel/debug.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/debug.h>
#include <kernel/interrupt.h>
#include <device/console.h>
#include <kernel/process.h>
#include <kernel/global.h>
#include <kernel/memory.h>
void k_thread_a(void*); // 内核线程
void k_thread_b(void*);

void uprog_a(void*); // 用户进程
void uprog_b(void*);

int test_var_a = 0, test_var_b = 0;

int main(void) {
   put_str("I am kernel\n");

   init_all();
   // console_put_int(test_var_a);
   // console_put_char('\n');
 
   thread_start("k_thread_a", 31, k_thread_a, "argA ");
   thread_start("k_thread_b", 8,  k_thread_b, "argB ");


   //  void* addr = get_kernel_pages(10);
   process_execute(uprog_a, "user_prog_a");
   process_execute(uprog_b, "user_prog_b");
 
   
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
      console_put_str("v_a: 0x");
      console_put_int(test_var_a);
      console_put_char('\n');
   }
}

void k_thread_b(void* arg) {     
   char* para = arg;
   while(1) {
      console_put_str("v_b: 0x");
      console_put_int(test_var_b);
      console_put_char('\n');
   }
}

void uprog_a(void* arg) {
   while (1) {
      test_var_a++;   
   }
}

void uprog_b(void* arg) {
   while (1) {
      test_var_b++;
   }  
}