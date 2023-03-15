#ifndef _KERNEL_THREAD_H
#define _KERNEL_THREAD_H
#include <lib/kernel/stdint.h>
#include <kernel/global.h>
#include <kernel/list.h>
#include <kernel/memory.h>
#include <lib/kernel/bitmap.h>

#define PG_SIZE 4096
typedef void thread_func(void*);
typedef int16_t pid_t;

#define MAX_FILES_OPEN_PER_PROC 8 // 每个进程最多打开文件的数量
//进程的状态
enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

//中断栈，按此结构压入上下文寄存器
struct intr_stack {
    uint32_t vec_no;
    uint32_t edi;  
    uint32_t esi;  
    uint32_t ebp;  
    uint32_t esp_dummy;  
    uint32_t ebx;  
    uint32_t edx;  
    uint32_t ecx;  
    uint32_t eax;  
    uint32_t gs;  
    uint32_t fs;  
    uint32_t es;  
    uint32_t ds;

    //下面的属性由CPU从低特权级进入高特权级时压入
    uint32_t err_code;  
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

//线程栈，switch_to时保存线程环境
struct thread_stack {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;
    //线程第一次执行时保存待运行的函数的地址，switch_to 函数实现任务切换用于保存任务切换后的新任务的返回地址。
    void (*eip) (thread_func* func, void* func_arg);
    /*****线程第一次被调度上cpu时使用*****/
    void (*unused_retadddr);//充当返回地址占位用
    thread_func* function;
    void* func_arg;
};

//程序控制块PCB
struct task_struct {
   uint32_t* self_kstack; // 内核线程自己的内核栈栈顶指针
   pid_t pid;
   enum task_status status;
   char name[16];
   uint8_t priority;
   
   uint8_t ticks; // 嘀嗒数
   uint32_t elapsed_ticks; // 已经占用了的cpu嘀嗒数

   int32_t fdtable[MAX_FILES_OPEN_PER_PROC]; // 文件描述符数组

   struct list_elem general_tag;  // 一般队列中的节点
   struct list_elem all_list_tag; // 线程队列中的节点

   uint32_t* pgdir; // 进程页表的虚拟地址，线程为NULL
   struct virtual_addr userprog_vaddr; // 用户进程的虚拟地址池

   struct mem_block_desc u_block_desc[DESC_CNT]; // 用户内存块描述符数组

   uint32_t cwd_inode_nr; // 进程所在工作目录的i结点编号

   uint32_t stack_magic;  // 栈的边界标记，用于检测栈溢出
};

extern struct list thread_ready_list;
extern struct list thread_all_list;

void thread_create(struct task_struct* pthread, thread_func function, void* func_args);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_args);
struct task_struct* running_thread(void);
void schedule(void);
void thread_init(void);

void thread_block(enum task_status stat);
void thread_unblock(struct task_struct *pthread);

void thread_yield(void);

#endif

