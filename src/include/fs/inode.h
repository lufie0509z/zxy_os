#ifndef __FS_INODE_H
#define __FS_INODE_H
#include <kernel/list.h>
#include <kernel/global.h>
#include <device/ide.h>

// i结点结构体
struct inode {
    uint32_t i_no;           // i结点编号，也是它在数组中的下标
    uint32_t i_size;         // 以字节为单位，普通文件/目录表（目录表项之和）的大小
    uint32_t i_open_cnt;     // 记录文件被打开的次数
    bool write_deny;         // 写操作不能同时

    uint32_t i_sectors[13];  // 前12个是直接块指针，第13个存储的是一级间接块指针
    struct list_elem inode_tag;  // i结点的标识，用于加入已打开的i结点列表，inode缓存

};
void inode_sync(struct partition* p, struct inode* inode, void* io_buf);
struct inode* inode_open(struct partition* p, uint32_t i_no);
void inode_init(uint32_t i_no, struct inode* new_inode);
void inode_close(struct inode* inode);

void inode_delete(struct partition* p, uint32_t i_no, void* io_buf);
void inode_release(struct partition* p, uint32_t i_no);
#endif