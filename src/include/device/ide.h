#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H
#include <lib/kernel/stdint.h>
#include <lib/kernel/bitmap.h>
#include <kernel/list.h>
#include <kernel/sync.h>

struct partition // 分区
{
    uint32_t start_lba;          // 开始扇区
    uint32_t sec_cnt;            // 扇区数
    struct disk* my_disk;        // 该分区在哪块硬盘上
    struct list_elem part_tag;   // 在队列中的标记
    char name[8];                // 分区名称
    struct super_block* sb;      // 超级块指针，此处仅占位
    struct bitmap block_bitmap;  // 块位图，用于管理本分区所有的块
    struct bitmap inode_map;     // i结点管理位图
    struct list open_inodes;     // 分区所打开的inode队列，文件系统中用到
};

// 硬盘
struct disk {
    char name[8];
    struct ide_channel* my_channel;  // 本硬盘所属的通道
    uint8_t dev_no;                  // 0是主盘，1是从盘
    struct partition prim_parts[4];  // 主分区
    struct partition logic_parts[8]; // 逻辑分区
};

// 
struct ide_channel {
    char name[8];
    uint16_t port_base;          // 通道的端口基址
    uint8_t irq_no;              // 本通道的中断号
    struct lock lock;
    bool expecting_intr;         // 本通道正在等待硬盘中断
    struct semaphore disk_done;  // 驱动程序的信号量
    struct disk devices[2];      // 一个通道有2个硬盘
};

extern uint8_t channel_cnt;
extern struct ide_channel channels[];
extern struct list partition_list;

void ide_init();
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt);
void intr_hd_handler(uint8_t irq_no);
#endif

