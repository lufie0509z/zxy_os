#ifndef __FS_FS_H
#define __FS_FS_H
#include <kernel/global.h>

#define MAX_FILES_PER_PART 4096    // 每个分区支持的最多文件数量
#define BITS_PER_SECTOR    4096    // 每个扇区的位数
#define SECTOR_SIZE        512     // 每个扇区的字节数
#define BLOCK_SIZE SECTOR_SIZE     // 每个块大小，这里一个块就是一个扇区

enum file_types {
    FT_UNKOWN,    
    FT_REGULAR,    // 普通文件
    FT_DIR         // 目录文件
};

void filesys_init();

#endif
