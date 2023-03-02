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

#endif