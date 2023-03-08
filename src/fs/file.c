#include <fs/fs.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/inode.h>
#include <fs/super_block.h>
#include <device/ide.h>
#include <kernel/global.h>
#include <kernel/thread.h>
#include <kernel/memory.h>
#include <kernel/string.h>
#include <lib/kernel/bitmap.h>
#include <lib/kernel/stdio-kernel.h>


struct file file_table[MAX_FILES_OPEN]; // 文件表，文件结构数组

// 从文件表中获取一个空闲位并返回他的下标，失败返回-1
int32_t get_free_slot_in_global() {
    uint32_t fd_idx = 3;
    while (fd_idx < MAX_FILES_OPEN) {
        if (file_table[fd_idx].fd_inode == NULL) break;
        fd_idx++;
    }
    if (fd_idx == MAX_FILES_OPEN) {
        printk("exceed max open files\n");
        return -1;
    }
    return fd_idx;
}

// 将第global_fd_idx个文件结构安装到当前内核线程/用户进程的pcb中，成功返回在文件描述符数组中的下标
int32_t pcb_fd_install(int32_t global_fd_idx) {
    struct task_struct* cur = running_thread();

    uint8_t pcb_fd_idx = 2;
    while (pcb_fd_idx < MAX_FILES_OPEN_PER_PROC) {
        if (cur->fdtable[pcb_fd_idx] == -1) {
            cur->fdtable[pcb_fd_idx] = global_fd_idx;
            break;
        }
        pcb_fd_idx++;
    }

    if (pcb_fd_idx == MAX_FILES_OPEN_PER_PROC) {
        printk("exceed max open files per proc\n");
        return -1;
    }
    return pcb_fd_idx;
}

// 在位图中分配一个i结点
int32_t inode_bitmap_alloc(struct partition* p) {
    int32_t bit_idx = bitmap_scan(&p->inode_map, 1); // 扫描位图

    if (bit_idx == -1) return -1;

    bitmap_set(&p->inode_map, bit_idx, 1);
    return bit_idx;
}
 
// 在数据块位图中分配一个扇区，返回扇区的地址
int32_t block_bitmap_alloc(struct partition* p) {
    int32_t bit_idx = bitmap_scan(&p->block_bitmap, 1);
    if (bit_idx == -1) return -1;
    bitmap_set(&p->block_bitmap, bit_idx, 1);
    return (p->sb->block_bitmap_lba + bit_idx); // 返回扇区地址
}

// 将内存位图中bit_idx所在的512字节信息同步到磁盘中
void bitmap_sync(struct partition* p, uint32_t bit_idx, uint8_t bitmap_type) {
    uint32_t off_sec = bit_idx / 4096;         // i结点所在扇区相对于位图的偏移，以扇区为单位
    uint32_t off_size = off_sec * BLOCK_SIZE;  // i结点所在扇区相对于位图的偏移，以字节为单位

    uint32_t sec_lab;       // 位图的扇区地址
    uint8_t* bitmap_off;    // 位图的字节偏移量

    switch (bitmap_type) {
        case INODE_BITMAP:
            sec_lab = p->sb->inode_bitmap_lba + off_sec;  // 扇区地址
            bitmap_off = p->inode_map.bits + off_size;    // 位图地址
            break;

        case BLOCK_BITMAP:
            sec_lab = p->sb->block_bitmap_lba + off_sec;
            bitmap_off = p->block_bitmap.bits + off_size;
            break;
    }

    // 将信息同步到磁盘中
    ide_write(p->my_disk, sec_lab, bitmap_off, 1);
}

int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag) {
    void* io_buf = sys_malloc(1024); // 后续操作的公共缓冲区
    if (io_buf == NULL) {
        printk("in file_creat: sys_malloc for io_buf failed\n");
        return -1;
    }

    uint8_t rollback_step = 0;        // 用于确认回滚时需要做的操作

    int32_t inode_no = inode_bitmap_alloc(cur_part); // 新创建的文件的i结点号
    if (inode_no == -1 ) {
        printk("in file_creat: allocate inode failed\n");
        return -1;
    } 

    
    /* 申请inode结点
     * 需要从堆中申请内存，不能作为局部变量（函数退出时会释放）
     * file_table数组中文件描述符的i结点指针需要指向他 */
    struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
    if (new_file_inode == NULL) {
        printk("in file_creat: allocate inode failed\n");
        rollback_step = 1;
        goto rollback;
    }
    inode_init(inode_no, new_file_inode);

    // 文件描述符相关
    int32_t fd_idx = get_free_slot_in_global();
    if (fd_idx == -1) {
        printk("exceed max open files\n");
        rollback_step = 2;
        goto rollback;
    }
    file_table[fd_idx].fd_flag = flag;
    file_table[fd_idx].fd_pos = 0;
    file_table[fd_idx].fd_inode = new_file_inode;
    file_table[fd_idx].fd_inode->write_deny = false;

    // 目录项相关
    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));

    create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

    // 将相关内存数据同步到磁盘中
    // 在父目录下安装目录项
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
        printk("sync dir_entry to disk failed\n");
        rollback_step = 3;
        goto rollback;
    }

    // 将父目录i结点信息同步到硬盘
    memset(io_buf, 0, 1024);
    inode_sync(cur_part, parent_dir->inode, io_buf);

    // 将新创建文件的i结点信息同步到硬盘
    memset(io_buf, 0, 1024);
    inode_sync(cur_part, new_file_inode, io_buf);

    // 将i结点位图同步到硬盘
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    // 将创建的文件i结点添加到open_inodes链表
    list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
    new_file_inode->i_open_cnt = 1;

    sys_free(io_buf);
    return pcb_fd_install(fd_idx);

    // 如果某个操作失败，需要进行回滚，回滚是累加的
    rollback:
        switch (rollback_step) {
            case 3:
                memset(&file_table[fd_idx], 0, sizeof(struct file));
            case 2:
                sys_free(new_file_inode);
            case 1:
                bitmap_set(&cur_part->inode_map, inode_no, 0);
                break;
        }
        sys_free(io_buf);
        return -1;
}

