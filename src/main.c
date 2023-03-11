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
#include <user/syscall.h>
#include <user/syscall-init.h>
#include <lib/stdio.h>
#include <fs/fs.h>
#include <fs/file.h>

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int main(void) {
   put_str("I am kernel\n");
   init_all();
   // intr_enable();
  
   process_execute(u_prog_a, "u_prog_a");
   process_execute(u_prog_b, "u_prog_b");
   thread_start("k_thread_a", 31, k_thread_a, "I am thread_a");
   thread_start("k_thread_b", 31, k_thread_b, "I am thread_b");
   
   
   uint32_t fd1 = sys_open("/file1", O_CREATE);
   sys_close(fd1);

   uint32_t fd2 = sys_open("/file1", O_RDWR);

   // printf("open /file1, fd:%d\n", fd2); 
   sys_write(fd2, "hello,zhang\n", 12); // 写文件 fd2!!!
   sys_write(fd2, "zxyzxyzxyzxy", 12); // 写文件 fd2!!!
   sys_write(fd2, "hello,world\n", 12); // 写文件 fd2!!!
   sys_close(fd2);

   uint32_t fd3 = sys_open("/file1", O_RDWR);

   int read_bytes;
   
   char buf[64] = {0};
   read_bytes = sys_read(fd3, buf, 24);
   // ide_read(cur_part->my_disk, 0x6ac, buf, 1);
   printf("1_ read %d bytes:\n%s", read_bytes, buf);

   memset(buf, 0, 64);
   read_bytes = sys_read(fd3, buf, 12);
   printf("2_ read %d bytes:\n%s", read_bytes, buf);

   memset(buf, 0, 64);
   read_bytes = sys_read(fd3, buf, 6);
   printf("3_ read %d bytes:\n%s", read_bytes, buf);

   // printf("________  close file1 and reopen  ________\n");
   // sys_close(fd3);
   // uint32_t fd4 = sys_open("/file1", O_RDWR);
   printf("________ SEEK_SET 0 ________\n");
   memset(buf, 0, 64);
   sys_sleek(fd3, 0, SEEK_SET);
   read_bytes = sys_read(fd3, buf, 24);
   printf("4_ read %d bytes:\n%s", read_bytes, buf);

   sys_close(fd3);

   // sys_close(fd1);

   while(1);
   return 0;
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
