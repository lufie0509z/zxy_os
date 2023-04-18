#include <user/wait_exit.h>
#include <kernel/list.h>
#include <kernel/global.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/debug.h>
#include <lib/stdio.h>
#include <lib/kernel/stdint.h>
#include <fs/fs.h>

/* 回收用户进程的资源：
 * 页表中对应的物理页
 * 虚拟内存池占用的物理页框
 * 打开的文件 */
static void release_prog_resource(struct task_struct* release_thread) {
    uint32_t* pgdir_vaddr = release_thread->pgdir;
    // 遍历页表，如果页表的 p 位为 1，说明已经分配了物理页框
    uint16_t user_pde_nr = 768, pde_idx = 0;
    uint32_t pde = 0;
    uint32_t* v_pde_ptr = NULL;

    uint16_t user_pte_nr = 1024, pte_idx = 0;
    uint32_t pte = 0;
    uint32_t* v_pte_ptr = NULL;

    uint32_t* first_pte_vaddr_in_pde = NULL; // pde 中第一个pte指向的物理页起始地址
    uint32_t pg_phy_addr = 0;

    while (pde_idx < user_pde_nr) {
        v_pde_ptr = pgdir_vaddr + pde_idx;
        pde = *v_pde_ptr;
        // 如果页目录项的 p 位为 1，就说明可能有页表项
        if (pde & 0x00000001) { 
            // 获取第 pde_idx 个页表中第 0 个 pte 的虚拟地址
            first_pte_vaddr_in_pde = pte_ptr(pde_idx * 0x400000);
            pte_idx = 0;
            while (pte_idx < user_pte_nr) {
                v_pte_ptr = first_pte_vaddr_in_pde + pte_idx;
                pte = *v_pte_ptr;
                if (pte & 0x00000001) {
                    pg_phy_addr = pte & 0xfffff00;
                    // 将 pte 中记录的物理页框对应的内存池中的位清 0   
                    free_a_phy_page(pg_phy_addr);
                }
                pte_idx++;
            }
            pg_phy_addr = pde & 0xfffff000;
            free_a_phy_page(pg_phy_addr);
        }
        pde_idx++;
    }

    // 回收用户的虚拟地址池所占的物理内存
    uint32_t bitmap_pg_cnt = (release_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len) / PG_SIZE;
    uint8_t* user_vaddr_pool_bitmap = release_thread->userprog_vaddr.vaddr_bitmap.bits;
    mfree_page(PF_KERNEL, user_vaddr_pool_bitmap, bitmap_pg_cnt);

    // 关闭进程中打开的文件
    uint32_t fd_idx = 3;
    while (fd_idx < MAX_FILES_OPEN_PER_PROC) {
        if (release_thread->fdtable[fd_idx] != -1) {
            sys_close(fd_idx);
        }
        fd_idx++;
    }
}

// 查找父亲进程为 ppid 的子进程，list_traversal 的回调函数
static bool fine_child(struct list_elem* pelem, int32_t ppid) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    if (pthread->parent_pid == ppid) return true;
    else return false;
}

// 查找状态为挂起 HANGING 的子进程，list_traversal 的回调函数
static bool find_hanging_child(struct list_elem* pelem, int32_t ppid) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    if (pthread->status == TASK_HANGING && pthread->parent_pid == ppid) return true;
    else return false;
}

// 将子进程过继给 init 进程，list_traversal 的回调函数
static bool init_adopt_a_child(struct list_elem* pelem, int32_t ppid) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    if (pthread->parent_pid == ppid) {
        pthread->parent_pid = 1;
    }
    return false; // 让 list_traversal 继续调用
}

// 阻塞父进程等待子进程结束调用 exit 将返回值存放在地址 status，成功返回子进程的 pid 失败返回 -1
pid_t sys_wait(int32_t* status) {
    struct task_struct* parent_thread = running_thread();

    while (1) {
        // 优先处理挂起的子进程
        struct list_elem* child_elem = list_traversal(&thread_all_list, \
                                       find_hanging_child, parent_thread->pid);

        if (child_elem != NULL) {
            struct task_struct* child_thread = \
                                elem2entry(struct task_struct, all_list_tag, child_elem);
            *status = child_thread->exit_status;

            pid_t child_pid = child_thread->pid;

            // 回收线程的 PCB 和页表，并将它从就绪队列和全部队列中去除
            thread_exit(child_thread, false); // 第二个参数为 false，说明调用结束后还要回来
            // PCB 已经回收了进程彻底消失

            return child_pid;
        }
        
        child_elem = list_traversal(&thread_all_list, fine_child, parent_thread->pid);
        
        if (child_elem == NULL) { // 没有子进程
            return -1;
        } else {
            thread_block(TASK_WAITING); // 阻塞父进程，直到子进程 exit 时唤醒
        }
    }
}

// 子进程结束自己时调用，status 是退出时的状态
void sys_exit(int32_t status) {
    struct task_struct* child_thread = running_thread();
    child_thread->exit_status = status;

    if (child_thread->parent_pid == -1) {
        PANIC("sys_exit: child_thread->parent_pid == -1\n");
    }
    
    // 将该进程的子进程过继给 init 进程
    list_traversal(&thread_all_list, init_adopt_a_child, child_thread->pid);

    // 回收除 PCB 外的所有资源
    release_prog_resource(child_thread);

    // 如果父进程在等待自己就唤醒父进程
    struct task_struct* parent_thread = pid_to_thread(child_thread->parent_pid);

    if (parent_thread->status == TASK_WAITING) {
        thread_unblock(parent_thread);
    }

    // 将自己挂起，等待父进程知道其已经退出并获取它的 status 后回收 PCB
    thread_block(TASK_HANGING);
}

