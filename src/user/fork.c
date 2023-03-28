#include <kernel/list.h>
#include <kernel/debug.h>
#include <kernel/global.h>
#include <kernel/thread.h>
#include <kernel/string.h>
#include <kernel/memory.h>
#include <kernel/interrupt.h>
#include <fs/file.h>
#include <user/process.h>
#include <lib/kernel/stdint.h>

extern void intr_exit();

// 将父进程的PCB 虚拟地址池位图拷贝给子进程
static int32_t copy_pcb_vaddrbitmap_stack(struct task_struct* child_thread, \
                                          struct task_struct* parent_thread) {
    // 将父进程的pcb 内核栈全部复制给子进程，下面再单独修改其中的部分属性值                                         
    memcpy(child_thread, parent_thread, PG_SIZE);
    child_thread->pid = fork_pid();
    child_thread->status = TASK_READY; // 将新进程加入就绪队列中，之后调度其上CPU
    child_thread->elapsed_ticks = 0;
    child_thread->ticks = child_thread->priority;
    child_thread->parent_pid = parent_thread->pid;

    child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
    child_thread->all_list_tag.prev = child_thread->all_list_tag.next = NULL;

    // 初始化进程自己的内存块描述符，没有的话子进程内存分配时会缺页异常
    block_desc_init(&child_thread->u_block_desc);

    // 将父进程的虚拟地址位图复制给子进程
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP  \
        ((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE); // 虚拟地址位图需要的页框数
    void* vaddr_bitmap = get_kernel_pages(bitmap_pg_cnt);
    if (vaddr_bitmap == NULL) return 0;

    /* 操作前，child_thread->userprog_vaddr.vaddr_bitmap.bits指向父进程虚拟地址的位图地址
     * 将父进程的虚拟地址位图内容复制给vaddr_bitmap
     * 操作完成后，child_thread->userprog_vaddr.vaddr_bitmap.bits指向自己的位图vaddr_bitmap */
    memcpy(vaddr_bitmap, parent_thread->userprog_vaddr.vaddr_bitmap.bits, bitmap_pg_cnt * PG_SIZE);
    child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_bitmap;

    ASSERT(strlen(child_thread->name) < 11); // 防止下面拼接后越界
    strcat(child_thread->name, "_fork");     // 子进程和父进程同名，这里仅作调试使用

    return 0;
}


// 将父进程的程序体（代码段数据段等）用户栈复制给子进程。其中页缓冲区buf_page必须是内核页
static void copy_block_stack3(struct task_struct* child_thread,  \
                              struct task_struct* parent_thread, void* buf_page) {
    uint8_t* vaddr_bitmap = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
    uint32_t btmp_bytes_len = parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
    uint32_t vaddr_start =parent_thread->userprog_vaddr.vaddr_start;

    uint32_t idx_byte = 0;
    uint32_t idx_bit = 0;
    uint32_t prog_vaddr = 0;

    // 将父进程用户空间中的数据复制到内核的页缓冲区，再复制到子进程中
    while (idx_byte < btmp_bytes_len) { // 逐字节查看位图
        if (vaddr_bitmap[idx_byte]) {   
            while (idx_bit < 8) {       // 逐位查看该字节
                if (BITMAP_MASK << idx_bit & vaddr_bitmap[idx_byte]) {

                    prog_vaddr = vaddr_start + (idx_byte * 8 + idx_bit) * PG_SIZE;

                    // 利用内核页中转，将父进程用户空间的数据复制到子进程的用户空间
                    memcpy(buf_page, (const void*)prog_vaddr, PG_SIZE);

                    // 激活子进程的页表，确保pte和pde安装在子进程的页目录表中
                    page_dir_activate(child_thread);

                    // 为子进程的虚拟地址分配一个物理页
                    get_a_page_without_opvaddrbitmap(PF_USER, prog_vaddr);

                    // 内核空间到子进程的用户空间
                    memcpy((void*)prog_vaddr, buf_page, PG_SIZE);

                    // 恢复父进程的页表
                    page_dir_activate(parent_thread);
                }
                idx_bit++;
            }
        }
        idx_byte++;
    }

}

// 修改函数返回值为0，为子进程构建线程栈thread_stack
static int32_t build_child_statck(struct task_struct* child_thread) {
    // intr_stack是中断入口程序中保存上下文的地方
    // 获取子进程0级栈的栈顶
    struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)child_thread + PG_SIZE - sizeof(struct intr_stack));
    intr_0_stack->eax = 0; // 子进程返回值是0，eax寄存器中是函数返回值

    // 为switch_to函数构建 thread_stack，它的栈底在intr_stack栈顶下面
    uint32_t* ret_addr_in_thread_stack = (uint32_t*)intr_0_stack - 1; // eip

    uint32_t* esi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 2; 
    uint32_t* edi_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 3; 
    uint32_t* ebx_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 4; 

    // ebp在thread_stack中的地址便是当时的esp(0级栈的栈顶)
    uint32_t* ebp_ptr_in_thread_stack = (uint32_t*)intr_0_stack - 5;  

    *ret_addr_in_thread_stack =(uint32_t)intr_exit; // 子进程被调度后可以从中断处返回

    // 将thread_stack的栈顶记录在pcb的self_kstack，将来switch_to调度时可以用它作栈顶
    child_thread->self_kstack = ebp_ptr_in_thread_stack; 	    

    return 0;
}

// 更新线程的i结点打开次数，除前3个标准文件描述符
static void update_inode_open_cnts(struct task_struct* thread) {
    int32_t local_fd = 3, global_fd = 0;
    while (local_fd < MAX_FILES_OPEN_PER_PROC) {
        global_fd = thread->fdtable[local_fd];
        ASSERT(global_fd < MAX_FILES_OPEN);
        if (global_fd != -1) file_table[global_fd].fd_inode->i_open_cnt++;
        local_fd++;
    }
}

// 将父进程的所有资源拷贝给子进程
static int32_t copy_process(struct task_struct* child_thread, \
                            struct task_struct* parent_thread) {

    void* buf_page = get_kernel_pages(1); // 申请了一页大小的内核空间作为内核缓冲区
    if (buf_page == NULL) return -1;                       

    // 父进程的PCB 虚拟地址位图 内核栈
    if (copy_pcb_vaddrbitmap_stack(child_thread, parent_thread) == -1) return -1;

    // 为子进程创建页表
    child_thread->pgdir = create_page_dir();
    if (child_thread->pgdir == NULL) return -1;

    // 父进程的程序体和用户栈
    copy_block_stack3(child_thread, parent_thread, buf_page);

    // 子进程的thread_stack 修改返回值
    build_child_statck(child_thread);

    // 相关文件i结点的打开次数
    update_inode_open_cnts(child_thread);

    mfree_page(PF_KERNEL, buf_page, 1);

    return 0;
}

// 克隆当前进程
pid_t sys_fork() {
    struct task_struct* parent_thread = running_thread();

    // 获取1页内核空间作为子进程的PCB
    struct task_struct* child_thread = get_kernel_pages(1); 
    if (child_thread == NULL) return -1;

    ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);

    copy_process(child_thread, parent_thread);

    // 加入就绪队列和所有线程队列
    ASSERT(!elem_find(&thread_ready_list, &child_thread->general_tag));
    list_append(&thread_ready_list, &child_thread->general_tag);
    ASSERT(!elem_find(&thread_all_list, &child_thread->all_list_tag));
    list_append(&thread_all_list, &child_thread->all_list_tag);

    return child_thread->pid;
}
