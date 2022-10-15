

#include <kernel/thread.h>
#include <lib/kernel/stdint.h>
#include <kernel/string.h>
#include <kernel/memory.h>
#include <kernel/list.h>
#include <kernel/interrupt.h>


#define PG_SIZE 4096

struct task_struct* main_thread;//主线程的pcb
struct list thread_ready_list;//就绪队列
struct list thread_all_list;
static struct list_elem* thread_tag;//记录tag,用于将结点转换到pcb

extern void switch_to(struct task_struct* cur, struct tasK_struct* next);


// 获取当前线程的pcb
struct task_struct* running_thread() {
    uint32_t esp;
    asm ("mov %%esp, %0" : "=g" (esp)); //获取当前的栈指针，各线程 0 特权级栈都是在自己的 PCB 当中
    return (struct task_struct*)(esp & 0xfffff000);
}
 
// 由kernel_thread去执行function(func_arg)
static void kernel_thread(thread_func* function, void* func_arg) {
    //开中断,避免屏蔽了时钟中断，后面的进程无法被调度
    intr_enable();
    function(func_arg);
}

//初始化线程栈thread_stack
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {
    pthread->self_kstack -= sizeof(struct intr_stack);//预留中断栈的空间
    pthread->self_kstack -= sizeof(struct thread_stack);//预留线程栈的空间

    struct thread_stack* kthread_stack = (struct thread_stack*) pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;

    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

//初始化线程信息
void init_thread(struct task_struct* pthread, char* name, int prio) {
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);
    
    if (pthread == main_thread) pthread->status = TASK_RUNNING;
    else pthread->status = TASK_READY;

    // pthread->status = TASK_RUNNING;

    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;
    //self_kstack 是线程自己在内核态下使用的栈顶地址
    pthread->stack_magic = 0x20000509;

}

//创建线程，线程执行函数function(func_arg)
struct task_struct* thread_start(char* name, int prio, thread_func function, void* fun_arg) {
    //PCB位于内核空间
    // put_str(name);
    struct task_struct* thread = get_kernel_pages(1);
    
    init_thread(thread, name, prio);   
    thread_create(thread, function, fun_arg);
    //将线程加入队列中
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);
    
    return thread;
}

//将 main 线程完善为主线程
static void make_main_thread(void) {
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);

    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

//实现任务调度
void schedule(void) {
    ASSERT(intr_get_status() == INTR_OFF);

    struct task_struct* cur = running_thread();

    if (cur->status == TASK_RUNNING) {//时间片用完加入就绪队列
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    } else {
        //todo:线程被阻塞了
    }

    //将就绪队列的第一个线程弹出上处理器
    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    //利用tag获取pcb的地址
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;
    switch_to(cur, next);

}

//初始化线程环境
void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);

    make_main_thread();

    put_str("thrad_init done\n");
}

