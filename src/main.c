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
#include <kernel/string.h>
#include <user/shell.h>
#include <user/syscall.h>
#include <user/syscall-init.h>
#include <lib/stdio.h>
#include <fs/fs.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/inode.h>

void init();
int main(void) {
   put_str("I am kernel\n");
   init_all();
   // intr_enable();
   // process_execute(u_prog_a, "u_prog_a");
   // process_execute(u_prog_b, "u_prog_b");
   // thread_start("k_thread_a", 31, k_thread_a, "I am thread_a");
   // thread_start("k_thread_b", 31, k_thread_b, "I am thread_b");
   // uint32_t fd1 = sys_open("/file1", O_CREATE);
  
   // uint32_t fd2 = sys_open("/file1", O_RDWR);
   // printf("fd1: %d  fd2: %d\n", fd1, fd2);
   // sys_write(fd2, "hello,world\n", 12); // 写文件 fd2!!!
   // sys_write(fd2, "hello,world\n", 12); // 写文件 fd2!!!

   // sys_close(fd1);
   // sys_close(fd2);

   sys_unlink("/prog_pipe");
   uint32_t file_size = 21072;
   uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
   struct disk* sda = &channels[0].devices[0];
   void* prog_buf = sys_malloc(file_size);
   ide_read(sda, 300, prog_buf, sec_cnt);
   int32_t fd = sys_open("/prog_pipe", O_CREATE|O_RDWR);
   
   if (fd != -1) {
      int ret = sys_write(fd, prog_buf, file_size);
      if (ret == -1) {
         printk("file write error!\n");
         while(1);
      }

   }


   cls_screen();
   console_put_str("[zzzzzxy@localhost /]$ ");
   
   // while(1);
   thread_exit(running_thread(), true);

   return 0;
}

// init进程
void init() {
  
   uint32_t ret_pid = fork();
   if (ret_pid) {
      int status;
      int child_pid;
      while(1) {
         // init 线程回收僵尸线程
         child_pid = wait(&status);
         printf("I'm init, my pid is 1. I receive a child %d, it's status is %d.\n", child_pid, status);
      }
      // printf("I am father, my pid is %d, child pid is %d\n", getpid(), ret_pid);
      // while(1);
       
   } else {
      // printf("I am child, my pid is %d, ret pid is %d\n", getpid(), ret_pid);
      my_shell();

   }
   // while(1);
   PANIC("init: should not be here");
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {     
   void* addr1 = sys_malloc(256);
   void* addr2 = sys_malloc(255);
   void* addr3 = sys_malloc(254);
   console_put_str(" thread_a malloc addr:0x");
   console_put_int((int)addr1);
   console_put_char(',');
   console_put_int((int)addr2);
   console_put_char(',');
   console_put_int((int)addr3);
   console_put_char('\n');

   int cpu_delay = 10000000;
   while(cpu_delay-- > 0);
   console_put_str(" thread_a free memory\n");
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     
   void* addr1 = sys_malloc(256);
   void* addr2 = sys_malloc(255);
   void* addr3 = sys_malloc(254);
   console_put_str(" thread_b malloc addr:0x");
   console_put_int((int)addr1);
   console_put_char(',');
   console_put_int((int)addr2);
   console_put_char(',');
   console_put_int((int)addr3);
   console_put_char('\n');

   int cpu_delay = 10000000;

   while(cpu_delay-- > 0);
   console_put_str(" thread_b free memory\n");
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

/* 测试用户进程 */
void u_prog_a(void) {
   void* addr1 = malloc(256);
   void* addr2 = malloc(255);
   void* addr3 = malloc(254);
   printf(" prog_a malloc addr: 0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 10000000;
   while(cpu_delay-- > 0);
   printf(" prog_a free memory\n");
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}

/* 测试用户进程 */
void u_prog_b(void) {
   void* addr1 = malloc(256);
   void* addr2 = malloc(255);
   void* addr3 = malloc(254);
   printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

   int cpu_delay = 10000000;
   while(cpu_delay-- > 0);
   printf(" prog_b free memory\n");
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}
