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
#include <kernel/debug.h>
#include <kernel/interrupt.h>
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
    return (p->sb->data_start_lba + bit_idx); // 返回扇区地址
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

// 打开结点编号为 i_no 的文件，成功返回其文件描述符号
int32_t file_open(uint32_t i_no, uint8_t flags) {

    int32_t fd_idx = get_free_slot_in_global();
    if (fd_idx == -1) {
        printk("exceed max open files\n");
        return -1;
    }

    file_table[fd_idx].fd_inode = inode_open(cur_part, i_no);
    file_table[fd_idx].fd_pos = 0;  // 每次打开文件时，都需要将该值置为0，使其指向文件开头
    file_table[fd_idx].fd_flag = flags;

    bool* write_deny = &file_table[fd_idx].fd_inode->write_deny;

    // 如果写文件
    if (flags & O_WRONLY || flags & O_RDWR) {
        enum intr_status old_status = intr_disable();
        if (!(*write_deny)) {
            // 没有别的进程在写文件，
            *write_deny = true;
            intr_set_status(old_status);
        } else {
            intr_set_status(old_status);
            printk("the file is written by other threaf, please try again later\n");
            return -1;
        }
    }

    return pcb_fd_install(fd_idx);
     
} 


// 关闭文件
int32_t file_close(struct file* f) {
    if (f == NULL) return -1;

    f->fd_inode->write_deny = false;
    inode_close(f->fd_inode);
    f->fd_inode = NULL; // 使文件结构可用
    return 0;
}

// 将buf中cnt个字节写入文件file，成功返回写入文件的字节数
int32_t file_write(struct file* file, const void* buf, uint32_t cnt) {
    if ((file->fd_inode->i_size + cnt) > (BLOCK_SIZE * (12 + 128))) {
        printk("exceed max file_size 71680 bytes, write file failed\n");
        return -1;
    }

    uint8_t* io_buf = (uint8_t*)sys_malloc(BLOCK_SIZE);    // 磁盘的读写操作都是以扇区为单位的
    if (io_buf == NULL) {
        printk("file_write: sys_malloc for io_buf failed\n");
        return -1;
    }

    uint32_t* all_blocks = (uint32_t*)sys_malloc(140 * 4);  // 用来记录文件140个数据块的块地址
    if (all_blocks == NULL) {
        printk("file_write: sys_malloc for all_blocks failed\n");
        return -1;
    }

    const uint8_t* src = (const uint8_t*)buf; //指向buf中待写入的数据
    uint32_t bytes_written = 0;     // 已经写入数据的字节大小
    uint32_t size_left = cnt;       // 剩余待写入的数据大小
    int32_t block_lba = -1;         // 操作的块地址
    uint32_t block_bitmap_idx = 0;  // 记录块在block_bitmap中的索引，作为参数传递给bitmap_sync
    uint32_t sec_idx;               // 扇区索引
    uint32_t sec_lba;               // 扇区地址
    uint32_t sec_off_bytes;         // 扇区内的字节偏移
    uint32_t sec_left_bytes;        // 扇区内剩余字节大小
    uint32_t chunk_size;            // 每次写入硬盘的数据块大小
    uint32_t block_idx;             // 块索引
    int32_t indirect_block_table;   // 一级间接表的地址

    /* 判断文件是不是第一次写，是的话就为他分配一个块
     * 每次分配块都需要同步 */
    if (file->fd_inode->i_sectors[0] == 0) {

        block_lba = block_bitmap_alloc(cur_part);
        
        if (block_lba == -1) {
            printk("file_write: block_bitmap_alloc failed\n");
            return -1;
        }
        file->fd_inode->i_sectors[0] = block_lba;

        // 每分配一个数据块就需要同步到磁盘中
        block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;

        bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
    }

    // 写文件前，文件已经被占用的块数
    uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;

    // 写文件后，文件占用的块数
    uint32_t file_will_use_blocks = (file->fd_inode->i_size + cnt) / BLOCK_SIZE + 1;
    ASSERT(file_will_use_blocks <= 140);

    uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;

    // 将文件的数据块扇区地址存入all_blocks中
    if (add_blocks == 0) {
        if (file_has_used_blocks <= 12) {
            block_idx = file_has_used_blocks - 1; // 指向最后一个数据块
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
        } else {
            ASSERT(file->fd_inode->i_sectors[12] != 0);

            indirect_block_table = file->fd_inode->i_sectors[12]; // 间接块的扇区地址
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    } else {
        if (file_will_use_blocks <= 12) {
            // 将原有扇区中包含剩余空间的(可继续用的)块地址存到all_blocks
            block_idx = file_has_used_blocks - 1;
            ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];

            // 再将未来要用的扇区分配好并写入all_blocks
            block_idx = file_has_used_blocks;
            while (block_idx < file_will_use_blocks) {
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 1 failed\n");
                    return -1;
                }
                ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;

                // 分配扇区时一定要同步到硬盘中
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

                block_idx++;
            }

        } else if (file_has_used_blocks <= 12 && file_will_use_blocks > 12) {
            block_idx = file_has_used_blocks - 1;
            ASSERT(file->fd_inode->i_sectors[block_idx] != 0);
            all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];  

            // 创建一级间接表
            block_lba = block_bitmap_alloc(cur_part);
            if (block_lba == -1) {
                printk("file_write: block_bitmap_alloc for situation 2 failed\n");
                return -1;
            }

            ASSERT(file->fd_inode->i_sectors[12] == 0);
            // 分配一级间接索引表
            indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;

            while (block_idx < file_will_use_blocks) {
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba == -1) {
                    printk("file_write: block_bitmap_alloc for situation 1 failed\n");
                    return -1;
                }
                
                if (block_idx < 12) {
                    // 直接块
                    ASSERT(file->fd_inode->i_sectors[block_idx] == 0);
                    file->fd_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
                } else {
                    // 间接块写入先all_blocks数组中，分配完成后一次性同步到磁盘中
                    all_blocks[block_idx] = block_lba;
                }

                // 同步
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

                block_idx++;
            }
            // 将一级间接索引表直接写入硬盘
            ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        } else {
            ASSERT(file->fd_inode->i_sectors[12] != 0);
            indirect_block_table = file->fd_inode->i_sectors[12];

            // 获取一级间接表
            ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);

            block_idx = file_has_used_blocks;
            while (block_idx < file_will_use_blocks) {
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba = -1) {
                    printk("file_write: block_bitmap_alloc for situation 3 failed\n");
                    return -1;
                }
                all_blocks[block_idx++] = block_lba;

                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
            }
            ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
        }
    }

    bool first_write_block = true;  // 包含剩余空间的(可继续用的)的扇区
    file->fd_pos = file->fd_inode->i_size - 1; // fd_pose置为文件大小-1
    while (bytes_written < cnt) {
        memset(io_buf, 0, BLOCK_SIZE);
        sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
        sec_lba = all_blocks[sec_idx];
        sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
        sec_left_bytes = BLOCK_SIZE - sec_off_bytes;

        // 本次写文件的字节大小
        chunk_size =(size_left < sec_left_bytes) ? size_left : sec_left_bytes;
        if (first_write_block) {
            ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
            first_write_block = false;
        }
        memcpy(io_buf + sec_off_bytes, src, chunk_size);
        printk("file write at lba 0x%x\n", sec_lba); 

        src += chunk_size;
        file->fd_inode->i_size += chunk_size;
        file->fd_pos += chunk_size;
        bytes_written += chunk_size;
        size_left -= chunk_size;
    }

    inode_sync(cur_part, file->fd_inode, io_buf);
    sys_free(all_blocks);
    sys_free(io_buf);
    return bytes_written;
}


