#include <fs/fs.h>
#include <fs/dir.h>
#include <fs/inode.h>
#include <fs/super_block.h>
#include <device/ide.h>
#include <kernel/list.h>
#include <kernel/debug.h>
#include <kernel/global.h>
#include <kernel/string.h>
#include <kernel/memory.h>
#include <lib/kernel/bitmap.h>
#include <lib/kernel/stdio-kernel.h>

struct partition* cur_part;  // 默认分区

// 为p分区创建文件系统，初始化元信息
static void partition_format(struct partition* p) {

    uint32_t boot_sector_secs = 1;   // 引导块占用的扇区数
    uint32_t super_block_secs = 1;   // 超级块占用的扇区数

    // i结点位图占用的扇区数，此处为1个扇区
    uint32_t inode_bitmap_secs = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
    // i结点数组占用的扇区数
    uint32_t inode_table_secs = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)), SECTOR_SIZE);

    uint32_t used_secs = boot_sector_secs + super_block_secs + inode_bitmap_secs + inode_table_secs;
    uint32_t free_secs = p->sec_cnt - used_secs;  // 空闲块（位图 +数据块）

    uint32_t block_bitmap_secs;
    block_bitmap_secs = DIV_ROUND_UP(free_secs, BITS_PER_SECTOR);
    uint32_t block_bitmap_bit_len = free_secs - block_bitmap_secs;  // 块位图长度，也就是块的数量
    block_bitmap_secs = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    // 超级块
    struct super_block sb;
    sb.magic = 0x19590318;
    sb.sec_cnt = p->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = p->start_lba;

    sb.block_bitmap_lba = sb.part_lba_base + 2;  // 前两个是引导块和超级块
    sb.block_bitmap_secs = block_bitmap_secs;

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_secs;
    sb.inode_bitmap_secs = inode_bitmap_secs;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_secs;
    sb.inode_table_secs = inode_table_secs;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_secs;
    sb.root_inode_no = 0;  // 根结点的i结点编号
    sb.dir_entry_size = sizeof(struct dir_entry);

    printk("%s info:\n", p->name);
    printk("    magic: 0x%x\n    part_lba_base: %x\n", sb.magic, sb.part_lba_base);
    printk("    all_sectors: 0x%x\n    inode_cnt: 0x%x\n", sb.sec_cnt, sb.inode_cnt);
    printk("    block_bitmap_lba: 0x%x\n    block_bitmap_sectors: 0x%x\n", sb.block_bitmap_lba, sb.block_bitmap_secs);
    printk("    inode_bitmap_lba: 0x%x\n    inode_bitmap_sectors: 0x%x\n", sb.inode_bitmap_lba, sb.inode_bitmap_secs);
    printk("    inode_table_lba: 0x%x\n    inode_table_sectors: 0x%x\n",sb.inode_table_lba, sb.inode_table_secs);
    printk("    data_start_lba: 0x%x\n", sb.data_start_lba);

    struct disk* hd = p->my_disk;

    // 将超级块的内容写入本分区的1号扇区（0号是引导块）
    ide_write(hd, p->start_lba + 1 , &sb, 1);
    printk("    super_block_lba: 0x%x\n", p->start_lba + 1);

    // 找出数据量最大的元信息，用它的尺寸作为缓冲区
    uint32_t buf_size = (sb.block_bitmap_secs >= sb.inode_bitmap_secs ? sb.block_bitmap_secs : sb.inode_bitmap_secs);
    buf_size = (buf_size >= sb.inode_table_secs ? buf_size : sb.inode_table_secs);
    buf_size = buf_size * SECTOR_SIZE;
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size); 

    // 初始化数据块位图并写入磁盘
    buf[0] |= 0x01;  // 第0块为根目录，先在位图中占位
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8;                 // 块位图中的最后一个字节
    uint8_t block_bitmap_last_bit = block_bitmap_bit_len % 8;                   // 块位图中最后一个字节有多少有效位
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);  // 最后一个扇区中，不足一扇区的字节数

    memset(&buf[block_bitmap_last_byte], 0xff, last_size); //将位图中最后一个扇区中无效的部分（包括最后包括有效位的一个字节）全部置1
    
    // 将最后一个字节中的有效位置0
    uint8_t bit_idx = 0; 
    while (bit_idx <= block_bitmap_last_bit) buf[block_bitmap_last_byte] &= ~(1 << bit_idx++);

    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_secs);

    // 初始化inode位图，最多有4096个文件，位图占用的扇区大小固定为1个
    memset(buf, 0, buf_size); // 清空缓存区
    buf[0] |= 0x01;  // 第0个i结点为根目录
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_secs);

    // 初始化i结点数组并写入磁盘
    memset(buf, 0, buf_size);
    struct inode* i = (struct inode*)buf;
    i->i_no = 0;                          // 根目录
    i->i_size = sb.dir_entry_size * 2;    // 当前目录.和上级目录..
    i->i_sectors[0] = sb.data_start_lba;  // 将根目录放在最开始的数据块中
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_secs);

    // 初始化根目录，写入目录项
    memset(buf, 0, buf_size);
    struct dir_entry* d = (struct dir_entry*)buf;

    memcpy(d->filename, ".", 1);   // 初始化当前目录.
    d->i_no = 0;
    d->f_type = FT_DIR;

    d++;                           // 指向下一个目录项

    memcmp(d->filename, "..", 2);  // 初始化上一级目录..
    d->i_no = 0;
    d->f_type = FT_DIR;

    ide_write(hd, sb.data_start_lba, buf, 1);

    printk("    root_dir_lba: 0x%x\n", sb.data_start_lba);
    sys_free(buf);

}

// 找到默认分区，然后将元信息读入内存
static bool partition_mount(struct list_elem* pelem, int arg) {
    char* part_name = (char*)arg;       // 将arg还原为字符指针
    struct partition* p = elem2entry(struct partition, part_tag, pelem);  // 将列表成员还原为分区指针
    
    if (!strcmp(part_name, p->name)) {  // 找到了默认分区
        cur_part = p;
        struct disk* hd = p->my_disk;

        // 将超级块信息读入内存
        struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);  // 超级块缓冲区

        cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));  // 在内存中创建cur_part的超级块
        if (cur_part->sb == NULL) PANIC("memory allocation failed!!!!!!");
        memset(sb_buf, 0, SECTOR_SIZE);

        // 将超级块信息读入缓冲区
        ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);

        // 将超级块信息从缓冲区中复制到内存中的分区超级块，并且舍去了填充数组
        memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

        // 将硬盘中的块位图信息读入内存
        cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_secs * SECTOR_SIZE);
        if (cur_part->block_bitmap.bits == NULL) PANIC("memory allocation failed!!!!!!"); // 物理内存可能不够
        // 并不是真实的位图，最后一个扇区可能含有已经置1的无效位
        cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_secs * SECTOR_SIZE;
        ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_secs);  

        // 将硬盘中的i结点位图信息读入内存
        cur_part->inode_map.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_secs * SECTOR_SIZE);
        if (cur_part->inode_map.bits == NULL) PANIC("memory allocation failed!!!!!!");
        cur_part->inode_map.btmp_bytes_len = sb_buf->inode_bitmap_secs * SECTOR_SIZE;
        ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_map.bits, sb_buf->inode_bitmap_secs);

        list_init(&cur_part->open_inodes); // 初始化分区的已打开i结点列表

        printk("mount %s done!\n", p->name);

        return true;
    }
    return false;
}


// 文件系统初始化，如果没有就对分区进行格式化并创建文件系统
void filesys_init() {
    uint8_t channel_no = 0, dev_no = 0, partition_idx = 0;

    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    if (sb_buf == NULL) PANIC("memory allocation failed!!!!!!");

    printk("searching file system ......");

    while (channel_no < channel_cnt) {   // 遍历通道
        dev_no = 0;
        while (dev_no < 2) {             // 遍历硬盘设备
            if (dev_no == 0) {           // 跳过裸盘hd60.img
                dev_no++;
                continue;
            }
            struct disk* hd = &channels[channel_no].devices[dev_no];
            struct partition* p = hd->prim_parts;

            while (partition_idx < 12) {  // 最多只支持4个主分区+8个逻辑分区
                if (partition_idx == 4) { // 逻辑分区
                    p = hd->logic_parts;                    
                }

                /* channels数组为全局变量，默认为0，嵌套成员partition也为0
                 * 扫描分区表时会将分区信息写入p中 
                 *此处用于判断分区是否存在
                 */
                if (p->sec_cnt != 0) {     
                    memset(sb_buf, 0, SECTOR_SIZE);
                    ide_read(hd, p->start_lba + 1, sb_buf, 1);  // 将超级块信息读入到缓存区中

                    if (sb_buf->magic == 0x19590318) {          // 文件系统已经初始化完成了
                        printk("%s has filesystem\n", p->name);
                    } else {                                    // 初始化文件系统
                        printk("formatting %s's partition%s\n", hd->name, p->name);
                        partition_format(p);
                    }

                }
                partition_idx++;
                p++;  // 下一个分区
            }
            dev_no++;
        }
        channel_no++;
    }
    sys_free(sb_buf);

    char default_part[8] = "sdb1"; // 默认挂载到的分区
  
    list_traversal(&partition_list, partition_mount, (int)default_part);
}

