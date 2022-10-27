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

// void k_thread_a(void*);
// void k_thread_b(void*);
// void u_prog_a(void);
// void u_prog_b(void);

// int prog_a_pid = 0, prog_b_pid = 0;
// int main(void) {
//    put_str("I am kernel\n");
//    init_all();

//    process_execute(u_prog_a, "user_prog_a");
//    process_execute(u_prog_b, "user_prog_b");

   
//    console_put_str(" main_pid:0x");
//    console_put_int(sys_getpid());
//    console_put_char('\n');
//    thread_start("k_thread_a", 31, k_thread_a, "argA ");
//    thread_start("k_thread_b", 31, k_thread_b, "argB ");
//    intr_enable();
//    while(1);
//    return 0;
// }

// /* 在线程中运行的函数 */
// void k_thread_a(void* arg) {     
//    char* para = arg;
//    console_put_str(" thread_a_pid:0x");
//    console_put_int(sys_getpid());
//    console_put_char('\n');
//    // console_put_str(" prog_a_pid:0x");
//    // console_put_int(prog_a_pid);
//    // console_put_char('\n');
//    while(1);
// }

// /* 在线程中运行的函数 */
// void k_thread_b(void* arg) {     
//    char* para = arg;
//    console_put_str(" thread_b_pid:0x");
//    console_put_int(sys_getpid());
//    console_put_char('\n');
//    // console_put_str(" prog_b_pid:0x");
//    // console_put_int(prog_b_pid);
//    // console_put_char('\n');
//    while(1);
// }

// /* 测试用户进程 */
// void u_prog_a(void) {
//    printf(" prog_a_pid:0x%x.\n", getpid());
//    while(1);
// }

// /* 测试用户进程 */
// void u_prog_b(void) {
//    printf(" prog_b_pid:0x%x.\n", getpid());
//    while(1);
// }

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int main(void) {
   put_str("I am kernel\n");
   init_all();

   process_execute(u_prog_a, "user_prog_a");
   process_execute(u_prog_b, "user_prog_b");

   intr_enable();
   console_put_str(" main_pid:0x");
   console_put_int(sys_getpid());
   console_put_char('\n');
   thread_start("k_thread_a", 31, k_thread_a, "argA ");
   thread_start("k_thread_b", 31, k_thread_b, "argB ");
   while(1);
   return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {     
   char* para = arg;
   console_put_str(" thread_a_pid:0x");
   console_put_int(sys_getpid());
   console_put_char('\n');
   while(1);
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     
   char* para = arg;
   console_put_str(" thread_b_pid:0x");
   console_put_int(sys_getpid());
   console_put_char('\n');
   while(1);
}

/* 测试用户进程 */
void u_prog_a(void) {
   printf(" prog_a_pid:0x%x\n", getpid());
   while(1);
}

/* 测试用户进程 */
void u_prog_b(void) {
   printf(" prog_b_pid:0x%x\n", getpid());
   while(1);
}
