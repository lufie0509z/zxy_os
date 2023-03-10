#include <fs/fs.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/inode.h>
#include <fs/super_block.h>
#include <device/ide.h>
#include <kernel/memory.h>
#include <kernel/string.h>
#include <kernel/debug.h>
#include <kernel/global.h>
#include <lib/kernel/stdio-kernel.h>

struct dir root_dir;  // 根目录

// 打开根目录
void open_root_dir(struct partition* p) {
    root_dir.inode = inode_open(p, p->sb->root_inode_no);
    root_dir.dir_pose = 0;
}

// 打开i结点编号为i_no的目录，返回目录指针
struct dir* dir_open(struct partition* p, uint32_t i_no) {
    struct dir* dir = (struct dir*)sys_malloc(sizeof(struct dir));
    dir->inode = inode_open(p, i_no);
    dir->dir_pose = 0;
    return dir;
}

// 在目录中找到名字时name的文件或目录并将其存入目录项dir_e中
bool search_dir_entry(struct partition* p, struct dir* dir, const char* name, struct dir_entry* dir_e) {

    uint32_t block_cnt = 12 + 128; // 12个直接块+128个一级间接块

    uint32_t* all_blocks = (uint32_t*)sys_malloc(140 * 4);  // 此目录的全部i结点扇区地址
    if (all_blocks == NULL) {
        printk("search_dir_entry: sys_malloc for all_blocks failed");
        return false;
    }

    uint32_t block_idx = 0;
    while (block_idx < 12) {  // 将直接块存储的扇区地址记录下来
        all_blocks[block_idx] = dir->inode->i_sectors[block_idx];
        block_idx++;
    }

    // 处理一级间接块
    if (dir->inode->i_sectors[12] != 0) {
        ide_read(p->my_disk, dir->inode->i_sectors[12], all_blocks + 12, 1);
    }

    uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);  // 目录项不会垮扇区
    struct dir_entry* p_de = (struct dir_entry*)buf;   // p_de为目录项指针，初始值为buf

    uint32_t dir_entry_size = p->sb->dir_entry_size;
    uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

    block_idx = 0;
    
    while(block_idx < block_cnt) {
        if (all_blocks[block_idx] == 0) { // 该数据块中没有信息
            block_idx++;
            continue;
        }
        ide_read(p->my_disk, all_blocks[block_idx], buf, 1);  // 读入扇区数据
        
        uint32_t dir_entry_idx = 0;
        
        while(dir_entry_idx < dir_entry_cnt) {
            // printk("%s %s\n", p_de->filename, name);
            if (!strcmp(name, p_de->filename)) { // 找到了
                memcpy(dir_e, p_de, dir_entry_size);
                sys_free(buf);
                sys_free(all_blocks);
                return true;
            }
            dir_entry_idx++;
            p_de++;
        }
        block_idx++;
        
        p_de = (struct dir_entry*)buf; // p_de 指向了缓冲区buf尾
        memset(buf, 0, SECTOR_SIZE);   // 下一轮循环会重新将磁盘信息读入缓冲区中
    }
    sys_free(buf);
    sys_free(all_blocks);
    return false;
}

// 关闭目录，关闭目录的i结点并释放目录结构体所占用的内存空间
void dir_close(struct dir* dir) {
    if (dir == &root_dir) return; // 根目录在低端1M内存空间，不在堆上
    inode_close(dir->inode);
    sys_free(dir);
}
   
// 在内存中初始化目录项
void create_dir_entry (char* filename, uint32_t i_no, uint8_t f_type, struct dir_entry* d_en) {
    ASSERT(strlen(filename) <= MAX_FILE_NAME_LEN);

    memcpy(d_en->filename, filename, strlen(filename));
    d_en->i_no = i_no;
    d_en->f_type = f_type;
}

// 将目录项p_de加入到父目录中，io_buf由主调函数提供
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf) {
    struct inode* parent_dir_inode = parent_dir->inode;
    uint32_t dir_size = parent_dir_inode->i_size;  // 父目录大小（所有目录项之和）
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

    ASSERT(dir_size % dir_entry_size == 0);

    uint32_t dir_entrys_per_sec = SECTOR_SIZE / dir_entry_size; 
    int32_t block_lba = -1;

    uint8_t block_idx = 0;               // 用来遍历all_blocks
    uint32_t all_blocks[12 + 128] = {0}; // 该数组存储父目录所有数据块的lba，默认为0

    while (block_idx < 12) {             // 将直接块lba地址直接存入all_blocks数组中
        all_blocks[block_idx] = parent_dir_inode->i_sectors[block_idx];
        block_idx++;
    }

    // if (parent_dir_inode->i_sectors[12] != 0) {
    //     ide_read(cur_part->my_disk, parent_dir_inode->i_sectors[12], all_blocks + 12, 1);
    // }

    struct dir_entry* dir_e = (struct dir_entry*)io_buf;  // 目录项指针dir_e现在指向io_buf起始处
    int32_t block_bitmap_idx = -1;        // 数据块位图中相对于数据块位图起始处的offset

    block_idx = 0;

    while (block_idx < 140) {
        block_bitmap_idx = -1;
        if (all_blocks[block_idx] == 0) {
           
            block_lba = block_bitmap_alloc(cur_part);    
            if (block_lba == -1) {
                printk("alloc block bitmap for sync_dir_entry failed\n");
                return false;
            }
            // 每次分配一个数据块就必须同步到硬盘中
            block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
            // printk("%d %d\n", block_lba, cur_part->sb->data_start_lba);
            bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

            block_bitmap_idx = -1;

            if (block_idx < 12) { 
                // 直接块，写入分配的扇区地址写入
                parent_dir_inode->i_sectors[block_idx] = all_blocks[block_idx] = block_lba;
            } else if (block_idx == 12) {
                // 一级间接索引块尚未分配，上面分配的数据块将作为一级间接块地址
                parent_dir_inode->i_sectors[12] = block_lba;
                block_lba = -1;
                block_lba = block_bitmap_alloc(cur_part);
                if (block_lba == -1) {
                    // 分配失败，需要将之前的操作回滚
                    block_bitmap_idx = parent_dir_inode->i_sectors[12] - cur_part->sb->data_start_lba;
                    bitmap_set(&cur_part->block_bitmap, block_bitmap_idx, 0);
                    parent_dir_inode->i_sectors[12] = 0;
                    printk("alloc block bitmap for sync_dir_entry failed\n");
                    return false;
                }

                // 每次分配一个数据块就必须同步到硬盘中
                block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
                bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

                all_blocks[block_idx] = block_lba;

                // 将新分配的第0个间接块地址写入一级间接块索引表
                ide_write(cur_part->my_disk, parent_dir_inode->i_sectors[12], all_blocks + 12, 1);
            } else {
                // 一级间接索引块尚已存在，将间接块地址写入磁盘中的索引表
                all_blocks[block_idx] = block_lba;
                ide_write(cur_part->my_disk, parent_dir_inode->i_sectors[12], all_blocks + 12, 1);
            }

            // 将目录项写入新分配的块中
            memset(io_buf, 0, 512);
            memcpy(io_buf, p_de, dir_entry_size);        // 将目录项信息写入缓冲区中
            ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
            parent_dir_inode->i_size += dir_entry_size;  // 修改父目录的相关信息
            return true;
        }

        // 将数据块读入内存，寻找空的目录项
        ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
        uint8_t dir_entry_idx = 0;
        dir_e = (struct dir_entry*)io_buf; 
        // printk("all_blocks[block_idx] != 0\n");
        while (dir_entry_idx < dir_entrys_per_sec) {
            if ((dir_e + dir_entry_idx)->f_type == FT_UNKOWN) {
                memcpy((dir_e + dir_entry_idx), p_de, dir_entry_size);  // dir_e指向io_buf起始地址
                ide_write(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
                parent_dir_inode->i_size += dir_entry_size;
                // printk("return true;");
                return true;
            }
            dir_entry_idx++;
        }
        
        block_idx++;
    }
    
    printk("directory is full!\n");
    return false;
}
