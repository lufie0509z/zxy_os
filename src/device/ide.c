#include <device/ide.h>
#include <device/timer.h>
#include <device/console.h>
#include <kernel/io.h>
#include <kernel/debug.h>
#include <kernel/global.h>
#include <kernel/memory.h>
#include <lib/stdio.h>
#include <lib/kernel/stdint.h>
#include <lib/kernel/stdio-kernel.h>

// 硬盘控制器不同寄存器的端口号
#define reg_data(channel)      (channel->port_base + 0)
#define reg_error(channel)     (channel->port_base + 1)
#define reg_sect_cnt(channel)  (channel->port_base + 2)
#define reg_lba_l(channel)     (channel->port_base + 3)
#define reg_lba_m(channel)     (channel->port_base + 4)
#define reg_lba_h(channel)     (channel->port_base + 5)
#define reg_dev(channel)       (channel->port_base + 6)
#define reg_status(channel)    (channel->port_base + 7)
#define reg_cmd(channel)       (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)       (reg_alt_status(channel))

// 寄存器中的关键位 BIT_寄存器名称_位名
// reg_alt_status
#define BIT_STAT_BSY    0x80    // 硬盘忙
#define BIT_STAT_DRDY   0x40    // 驱动器准备好了
#define BIT_STAT_DRQ    0x8     // 数据传输准备好了

// device
#define BIT_DEV_MBS     0xa0    // 指device中的第5位和第7位，两位固定为1
#define BIT_DEV_LBA     0x40    
#define BIT_DEV_DEV     0x10

// 硬盘操作命令
#define CMD_IDENTIFY     0xec   // 获取硬盘身份信息
#define CMD_READ_SECTOR  0x20   // 读扇区
#define CMD_WRITE_SECTOR 0x30   // 写扇区

// 最大的lba地址，用于调试避免扇区地址越界
#define max_lba ((80 * 1024 * 1024 / 512) - 1)

uint8_t channel_cnt;                // 通道数 硬盘数/2
struct ide_channel channels[2];     // 通道数组，有两个ide通道

int32_t ext_lba_base = 0;           // 总扩展分区的起始lba，扫描函数中作为扫描分区表的标记
uint8_t p_no = 0, l_no = 0;             // 主分区和逻辑分区的下标
struct list partition_list;         // 分区队列

// 分区表项结构体
struct partition_table_entry {
    uint8_t bootable;        // 是否可引导
    uint8_t start_head;      // 起始磁头号
    uint8_t start_sec;       // 起始扇区号
    uint8_t start_chs;       // 起始柱面号
    uint8_t fs_type;         // 分区的类型
    uint8_t end_head;
    uint8_t end_sec;
    uint8_t end_chs;

    uint32_t start_lba;        // 本分区起始逻辑区块地址lba
    uint32_t sec_cnt;          // 扇区总数
} __attribute__((packed));    // 压缩的，不允许为了对齐而填充

// 引导扇区结构体
struct boot_sector {
    uint8_t other[446];       // 引导代码，此处用于占位
    struct partition_table_entry partition_table[4];  // 分区表
    uint16_t signature;       // 魔数，小端序
} __attribute__((packed)); 


// 选择读写的磁盘
static void select_disk(struct disk* hd) {
    uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
    if (hd->dev_no == 1) {// 0是主盘，1是从盘
        reg_device |= BIT_DEV_DEV;
    }
    outb(reg_dev(hd->my_channel), reg_device);
}

// 向硬盘控制器写入起始扇区地址及要读写的扇区数
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {
    ASSERT(lba <= max_lba);
    struct ide_channel* channel = hd->my_channel;

    outb(reg_sect_cnt(channel), sec_cnt); // 写入读写的扇区数

    // 写入扇区的lba
    outb(reg_lba_l(channel), lba);
    outb(reg_lba_m(channel), lba >> 8);
    outb(reg_lba_h(channel), lba >> 16);

    // lba地址的24-27位需要存入device寄存器的0-3位
    outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);

}

// 向通道channel发送cmd命令
static void cmd_out(struct ide_channel* channel, uint8_t cmd) {
    channel->expecting_intr = true;
    outb(reg_cmd(channel), cmd);
}


// 从硬盘中读sector_cnt个扇区到缓冲区buf中
static void read_from_sector(struct disk* hd, void* buf, uint8_t sector_cnt) {
    uint32_t size_in_byte;
    if (sector_cnt == 0) size_in_byte = 256 * 512;
    else size_in_byte = sector_cnt * 512;

    // 以字（2个字节）为单位读取
    insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

// 将缓冲区中sector_cnt个扇区写入硬盘中
static void write_to_sector(struct disk* hd, void* buf, uint8_t sector_cnt) {
    uint32_t size_in_byte;
    if (sector_cnt == 0) size_in_byte = 256 * 512;
    else size_in_byte = sector_cnt * 512;

    // 以字为单位写入
    outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

// 等代硬盘30秒，判断硬盘的状态
static bool busy_wait(struct disk* hd) {
    struct ide_channel* channel = hd->my_channel;
    uint16_t time_limit = 30 * 1000;
    while (time_limit -= 10 >= 0) {
        if(!(inb(reg_status(channel)) & BIT_STAT_BSY)) return(inb(reg_status(channel)) & BIT_STAT_DRQ);
        else mtime_sleep(10); // status寄存器的BSY位为1，硬盘繁忙
    }
    return false;
}

// 从硬盘中读取sec_cnt个扇区到buf
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);

    lock_acquire(&hd->my_channel->lock);

    select_disk(hd);     // 选择操作的硬盘

    uint32_t secs_op;    // 一次读写的扇区数
    uint32_t secs_done = 0;  // 已读写的扇区数
    while (secs_done < sec_cnt) { 
        //计算本次读写的扇区数，读写扇区数端口是8位寄存器
        if ((secs_done + 256) <= sec_cnt) secs_op = 256;
        else secs_op = sec_cnt - secs_done;

        select_sector(hd, lba + secs_done, secs_op); // 写入起始扇区地址及要读写的扇区数

        cmd_out(hd->my_channel, CMD_READ_SECTOR);  // 向reg_cmd寄存器发出写命令

        sema_down(&hd->my_channel->disk_done);     // 将驱动程序阻塞

        if (!busy_wait(hd)) { // 读失败
            char error[64];
            sprintf(error, "%s read sector %d failed!!!!!!\n", hd->name, lba);
            PANIC(error);
        }

        read_from_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);    // 将扇区数据读入到缓冲区
        secs_done += secs_op;
    }

    lock_release(&hd->my_channel->lock);

}

// 将buf中的sec_cnt个扇区写到硬盘
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
    ASSERT(lba <= max_lba);
    ASSERT(sec_cnt > 0);

    lock_acquire(&hd->my_channel->lock);

    select_disk(hd);

    uint32_t secs_op, secs_done = 0;

    while (secs_done < sec_cnt) {
        if ((secs_done + 256) <= sec_cnt) secs_op = 256;
        else secs_op = sec_cnt - secs_done;

        select_sector(hd, lba + secs_done, secs_op);
        cmd_out(hd->my_channel, CMD_WRITE_SECTOR);

        if(!busy_wait(hd)) {
            char error[64];
            sprintf(error, "%s write sector %d failed!!!!!!\n", hd->name, lba);
            PANIC(error);
        }

        write_to_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);

        sema_down(&hd->my_channel->disk_done);

        secs_done += secs_op;
    }

    lock_release(&hd->my_channel->lock);

}



// 将dst中len个相邻字节交换位置后存入buf
static void swap_pairs_bytes(const char* dst, char* buf, uint32_t len) {
    uint8_t idx;
    for (idx = 0; idx < len; idx += 2) {
        buf[idx + 1] = *dst++;
        buf[idx] = *dst++;
    }
    buf[idx] = '\0';
}

// 获取硬盘的参数信息
static void identify_disk(struct disk* hd) {
    char id_info[512];  // 用于存储返回后的硬盘信息

    select_disk(hd);
    cmd_out(hd->my_channel, CMD_IDENTIFY);   // 选择硬盘并向通道发送命令

    sema_down(&hd->my_channel->disk_done);    // 阻塞自己，等待处理完成后通过中断程序唤醒

    if(!busy_wait(hd)) {                      // 判断硬盘状态
        char error[64];
        sprintf(error, "%s identify failed!!!!!!", hd->name);
        PANIC(error);
    }

    read_from_sector(hd, id_info, 1);         // 读取硬盘信息
    char buf[64]; 
    uint8_t sn_start = 10 * 2, sn_len = 20;   // 序列号信息，10表示字偏移量
    swap_pairs_bytes(&id_info[sn_start], buf, sn_len);
    printk("  disk %s info:\n    SN: %s\n", hd->name, buf);
    memset(buf, 0, sizeof(buf));

    uint8_t md_start = 27* 2, md_len = 40;    // 型号信息
    swap_pairs_bytes(&id_info[md_start], buf, md_len);
    printk("    MODULE: %s\n", buf);

    uint32_t sectors = *(uint32_t*)&id_info[60 * 2];
    printk("    SECTORS: %d\n", sectors);
    printk("    CAPACITY: %dMB\n", sectors * 512 / 1024/ 1024);
    
}


// 扫描硬盘hd中地址为ext_lba的扇区中所有分区，针对每个子扩展分区会递归调用
static bool partition_scan(struct disk* hd, uint32_t ext_lba) {
    // 避免递归调用时栈溢出，利用指针bs存储子扩展分区所在的扇区
    struct boot_sector* bs = sys_malloc(sizeof(struct boot_sector));
    ide_read(hd, ext_lba, bs, 1);
    struct partition_table_entry* p = bs->partition_table;  // 获取分区表地址
    uint8_t idx = 0;
    while (idx++ < 4) {
        if (p->fs_type == 0x5) {   // 是扩展分区
            if (ext_lba_base != 0) {
                // 获取到的是EBR引导扇区中的分区表，子扩展分区的起始扇区地址是相对于主引导扇区中的总扩展分区地址ext_lba_bas
                partition_scan(hd, p->start_lba + ext_lba_base);
            } else { //说明这是第一次调用partition_scan，得到的是MBR引导扇区的分区表
                ext_lba_base = p->start_lba; // 记录下总扩展分区的起始lba地址，所有的子扩展分区都相对于它
                partition_scan(hd, p->start_lba);
            } 
        } else if(p->fs_type != 0) {
            if (ext_lba == 0) { //当前是MBR引导扇区，扩展分区的情况已经处理过了，此时是主分区
                // 将主分区的信息收录到硬盘 hd 的 prim_parts 数组中
                hd->prim_parts[p_no].start_lba = ext_lba + p->start_lba;
                hd->prim_parts[p_no].sec_cnt = p->sec_cnt;
                hd->prim_parts[p_no].my_disk = hd;
                list_append(&partition_list, &hd->prim_parts[p_no].part_tag); // 加入分区队列
                sprintf(hd->prim_parts[p_no].name, "%s%d", hd->name, p_no + 1);
                p_no++;
                ASSERT(p_no < 4);
            } else { //逻辑分区
                hd->logic_parts[l_no].start_lba = ext_lba + p->start_lba;
                hd->logic_parts[l_no].sec_cnt = p->sec_cnt;
                hd->logic_parts[l_no].my_disk = hd;
                list_append(&partition_list, &hd->logic_parts[l_no].part_tag);
                sprintf(hd->logic_parts[l_no].name, "%s%d", hd->name, l_no + 5); // 逻辑分区从5开始，主分区有4个
                l_no++;
                if (l_no >= 8) return; // 仅支持8个逻辑分区

            }
        }
        p++;

    }
    sys_free(bs);

}

// 打印分区相关信息
static bool partition_info(struct list_elem* pelem, int arg UNUSED) {
    struct partition* p = elem2entry(struct partition, part_tag, pelem);
    printk("    %s start_lab: 0x%x, sec_cnt: 0x%x\n", p->name, p->start_lba, p->sec_cnt);
    // 被用在 list_traversal 中作为回调函数调用
    return false;
}

// 硬盘中断处理程序，负责两个通道的中断
void intr_hd_handler(uint8_t irq_no) {
    ASSERT(irq_no == 0x2e || irq_no == 0x2f);
    uint8_t ch_no = irq_no - 0x2e; // 通道号
    struct ide_channel* channel = &channels[ch_no];
    ASSERT(channel->irq_no == irq_no);

    // 如果通道发生了中断信号，只会有最近一次的硬盘操作引起
    if (channel->expecting_intr) {
        channel->expecting_intr = false;
        sema_up(&channel->disk_done); // 唤醒阻塞在此信号量上的驱动程序

        inb(reg_status(channel));     // 中断处理完成，显式通知硬盘控制器
    } // 错误情况暂不处理
}



// 初始化相关数据结构
void ide_init() {
    printk("ide_init start.\n");
    uint8_t hd_cnt = *((uint8_t*)(0x475));  // 获取硬盘数量
    ASSERT(hd_cnt > 0);
    channel_cnt = DIV_ROUND_UP(hd_cnt, 2);  // 根据硬盘数获取通道数量
    struct ide_channel* channel;
    uint8_t channel_no = 0;

    while (channel_no < channel_cnt) { // 处理每一个通道上的两个硬盘
        channel = &channels[channel_no];
        sprintf(channel->name, "ide%d", channel_no);
        switch (channel_no) {
            case 0:
                channel->port_base = 0x1f0;
                channel->irq_no = 0x20 + 14;  //从片8259A上倒数第二的中断引脚,也就是ide0通道的中断向量号
                break;
            case 1:
                channel->port_base = 0x170;
                channel->irq_no = 0x20 + 15;  // 从8259A上最后一个中断引脚,我们用来响应ide1通道上的硬盘中断
                break;
        }
      
        channel->expecting_intr = false;
        lock_init(&channel->lock);
        sema_init(&channel->disk_done, 0);

        register_handler(channel->irq_no, intr_hd_handler);

        uint8_t dev_no = 0;
        while(dev_no < 2) {
   
            struct disk* hd = &channel->devices[dev_no];
            hd->my_channel = channel;
            hd->dev_no = dev_no;
            sprintf(hd->name, "sd%c", 'a' + channel_no * 2 + dev_no);
  
            identify_disk(hd); // 获取硬盘信息
            if (dev_no != 0) partition_scan(hd, 0); //扫描硬盘
            p_no = 0, l_no = 0;
            dev_no++;
        }
        dev_no = 0; // 将硬盘驱动器号置 0，为下一个 channel 的两个硬盘初始化
        channel_no++;
    }

    printk("\n  all partition info:\n");
    list_traversal(&partition_list, partition_info, (int)NULL);

    printk("ide_init end.\n");
}