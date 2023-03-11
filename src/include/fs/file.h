#ifndef __FS_FILE_H
#define __FS_FILE_H
#include <fs/inode.h>
#include <device/ide.h>
#include <kernel/global.h>

#define MAX_FILES_OPEN 32 // 系统可打开的最大文件次数，因为一个文件可以多次打开

// 文件结构
struct file {
    uint32_t fd_pos;  // 文件操作在文件内的偏移量
    uint32_t fd_flag; // 文件操作标识符
    struct inode* fd_inode;
};

// 标准输入输出描述符
enum std_fd {
    stdin_no,         // 0 标准输入
    std_out,          // 1 标准输出
    std_err           // 2 标准错误
};

// 位图类型
enum bitmap_type {
    INODE_BITMAP,    // i结点位图
    BLOCK_BITMAP     // 数据块位图
};

extern struct file file_table[MAX_FILES_OPEN];

int32_t get_free_slot_in_global();
int32_t pcb_fd_install(int32_t global_fd_idx);
int32_t inode_bitmap_alloc(struct partition* p);
int32_t block_bitmap_alloc(struct partition* p);
void bitmap_sync(struct partition* p, uint32_t bit_idx, uint8_t bitmap_type);

int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag);

int32_t file_open(uint32_t i_no, uint8_t flags);
int32_t file_close(struct file* f);

int32_t file_write(struct file* file, const void* buf, uint32_t cnt);
int32_t file_read(struct file* file, void* buf, uint32_t cnt);

#endif

