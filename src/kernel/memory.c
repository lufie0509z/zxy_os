#include <kernel/memory.h>
#include <lib/kernel/stdint.h>
#include <lib/kernel/print.h>
#include <kernel/debug.h>
#include <kernel/string.h>
#include <kernel/sync.h>
#include <device/console.h>
#include <kernel/interrupt.h>

# define PG_SIZE 4096

// 位图地址
# define MEM_BITMAP_BASE 0xc009a000

// 内核使用的起始虚拟地址
// 跳过低端1MB内存，中间10为代表页表项偏移，即0x100，即256 * 4KB = 1MB
# define K_HEAP_START 0xc0100000

// 获取高10位页目录项标记
# define PDE_INDEX(addr) ((addr & 0xffc00000) >> 22)
// 获取中间10位页表标记 
# define PTE_INDEX(addr) ((addr & 0x003ff000) >> 12) 


struct pool {
    struct bitmap pool_bitmap;
    uint32_t phy_addr_start;
    uint32_t pool_size;

    struct lock lock; //申请内存的时候需要互斥
};

struct pool kernel_pool, user_pool;
struct virtual_addr kernel_vaddr;


// 内存仓库 arena
struct arena {
    struct mem_block_desc* desc;  //内存块描述符

    uint32_t cnt; // large 为 true, 表示页框数，否则表示空闲的内存块数量
    bool large;
};

struct mem_block_desc k_block_descs[DESC_CNT]; //内存块描述符数组


/**
 * 初始化内存池.
 */ 
static void mem_pool_init(uint32_t all_memory) {
    put_str("Start init Memory pool...\n");

    // 页表(一级和二级)占用的内存大小，256的由来:
    // 一页的页目录，页目录的第0和第768项指向一个页表，此页表分配了低端1MB内存(其实此页表中也只是使用了256个表项)，
    // 剩余的254个页目录项实际没有分配对应的真实页表，但是需要为内核预留分配的空间
    uint32_t page_table_size = PG_SIZE * 256;

    // 已经使用的内存为: 低端1MB内存 + 现有的页表和页目录占据的空间
    uint32_t used_mem = (page_table_size + 0x100000);

    uint32_t free_mem = (all_memory - used_mem);
    uint16_t free_pages = free_mem / PG_SIZE;

    uint16_t kernel_free_pages = (free_pages  / 2);
    uint16_t user_free_pages = (free_pages - kernel_free_pages);

    // 内核空间bitmap长度(字节)，每一位代表一页
    uint32_t kernel_bitmap_length = kernel_free_pages / 8;
    uint32_t user_bitmap_length = user_free_pages / 8;

    // 内核内存池起始物理地址，注意内核的虚拟地址占据地址空间的顶端，但是实际映射的物理地址是在这里
    uint32_t kernel_pool_start = used_mem;
    uint32_t user_pool_start = (kernel_pool_start + kernel_free_pages * PG_SIZE);

    kernel_pool.phy_addr_start = kernel_pool_start;
    user_pool.phy_addr_start = user_pool_start;

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kernel_bitmap_length;
    user_pool.pool_bitmap.btmp_bytes_len = user_bitmap_length;

    // 内核bitmap和user bitmap bit数组的起始地址
    kernel_pool.pool_bitmap.bits = (void*) MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*) (MEM_BITMAP_BASE + kernel_bitmap_length);

    put_str("    Kernel pool bitmap address: ");
    put_int(kernel_pool.pool_bitmap.bits);
    put_str("; Kernel pool physical address: ");
    put_int(kernel_pool.phy_addr_start);
    put_char('\n');

    put_str("    User pool bitmap address: ");
    put_int(user_pool.pool_bitmap.bits);
    put_str("; User pool physical address: ");
    put_int(user_pool.phy_addr_start);
    put_char('\n');

    bitmap_init(&kernel_pool.pool_bitmap);
    bitmap_init(&user_pool.pool_bitmap);

    //初始化锁
    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kernel_bitmap_length;
    // 内核虚拟地址池仍然保存在低端内存以内
    kernel_vaddr.vaddr_bitmap.bits = (void*) (MEM_BITMAP_BASE + kernel_bitmap_length + user_bitmap_length);
    kernel_vaddr.vaddr_start = K_HEAP_START;

    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("Init memory pool done.\n");
}

static void printKernelPoolInfo(struct pool p) {
    put_str("Kernel pool bitmap address: ");
    put_int(p.pool_bitmap.bits);
    put_str("; Kernel pool physical address: ");
    put_int(p.phy_addr_start);
    put_char('\n');
}

static void printUserPoolInfo(struct pool p) {
    put_str("User pool bitmap address: ");
    put_int(p.pool_bitmap.bits);
    put_str("; User pool physical address: ");
    put_int(p.phy_addr_start);
    put_char('\n');
}

/**
 * 申请指定个数的虚拟页.返回虚拟页的起始地址，失败返回NULL.
 */ 
static void* vaddr_get(enum pool_flags pf, uint32_t pg_count) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t count = 0;

    if (pf == PF_KERNEL) {
        bit_idx_start = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_count);
        // put_str("var_bit_index");
        // put_int(bit_idx_start);
        // put_str("\n");
        if (bit_idx_start == -1) {
            // 申请失败，虚拟内存不足
            return NULL;
        }
        // 修改bitmap，占用虚拟内存
        while (count < pg_count) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, (bit_idx_start + count), 1);
            ++count;
        }
        vaddr_start = (kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE); 
    } else { // 用户内存池
        struct task_struct* cur = running_thread();
        bit_idx_start = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_count);
        if (bit_idx_start == -1) {
            return NULL;
        }

        while (count < pg_count)
        {
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, (bit_idx_start + count), 1);
            ++count;
        }
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;

        //(0xc0000000 - PG_SIZE)作为用户 3 级栈在 start_process 被分配
        ASSERT((uint32_t)vaddr_start < 0xc0000000 - PG_SIZE);
    }

    return (void*) vaddr_start;
}

/**
 * 得到虚拟地址对应的PTE的指针.
 */ 
uint32_t* pte_ptr(uint32_t vaddr) {
    return (uint32_t*) (0xffc00000 + ((vaddr & 0xffc00000) >> 10) + (PTE_INDEX(vaddr) << 2));
}

/**
 * 得到虚拟地址对应的PDE指针.
 */ 
uint32_t* pde_ptr(uint32_t vaddr) {
    return (uint32_t*) ((0xfffff000) + (PDE_INDEX(vaddr) << 2));
}

/**
 * 在给定的物理内存池中分配一个物理页，返回其物理地址.
 */ 
static void* palloc(struct pool* m_pool) {
    int bit_index = bitmap_scan(&m_pool->pool_bitmap, 1);
    if (bit_index == -1) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_index, 1);
    uint32_t page_phyaddr = ((bit_index * PG_SIZE) + m_pool->phy_addr_start);
    return (void*) page_phyaddr;
}

/**
 * 通过页表建立虚拟页与物理页的映射关系.
 */ 
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
    uint32_t vaddr = (uint32_t) _vaddr, page_phyaddr = (uint32_t) _page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr); 
    uint32_t* pte = pte_ptr(vaddr);
    
   
    if (*pde & 0x00000001) {
        // 页目录项已经存在
  
        if (!(*pte & 0x00000001)) {
            // 物理页必定不存在，使页表项指向我们新分配的物理页
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        } else {
            PANIC("pte repeat");
        }
    } else {
        // 新分配一个物理页作为页表
        uint32_t pde_phyaddr = (uint32_t) palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        // 清理物理页
        // ASSERT(3== 2);
        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);  
        // ASSERT(3== 2);
        ASSERT(!(*pte & 0x00000001));
        // ASSERT(3== 2);
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        // ASSERT(1 == 2);
    }
     if(vaddr == (0xc0000000 - 0x1000)){

    // ASSERT(1 == 2);
    }
}


/**
 * 分配page_count个页空间，自动建立虚拟页与物理页的映射.
 */ 
void* malloc_page(enum pool_flags pf, uint32_t page_count) {
    ASSERT(page_count > 0 && page_count < 3840);

    // 在虚拟地址池中申请虚拟内存
    void* vaddr_start = vaddr_get(pf, page_count);
  
    if (vaddr_start == NULL) {
        return NULL;
    }

    uint32_t vaddr = (uint32_t) vaddr_start, count = page_count;
    struct pool* mem_pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;

    // 物理页不必连续，逐个与虚拟页做映射
    while (count > 0) {
        void* page_phyaddr = palloc(mem_pool);
        if (page_phyaddr == NULL) {
            return NULL;
        }
        
        page_table_add((void*) vaddr, page_phyaddr);
        vaddr += PG_SIZE;
        --count;
    }

    return vaddr_start;
}

/**
 * 在内核内存池中申请page_count个页.
 */ 
void* get_kernel_pages(uint32_t page_count) {
    lock_acquire(&kernel_pool.lock);
    void* vaddr = malloc_page(PF_KERNEL, page_count);
    if (vaddr != NULL) {
        memset(vaddr, 0, page_count * PG_SIZE);
    }
    lock_release(&kernel_pool.lock);
    return vaddr;
}


/**
 * 在用户内存池申请页，并且返回分配的虚拟地址
 */
void* get_user_pages(uint32_t page_count) {
    lock_acquire(&user_pool.lock);//加锁
    void* vaddr = malloc_page(PF_USER, page_count);
    if (vaddr != NULL) {
        memset(vaddr, 0, page_count * PG_SIZE);
    }
    lock_release(&user_pool.lock);
    return vaddr;
}

/**
 * 申请一页内存，并用 vaddr 映射到该页，也就是说将虚拟地址 vaddr 与 pf 内存池中的物理地址关联
 */
void* get_a_page(enum pool_flags pf, uint32_t vaddr) {
    struct pool* mem_pool = (pf & PF_KERNEL) ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);

    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;
    
    
    //如果是用户进程申请用户内存，就修改用户自己的虚拟地址位图
    if (cur->pgdir != NULL && pf == PF_USER) {
        bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);
    } else if (cur->pgdir == NULL && pf == PF_KERNEL) {//内核线程申请内核内
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
    } else {
        PANIC("get_a_page:not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    }

    void* page_phyaddr = palloc(mem_pool); //在给定的物理内存池中分配一页物理地址
    if (page_phyaddr == NULL) return NULL;
    
    page_table_add((void*)vaddr, page_phyaddr);
//   ASSERT(1 == 2);
    lock_release(&mem_pool->lock);

    return (void*) vaddr;
}

/**
 * 返回该虚拟地址映射到的物理地址
 */
uint32_t addr_v2p(uint32_t vaddr) {

    uint32_t* pte = pte_ptr(vaddr);

    uint32_t phyaddr = ((*pte) & 0xfffff000) + (vaddr & 0x00000fff);
    return phyaddr;
}

//内存块描述符初始化
void block_desc_init(struct mem_block_desc* desc_array) {
    uint32_t index = 0, block_size = 16;

    for (index = 0; index < DESC_CNT; index++) {
        desc_array[index].block_size = block_size;
        desc_array[index].block_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;
        list_init(&desc_array[index].free_list);

        block_size *= 2;
    }

}

/**
 * 申请内存
 */
//返回 arena 中第 idx 个内存块的地址
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

//返回内存块 b 所在的 arena 的虚拟地址
static struct arena* block2arena(struct mem_block* b) {
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

//申请 size 字节的内存
void* sys_malloc(uint32_t size) {
    
    enum pool_flags PF;
    struct pool* mem_pool;
    uint32_t pool_size;
    struct mem_block_desc* descs;
    struct task_struct* cur_thread = running_thread();

    if (cur_thread->pgdir == NULL) { //内核线程
        PF = PF_KERNEL;
        // console_put_str("kernel");
        mem_pool = &kernel_pool;
        pool_size = kernel_pool.pool_size;
        descs = k_block_descs;
    } else { //用户进程
        PF = PF_USER;
        // console_put_str("user");
        mem_pool = &user_pool;
        pool_size = user_pool.pool_size;
        descs = cur_thread->u_block_desc;
    }

    if (!(size > 0 && size < pool_size)) return NULL;

    struct arena* a;
    struct mem_block* b;

    lock_acquire(&mem_pool->lock);

    if (size > 1024) { //申请的内存超过1024，将整页分配出去
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE); //需要的页框数
        a = malloc_page(PF, page_cnt);
        
        if (a != NULL) {
            memset(a, 0, page_cnt * PG_SIZE);

            // 内存块描述符表为空，large 为 true，cnt 表示需要的页框数
            a->desc = NULL;
            a->cnt = page_cnt;
            a->large = true;

            lock_release(&mem_pool->lock);
            return (void*)(a + 1); //跨过元信息部分，返回 arena 中的内存池起始地址
        } else {
            lock_release(&mem_pool->lock);
            return NULL;
        }
    } else { 
        //找到适合的内存块大小
        uint32_t desc_idx;
        for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
            if (size <= descs[desc_idx].block_size) break;
        }

        // 如果没有可用的内存块，就创建新的 arena 并为它分配内存块
        if (list_empty(&descs[desc_idx].free_list)) {
            a = malloc_page(PF, 1);
            if (a == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
            }

            memset(a, 0, PG_SIZE);
            a->desc = &descs[desc_idx];
            a->large = false;
            a->cnt = descs[desc_idx].block_per_arena;

            //将 arena 拆分为内存块，加入空闲内存块链表中
            uint32_t block_idx;
            enum intr_status old_status = intr_disable();
            for (block_idx = 0; block_idx < descs[desc_idx].block_per_arena; block_idx++) {
                b = arena2block(a, block_idx);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }

            intr_set_status(old_status);
        }

        //分配内存块
        b = elem2entry(struct mem_block, free_elem, list_pop(&descs[desc_idx].free_list)); //获取内存块地址
        
        memset(b, 0, descs[desc_idx].block_size);
        a = block2arena(b); //获取内存块 b 所在的 arena 地址
        a->cnt--;
        
        lock_release(&mem_pool->lock);
        // console_put_int((uint32_t)b);
        return (void*)b;
    }


}

/**
 * 释放内存
 */
//在物理地址池中释放物理页地址
void pfree(uint32_t pg_phy_addr) {
    struct pool* mem_pool;
    uint32_t bm_idx = 0;
    if (pg_phy_addr > user_pool.phy_addr_start) {  //用户的物理内存池
        mem_pool = &user_pool;
        bm_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    } else {                                       //内核的物理内存池
        mem_pool = &kernel_pool;
        bm_idx = (pg_phy_addr- kernel_pool.phy_addr_start) / PG_SIZE;
    }

    bitmap_set(&mem_pool->pool_bitmap, bm_idx, 0);
}

// 在页表中去掉虚拟地址的映射
static void page_table_pte_remove(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);

    *pte &=  ~PG_P_1;  //将虚拟地址对应 pte 的 P 位置 0
    asm volatile ("invlpg %0": : "m" (vaddr): "memory"); //刷新快表 tlb
}

//在虚拟地址池中释放虚拟地址，释放以_vaddr 起始的连续 pg_cnt 个虚拟页地址
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t bm_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;

    if (pf == PF_KERNEL) {
        bm_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        while (cnt < pg_cnt)  bitmap_set(&kernel_vaddr.vaddr_bitmap, bm_idx_start + cnt++, 0);
    } else {
        struct task_struct* cur_thread = running_thread();
        bm_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
        while (cnt < pg_cnt) bitmap_set(&cur_thread->userprog_vaddr, bm_idx_start + cnt++, 0);
    }
}

// 释放以虚拟地址 vaddr 为起始的 cnt 个物理页框
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {

    uint32_t vaddr = (uint32_t)_vaddr, cnt = 0;
    ASSERT(pg_cnt >=1 && vaddr % PG_SIZE == 0); 

    uint32_t pg_phy_addr = addr_v2p(vaddr);
    ASSERT((pg_phy_addr % PG_SIZE == 0) && pg_phy_addr > 0x102000); //释放的物理内存不在低端 1M 和 1K 的页表空间

    if (pg_phy_addr >= user_pool.phy_addr_start) { //用户的物理内存地址池内
        ASSERT(pf == PF_USER);
        vaddr -= PG_SIZE;
        while (cnt < pg_cnt) {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            //该物理地址属于用户的物理内存地址池内
            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);

            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);
            cnt++;
        }
        vaddr_remove(pf, _vaddr, pg_cnt);
    } else { //内核的物理内存地址池内
        ASSERT(pf == PF_KERNEL);
        vaddr -= PG_SIZE;
        while (cnt < pg_cnt) {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);

            //该物理地址属于内核的物理内存地址池内
            ASSERT((pg_phy_addr % PG_SIZE) == 0 &&     \
            pg_phy_addr < user_pool.phy_addr_start &&  \
            pg_phy_addr >= kernel_pool.phy_addr_start);

            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);
            cnt++;
        }
        vaddr_remove(pf, _vaddr, pg_cnt);
    }
}

// 释放或回收 ptr 所指向的内存
void sys_free(void* ptr) {
    ASSERT(ptr != NULL);

    enum pool_flags pf;
    struct pool* mem_pool;

    if(running_thread()->pgdir == NULL) {
        pf = PF_KERNEL;
        mem_pool = &kernel_pool;
    } else {
        pf = PF_USER;
        mem_pool = &user_pool;
    }

    lock_acquire(&mem_pool->lock);

    struct mem_block* b = ptr;
    struct arena* a = block2arena(b);
    ASSERT(a->large == 0 || a->large == 1);

    if (a->large == 1 && a->desc == NULL) { //释放大于 1024 字节的大内存
        mfree_page(pf, a, a->cnt);
    } else {
        list_append(&a->desc->free_list, &b->free_elem); //回收小内存块

        if (++a->cnt == a->desc->block_per_arena) { //如果该 arena 已经没人使用就释放
            for (uint32_t block_idx = 0; block_idx < a->desc->block_per_arena; block_idx++) {
                struct mem_block* b = arena2block(a, block_idx);
                ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                list_remove(&b->free_elem);
            }

            mfree_page(pf, a, 1);
        }
    }

    lock_release(&mem_pool->lock);
}


// 安装一页大小的vaddr，但是不需要在虚拟地址内存池中设置位图
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr) {
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);
    void* page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) {
        lock_release(&mem_pool->lock);
        return NULL;
    }
    page_table_add((void*)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

void mem_init(void) {
    put_str("Init memory start.\n");
    uint32_t total_memory = (*(uint32_t*) (0xb00));
    mem_pool_init(total_memory);
    block_desc_init(k_block_descs);
    put_str("Init memory done.\n");
}

