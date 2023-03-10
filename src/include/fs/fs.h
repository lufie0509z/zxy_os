#ifndef __FS_FS_H
#define __FS_FS_H

#include <device/ide.h>
#include <kernel/global.h>

#define MAX_FILES_PER_PART 4096    // 每个分区支持的最多文件数量
#define BITS_PER_SECTOR    4096    // 每个扇区的位数
#define SECTOR_SIZE        512     // 每个扇区的字节数
#define BLOCK_SIZE SECTOR_SIZE     // 每个块大小，这里一个块就是一个扇区

#define MAX_PATH_LEN       512     // 文件目录的最大路径长度（strlen(pathname)）

enum file_types {
    FT_UNKOWN,    
    FT_REGULAR,    // 普通文件
    FT_DIR         // 目录文件
};

enum oflags {
    O_RDONLY,      // 只读
    O_WRONLY,      // 只写
    O_RDWR,        // 读写
    O_CREATE = 4   // 创建文件，可以利用位操作复合参数/反推标识符
};

extern struct partition* cur_part;

struct path_search_record {
    char searched_path[MAX_PATH_LEN];  // 记录已经处理过的路径，其中最后一级目录未必存在，前面的所有路径都是存在的
    struct dir* parent_dir;            // 待查找目标的直接父目录
    enum file_types f_type;
};

void filesys_init();
int32_t path_depth_cnt (char* pathname);

int32_t sys_open(const char* fpathname, uint8_t flags);
int32_t sys_close(uint32_t fd);

int32_t sys_write(int32_t fd, void* buf, uint32_t cnt);
#endif
