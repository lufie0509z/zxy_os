#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H
#include <kernel/global.h>

// 超级块
struct super_block {
    uint32_t magic;             // 魔数，用来标识不同的文件系统
    uint32_t sec_cnt;           // 扇区总数
    uint32_t inode_cnt;         // 本分区中的inode数量，也是最大文件数
    uint32_t part_lba_base;     // 本分区的起始lba地址

    uint32_t block_bitmap_lba;  // 块位图起始lba地址
    uint32_t block_bitmap_secs; // 块位图占有的扇区数

    uint32_t inode_bitmap_lba;  // i结点
    uint32_t inode_bitmap_secs;

    uint32_t inode_table_lba;   // i结点数组
    uint32_t inode_table_secs;

    uint32_t data_start_lba;    // 数据块的起始lba地址
    uint32_t root_inode_no;     // 根目录所在的i结点编号
    uint32_t dir_entry_size;    // 目录项大小

    uint8_t pad[460];           // 填充用
} __attribute__ ((packed));
#endif
