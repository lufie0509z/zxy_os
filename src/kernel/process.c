#include <kernel/thread.h>
#include <kernel/global.h>
#include <kernel/memory.h>
#include <kernel/process.h>
#include <kernel/tss.h>
#include <device/console.h>>
#include <kernel/string.h>
#include <kernel/list.h>
#include <kernel/interrupt.h>
#include <lib/kernel/print.h>
#include <kernel/debug.h>

extern void intr_exit();

/* 构建用户进程初始上下文信息 */
void start_process(void* filename_) {
   void* function = filename_;
   struct task_struct* cur = running_thread();
   cur->self_kstack += sizeof(struct thread_stack);
   struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;	 
   proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
   proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
   proc_stack->gs = 0;		 // 用户态用不上,直接初始为0
   proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
   proc_stack->eip = function;	 // 待执行的用户程序地址
   proc_stack->cs = SELECTOR_U_CODE;
   proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
   proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE) ;
   proc_stack->ss = SELECTOR_U_DATA; 
   asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
}


// void start_process(void* filename_) {
//     void* func = filename_; //还没有实现文件系统，暂时用普通函数代替用户程序

//     struct task_struct* cur = running_thread();
//     cur->self_kstack += sizeof(struct thread_stack);  //现在指向中断栈的地址最低处

//     struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;
//     proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0; //对栈中的寄存器初始化
//     proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
//     proc_stack->gs = 0; //显存段段寄存器，操作系统不允许用户直接操作显存
//     proc_stack->fs = proc_stack->es = proc_stack->ds = SELECTOR_U_DATA; //DPL 为 3 的数据段
//     proc_stack->eip = func; //待执行的用户程序地址
//     proc_stack->cs = SELECTOR_U_CODE; //用户级代码段
//     proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
//     proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE); 
//     proc_stack->ss = SELECTOR_U_DATA;

//     /**
//      * 通过 jmp intr_exit 使程序流程跳转到中断出口地址
//      * 通过 jmp intr_exit 使程序流程跳转到中断出口地址
//      * 通过一系列 pop 指令和 iretd 指令，将 proc_stack 中的数据载入 CPU 的寄存器
//      * 从而使程序“假装”退出中断，进入特权级 3
//      */
//     asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
    
// }

//激活页表
void page_dir_activate(struct task_struct* pthread) {
    uint32_t page_phy_addr = 0x100000; //内核线程的页表物理地址
    
    if (pthread->pgdir != NULL) {//用户进程
        console_put_int(pthread->pgdir);
        // console_put_char('\n');
        page_phy_addr = addr_v2p((uint32_t)pthread->pgdir);
        put_int(page_phy_addr);
        // put_char("\n");
        // ASSERT(1 == 2);
    }
    // 更新页目录寄存器cr3
    console_put_str("  page_phy_addr");
    console_put_int(page_phy_addr);
    console_put_char('\n');
    

    asm volatile ("movl %0, %%cr3" : : "r" (page_phy_addr) : "memory");
  
    // if (page_phy_addr != 0x100000) ASSERT( 1== 2);
}

void process_activate(struct task_struct* pthread) {
    ASSERT(pthread != NULL);
    console_put_str("  process_activate");
    console_put_int((uint32_t)pthread->pgdir);
    console_put_str(pthread->name);
   
    page_dir_activate(pthread);
   
    if(pthread->pgdir != NULL) { 
        //如果是用户进程，就更新 tss.ss0 
        // ASSERT(1 == 2);
        update_tss_esp(pthread);
        // ASSERT(1 == 2);
    }
}


// //为用户进程创建页目录表
// uint32_t* create_page_dir() {
//     uint32_t* page_dir_vaddr = get_kernel_pages(1); //在内核空间中获取一页作为用户进程的页表
//     console_put_char("\n");
//     console_put_str("page_dir_vaddr");
//     console_put_str((uint32_t)page_dir_vaddr);
//     if (page_dir_vaddr == NULL) {
//         console_put_str("create_page_dir: get_kernel_page failed!");
//         return NULL;
//     }
    
//     /**
//      * 将内核的页目录项(768-1023项)复制到用户进程使用的页目录表中，从而使用户进程共享操作系统
//      * 内核页目录表的第1023项指向页目录表本身
//      * 内核页目录表起始处的虚拟地址是 0xfffff000
//      */
//     memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300 * 4), (uint32_t*)(0xfffff000 + 0x300 * 4), 1024);
   
//     //使用户进程页目录表的最后一项指向页目录表本身的物理地址
//     uint32_t page_dir_phyaddr = addr_v2p(page_dir_vaddr);
//     console_put_int(page_dir_vaddr);
//     page_dir_vaddr[1023] = page_dir_phyaddr | PG_US_S | PG_RW_W | PG_P_1;

//     return page_dir_vaddr;
// }


uint32_t* create_page_dir(void) {
    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    console_put_str("     create_page_dir");
    console_put_int((uint32_t)page_dir_vaddr);
    if (page_dir_vaddr == NULL) {
        console_put_str("create_page_dir: get_kernel_page failed!\n");
        return NULL;
    }

    /*  page_dir_vaddr + 0x300*4 --------> 768th in kernel page dir table */
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4), \
           (uint32_t*)(0xfffff000 + 0x300*4), 1024);

    // page dir address store in the last entry of table.
    // update dir table address as new page dir physical address
    uint32_t new_page_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
    console_put_int(new_page_phy_addr);
    page_dir_vaddr[1023] = new_page_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
    return page_dir_vaddr;
}
// // 为用户进程创建虚拟内存池
// void create_user_vaddr_bitmap(struct task_struct* user_prog) {
//     user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    
//     uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);  //记录位图需要的内存页框数
//     user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt); //为位图分配内存，返回虚拟地址
//     user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8; //位图长度
//     bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
// }

// create user proces va bitmap
void create_user_vaddr_bitmap(struct task_struct* user_prog) {
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP( ( 0xc0000000 - USER_VADDR_START ) / PG_SIZE / 8, PG_SIZE );
    console_put_str("create_user_vaddr_bitmap: bitmap_pg_cnt = ");
    console_put_int(bitmap_pg_cnt);
    console_put_char('\n');
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = ( 0xc0000000 - USER_VADDR_START ) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}


//创建进程， filename 是用户进程地址，name 是进程名
void process_execute(void* filename, char* name) {
    
    struct task_struct* thread = get_kernel_pages(1);
   
    init_thread(thread, name, default_prio);
    create_user_vaddr_bitmap(thread);

    thread_create(thread, start_process, filename);
    thread->pgdir = create_page_dir();

    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));

    list_append(&thread_all_list, &thread->all_list_tag);
    list_append(&thread_ready_list, &thread->general_tag);

    intr_set_status(old_status);
}
