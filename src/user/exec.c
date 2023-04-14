#include <fs/fs.h>
#include <lib/stdio.h>
#include <user/exec.h>
#include <user/assert.h>
#include <user/process.h>
#include <kernel/global.h>
#include <kernel/memory.h>
#include <kernel/string.h>
#include <kernel/thread.h>
#include <lib/kernel/stdint.h>

typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

extern void intr_exit();

// ELF 头部（ ELF Header）给出文件的组织情况
struct Elf32_Ehdr {
   unsigned char e_ident[16];
   Elf32_Half    e_type;
   Elf32_Half    e_machine;
   Elf32_Word    e_version;
   Elf32_Addr    e_entry;
   Elf32_Off     e_phoff;
   Elf32_Off     e_shoff;
   Elf32_Word    e_flags;
   Elf32_Half    e_ehsize;
   Elf32_Half    e_phentsize;
   Elf32_Half    e_phnum;
   Elf32_Half    e_shentsize;
   Elf32_Half    e_shnum;
   Elf32_Half    e_shstrndx;
};

// 程序头部表，描述了一个段或者其它系统在准备程序执行时所需要的信息
struct Elf32_Phdr {
   Elf32_Word p_type;		 // 段的类型
   Elf32_Off  p_offset;
   Elf32_Addr p_vaddr;
   Elf32_Addr p_paddr;
   Elf32_Word p_filesz;
   Elf32_Word p_memsz;
   Elf32_Word p_flags;
   Elf32_Word p_align;
};

// 段类型 
enum segment_type {
   PT_NULL,            // 忽略
   PT_LOAD,            // 可加载程序段
   PT_DYNAMIC,         // 动态加载信息 
   PT_INTERP,          // 动态加载器名称
   PT_NOTE,            // 一些辅助信息
   PT_SHLIB,           // 保留
   PT_PHDR             // 程序头表
};


// 将文件描述符 fd 指向的文件中，便宜为 offset，大小为 filesz 的段加载到虚拟地址为 vaddr 的位置
static bool segment_load (int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr) {
    
    uint32_t vaddr_first_page = vaddr & 0xfffff000;
    uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);

    uint32_t occupied_pages = 0; //该段占有的页框数
    if (filesz > size_in_first_page) {
        uint32_t left_size = filesz - size_in_first_page;
        occupied_pages = DIV_ROUND_UP(left_size, PG_SIZE) + 1;
    } else occupied_pages = 1;

    // printf("vaddr%d filesz %d occupied_pages%d\n", vaddr, filesz, occupied_pages);
    // 为新进程分配内存
    uint32_t page_idx = 0;
    uint32_t vaddr_page = vaddr_first_page;
    while (page_idx < occupied_pages) {
        uint32_t* pde = pde_ptr(vaddr_page);
        uint32_t* pte = pte_ptr(vaddr_page);
    
        /* 如果 pde 或者 pte 不存在就分配一物理页框
         * 需要先判断 pde 再判断 pte
         * pde 不存在时会导致判断 pte 是缺页异常 */
        if (!(*pde & 0x00000001) || !(*pte & 0x00000001)) {
            
            if (get_a_page(PF_USER, vaddr_page) == NULL) {
                return false;
            } 
        } 

        vaddr_page += PG_SIZE;
        page_idx++;
    }
    
    // 将文件指针指向 offset 处

    sys_lseek(fd, offset, SEEK_SET);
    // printf("vaddr%d filesz %d fd%d\n", vaddr, filesz, fd);
    sys_read(fd, (void*)vaddr, filesz);
    return true;
}

// 从文件系统中加载可执行程序，成功返回程序的起始地址，否则返回 -1
static int32_t load(const char* pathname) {
    
    int32_t ret = -1;

    struct Elf32_Ehdr elf_header;
    struct Elf32_Phdr prog_header;
    memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));

    int32_t fd = sys_open(pathname, O_RDONLY);
    if (fd == -1) return -1;

    if (sys_read(fd, &elf_header, sizeof(struct Elf32_Ehdr)) \
                        != sizeof(struct Elf32_Ehdr)) {
        ret = -1;
        goto done;
    }
    
    // 校验 elf 头
    if (memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) \
        || elf_header.e_type    != 2 \
        || elf_header.e_machine != 3 \
        || elf_header.e_version != 1 \
        || elf_header.e_phnum    > 1024 \
        || elf_header.e_phentsize != sizeof(struct Elf32_Phdr)) {
        ret = -1;
        goto done;
    }

    Elf32_Off  ph_offset = elf_header.e_phoff;     // 程序头部表的偏移
    Elf32_Half ph_size   = elf_header.e_phentsize; // 程序头部表的总大小

    // 遍历程序头部表（结构体数组），遇到可加载的段就读入内存
    uint32_t seg_idx = 0;
    while (seg_idx < elf_header.e_phnum) {
        memset(&prog_header, 0, ph_size);

        sys_lseek(fd, ph_offset, SEEK_SET); // 将文件指针指向段相关的结构体位置
        if (sys_read(fd, &prog_header, ph_size) != ph_size) {
            ret = -1;
            goto done;
        }

        // 如果是可加载的，就加载到内存中
        if (prog_header.p_type == PT_LOAD) {
            // printf("prog_header.p_vaddr: %d\n", prog_header.p_vaddr);
            if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)) {
                ret = -1;
                goto done;
            }
          
        }

        ph_offset += ph_size; // 更新为下一个程序头的偏移
        seg_idx++;
    }
 
    ret = elf_header.e_entry; // 返回值是程序的入口
    
done:
    sys_close(fd);
    return ret;

}

// 用可执行程序 pathname 的进程体替换正在运行的用户进程进程体，失败返回 -1，成功则执行新进程
int32_t sys_execv(const char* pathname, const char* argv[]) {
    uint32_t argc = 0;
    while (argv[argc]) argc++;
    
    int32_t entry_point = load(pathname);
    if (entry_point == -1) return -1;

    struct task_struct* cur = running_thread();

    // 将正在执行的进程名替换为文件名
    memcpy(cur->name, pathname, TASK_NAME_LEN);
    cur->name[TASK_NAME_LEN - 1] = 0;

    // 获取内核栈的地址
    struct intr_stack* intr_0_stack = \
        (struct intr_stack*)((uint32_t)cur + PG_SIZE - sizeof(struct intr_stack));

    // 修改栈中的数据为新进程的
    intr_0_stack->ebx = (int32_t)argv;
    intr_0_stack->ecx = argc;
    intr_0_stack->eip = (void*)entry_point;

    /* 旧进程用户栈中的数据对新进程没用
     * 新进程用户栈地址从新开始为最高用户地址空间
     * 为后续穿参做准备，用户空间的最高处用于存储命令行参数 */
    intr_0_stack->esp = (void*)0xc0000000;

    /* 将新进程内核栈地址赋值给 esp 寄存器
     * 然后跳转到 intr_exit，假装从中断返回，实现了新进程的运行 */
    asm volatile("movl %0, %%esp; jmp intr_exit" : : "g" (intr_0_stack) : "memory");
   
    return 0;

}
