

#include <lib/stdio.h>
#include <lib/kernel/stdint.h>
#include <lib/kernel/bitmap.h>
#include <kernel/list.h>
#include <kernel/sync.h>
#include <kernel/debug.h>
#include <kernel/string.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/interrupt.h>
#include <user/process.h>
#include <device/console.h>
#include <fs/fs.h>
#include <fs/file.h>

// pid的位图，最多支持 128*8
uint8_t pid_bitmap_bits[128] = {0};

// pid池
struct pid_pool {
    struct bitmap pid_bitmap;
    uint32_t pid_start;   // 起始 pid
    struct lock pid_lock; // pid 锁
}pid_pool;

struct task_struct* main_thread;      // 主线程的pcb
struct list thread_ready_list;        // 就绪队列
struct list thread_all_list;
static struct list_elem* thread_tag;  // 记录tag,用于将结点转换到pcb

struct task_struct* idle_thread;      // idle线程

extern void switch_to(struct task_struct* cur, struct tasK_struct* next);

// struct lock pid_lock;  //pid 是唯一的，分配 pid 时必须互斥

extern void init();

static void idle(void* arg UNUSED) {
    while (1) {
        thread_block(TASK_BLOCKED);
        // 开中断，hlt用于让处理器停止执行指令，将处理器挂起
        // 外部中断发生可唤醒处理器
        asm volatile ("sti; hlt" : : : "memory");
    }
}

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

// 初始化 pid池
static void pid_pool_init() {
    pid_pool.pid_start = 1;
    pid_pool.pid_bitmap.bits = pid_bitmap_bits;
    pid_pool.pid_bitmap.btmp_bytes_len = 128;
    bitmap_init(&pid_pool.pid_bitmap);
    lock_init(&pid_pool.pid_lock);
}

static pid_t allocate_pid() {
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
    lock_release(&pid_pool.pid_lock);
    return (bit_idx + pid_pool.pid_start);
}

void release_pid(pid_t pid) {
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = pid - pid_pool.pid_start;
    bitmap_set(&pid_pool, bit_idx, 0);
    lock_release(&pid_pool.pid_lock);
}

// 分配pid，因为allocate_pid是静态的,别的文件无法调用，这里封装一下。*/
pid_t fork_pid() {
    return allocate_pid();
}

//初始化线程栈thread_stack（中断退出时从该栈中获取上下文环境）
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

    pthread->pid = allocate_pid();

    strcpy(pthread->name, name);
    
    if (pthread == main_thread) pthread->status = TASK_RUNNING;
    else pthread->status = TASK_READY;

    // pthread->status = TASK_RUNNING;

    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;

    // 初始化文件描述符数组
    pthread->fdtable[0] = 0;  // 标准输入
    pthread->fdtable[1] = 1;  // 标准输出
    pthread->fdtable[2] = 2;  // 标准错误

    uint8_t fd_idx = 3;
    while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
        pthread->fdtable[fd_idx] = -1;
        fd_idx++;
    }

    pthread->cwd_inode_nr = 0; // 默认工作路径为根目录

    pthread->parent_pid = -1;  // 默认没有父进程

    //self_kstack 是线程自己在内核态下使用的栈顶地址
    pthread->stack_magic = 0x20000509;

    put_str("init_thread");
    put_int(pthread->pid); 
    if (pthread->parent_pid == -1) put_str("pthread->parent_pid == -1\n");
}

//创建线程，线程执行函数function(func_arg)
struct task_struct* thread_start(char* name, int prio, thread_func function, void* fun_arg) {
    //PCB位于内核空间
    // put_str(name);
    struct task_struct* thread = get_kernel_pages(1);
    
    init_thread(thread, name, prio); 
    // console_put_str(thread->name);
    // console_put_int((uint32_t)thread->pgdir);  
    ASSERT(thread->pgdir == NULL);
    thread_create(thread, function, fun_arg);
    // console_put_int((uint32_t)thread->pgdir);
    ASSERT(thread->pgdir == NULL);
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
    ASSERT(main_thread->pgdir == NULL);
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

    if (list_empty(&thread_ready_list)) { // 如果就绪队列为空，就唤醒idle_thread
        thread_unblock(idle_thread);
    }

    //将就绪队列的第一个线程弹出上处理器
    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    //利用tag获取pcb的地址
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;
    // console_put_str("cur");
    // console_put_str(cur->name);
    // if (cur->pgdir == NULL) console_put_str("none");
    // else console_put_int((uint32_t)next->pgdir);
    // console_put_str("next");
    // console_put_str(&next->name);
    // if (next->pgdir == NULL) console_put_str("none");
    // else console_put_int((uint32_t)next->pgdir);

    
    // 更新页表， 如果是用户进程就更新 tss.ss0
    process_activate(next);

    switch_to(cur, next);

}

//线程让步
void thread_yield(void) {
    struct task_struct* cur = running_thread();
    enum intr_status old_status = intr_disable(); 
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));

    list_append(&thread_ready_list, &cur->general_tag);
    cur->status = TASK_READY;

    schedule();

    intr_set_status(old_status);

}

//线程讲自己阻塞，状态修改为stas
void thread_block(enum task_status stat) {

    ASSERT(stat == TASK_BLOCKED || stat == TASK_HANGING || stat == TASK_WAITING);

    enum intr_status old_status = intr_disable();
    struct task_struct* cur_thread = running_thread();
    cur_thread->status = stat;

    schedule();


    //解除阻塞后才会继续执行
    intr_set_status(old_status);
}



void thread_unblock(struct task_struct* pthread) {
    enum intr_status old_status = intr_disable();

    ASSERT(pthread->status == TASK_BLOCKED || pthread->status == TASK_HANGING || pthread->status == TASK_WAITING);

    if (pthread->status != TASK_READY) {

        // ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
        if (elem_find(&thread_ready_list, &pthread->general_tag)) {
            PANIC("blocked thread in ready_list");
        }

        list_push(&thread_ready_list, &pthread->general_tag); //加入就绪队列的队头
        pthread->status = TASK_READY;
    }

    intr_set_status(old_status);

}

// 将ptr中的内容填充空格后输入到长度为buf_len的buf缓冲区中
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) {
    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0; // 用来存储ptr指向的数据的长度
    switch (format) {
        case 's': 
            out_pad_0idx = sprintf(buf, "%s", ptr);
            break;
        case 'd': // 16位整数
            out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));
        case 'x':
            out_pad_0idx = sprintf(buf, "%x", *((int32_t*)ptr));
    }
    while (out_pad_0idx < buf_len) { // 填充空格
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }
    sys_write(std_out, buf, buf_len - 1);
}

// list_traversal 函数中的回调函数，此处用于打印线程队列中的任务信息
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);

    char out_pad[16] = {0};
    pad_print(out_pad, 16, &pthread->pid, 'd');
    if (pthread->parent_pid == -1) pad_print(out_pad, 16, "NULL", 's');
    else pad_print(out_pad, 16, &pthread->parent_pid, 'd');

    // put_int(pthread->parent_pid);

    switch (pthread->status) {
        case 0:
            pad_print(out_pad, 16, "RUNNING", 's');
            break;
        case 1:
            pad_print(out_pad, 16, "READY", 's');
            break;
        case 2:
            pad_print(out_pad, 16, "BLOCKED", 's');
            break;
        case 3:
            pad_print(out_pad, 16, "WAITING", 's');
            break;
        case 4:
            pad_print(out_pad, 16, "HANGING", 's');
            break;
        case 5:
            pad_print(out_pad, 16, "DIED", 's');
    }
 
    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');
    memset(out_pad, 0, 16);
    ASSERT(strlen(pthread->name) < 17);
    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");
    // 在list_traversal中只有返回false才可以继续调用
    sys_write(std_out, out_pad, strlen(out_pad));

    return false;
    
}

void sys_ps() {
    char* ps_title = "PID            PPID           STAT           TICKS          COMMAND\n";
    sys_write(std_out, ps_title, strlen(ps_title));
    list_traversal(&thread_all_list, elem2thread_info, 0);
}

// 回收线程的 PCB 和页表，并将它从调度队列中去除
void thread_exit(struct task_struct* thread_over, bool need_schedule) {
    intr_disable();
    thread_over->status = TASK_DIED;

    // 如果不是当前线程，那么就有可能在就绪队列中，将其从中删除
    if (elem_find(&thread_ready_list, &thread_over->general_tag)) {
        list_remove(&thread_over->general_tag);
    }

    // 如果是内核进程就回收页表
    if (thread_over->pgdir) {
        mfree_page(PF_KERNEL, thread_over->pgdir, 1);
    }

    // 从所有线程的队列中删除
    list_remove(&thread_over->all_list_tag);

    // 回收 PCB，主线程的 PCB 不在堆中跳过
    if (thread_over != main_thread) {
        mfree_page(PF_KERNEL, thread_over, 1);
    }

    release_pid(thread_over->pid);

    if (need_schedule) {
        schedule();
        PANIC("thread_exit: should not be here\n");
    }
}

static bool check_pid(struct list_elem* pelem, pid_t pid) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    if (pthread->pid == pid) return true;
    return false;
}

// 根据 pid 找到线程的 PCB，若没有找到就返回 NULL
struct task_struct* pid_to_thread(int32_t pid) {
    struct list_elem* pelem = list_traversal(&thread_all_list, check_pid, pid);
    if (pelem == NULL) return NULL;
    
    struct task_struct* thread = elem2entry(struct task_struct, all_list_tag, pelem);
   
    return thread;
}
//初始化线程环境
void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);

    // lock_init(&pid_lock);
    pid_pool_init();

    // 创建第一个用户进程，其pid是1
    process_execute(init, "init");

    // thread_tag = NULL;

    make_main_thread();

    // 创建idle线程
    idle_thread = thread_start("idle", 10, idle, NULL);
    put_str("thread_init done\n");
}

