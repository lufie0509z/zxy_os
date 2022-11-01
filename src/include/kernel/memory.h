#ifndef __KERNEL_MEMORY_H 
#define __KERNEL_MEMORY_H 
#include <lib/kernel/stdint.h>
#include <lib/kernel/bitmap.h>
#include <kernel/list.h>

// 存在标志
# define PG_P_1 1
# define PG_P_0 0
// 只读
# define PG_RW_R 0
// 可写
# define PG_RW_W 2
// 系统级
# define PG_US_S 0
# define PG_US_U 4

/**
 * 内存池类型标志.
 */ 
enum pool_flags {
    // 内核类型
    PF_KERNEL = 1,
    PF_USER = 2
};

struct virtual_addr {
    struct bitmap vaddr_bitmap;
    // 虚拟内存的起始地址
    uint32_t vaddr_start;
};

extern struct pool kernel_pool, user_pool;


// 内存块
struct mem_block {
    struct list_elem free_elem;
};

//内存块描述符
struct mem_block_desc {
    uint32_t block_size;         // 内存块大小
    uint32_t block_per_arena;    //一个arena能够提供的内存块个数
    struct list free_list;  //空闲 mem_block 链表，可以由多个 arena 提供内存块
};

#define DESC_CNT 7               //内存块描述符个数

void mem_init(void);
void* get_kernel_pages(uint32_t page_count);
void* get_user_pages(uint32_t page_count);
void* malloc_page(enum pool_flags pf, uint32_t page_count);

void* get_a_page(enum pool_flags pf, uint32_t vaddr);

uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);

uint32_t addr_v2p(uint32_t vaddr);

void block_desc_init(struct mem_block_desc* desc_array);

void* sys_malloc(uint32_t size);

# endif

