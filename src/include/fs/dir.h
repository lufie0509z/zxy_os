#ifndef __FS_DIR_H
#define __FS_DIR_H

#include <fs/fs.h>
#include <fs/inode.h>
#include <kernel/global.h>

#define MAX_FILE_NAME_LEN 16 // 最长文件名

// 目录数据结构，磁盘中不存储
struct dir {               
    struct inode* inode;   // i结点
    uint32_t dir_pose;     // 记录目录项的偏移
    uint8_t dir_buf[512];  // 目录数据的缓存
};

// 目录项
struct dir_entry
{
    char filename[MAX_FILE_NAME_LEN];
    uint32_t i_no;           // i结点编号
    enum file_types f_type;  // 文件类型
};



extern struct dir root_dir;

void open_root_dir(struct partition* p);
struct dir* dir_open(struct partition* p, uint32_t i_no);
bool search_dir_entry(struct partition* p, struct dir* dir, const char* name, struct dir_entry* dir_e);
void dir_close(struct dir* dir);
void create_dir_entry (char* filename, uint32_t i_no, uint8_t f_type, struct dir_entry* d_en);
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf);

bool delete_dir_entry(struct partition* p, struct dir* pgdir, uint32_t i_no, void* io_buf);

struct dir_entry* dir_read(struct dir* dir);

bool dir_is_empty(struct dir* dir);
int32_t dir_remove(struct dir* parent_dir, struct dir* child_dir);

#endif