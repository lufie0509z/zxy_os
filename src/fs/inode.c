#include <fs/fs.h>
#include <fs/file.h>
#include <fs/inode.h>
#include <fs/super_block.h>
#include <device/ide.h>
#include <kernel/list.h>
#include <kernel/debug.h>
#include <kernel/global.h>
#include <kernel/string.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/interrupt.h>
#include <lib/kernel/stdio-kernel.h>

// 存储i结点在磁盘扇区中的位置
struct inode_pos {
    bool two_secs;      // 是否跨过两个扇区
    uint32_t sec_lba;   // i结点所在扇区的lba
    uint32_t off_size;  // i结点在扇区中的偏移
};

// 获取i结点所在扇区地址和在扇区内的偏移
static void locate_inode(struct partition* p, uint32_t i_no, struct inode_pos* i_pos) {
    ASSERT(i_no < 4096);

    uint32_t inode_table_lba = p->sb->inode_table_lba;

    uint32_t inode_size = sizeof(struct inode);
    uint32_t off_size = i_no * inode_size;      // i结点相当于i结点表的字节偏移
    uint32_t off_sec = off_size / 512;          // i结点所在扇区相当于i结点表的扇区偏移
    uint32_t off_size_in_sec = off_size % 512;  // i结点在扇区内的偏移
 
    uint32_t left_in_sec = 512 - off_size_in_sec;

    // 判断i结点是不是跨扇区
    if (left_in_sec < inode_size) i_pos->two_secs = true;
    else i_pos->two_secs = false;

    i_pos->sec_lba = off_sec + inode_table_lba;
    i_pos->off_size = off_size_in_sec;

}

// 将inode信息同步到分区中i结点到位置
void inode_sync(struct partition* p, struct inode* inode, void* io_buf) {
    uint8_t i_no = inode->i_no;
    struct inode_pos inode_pos;

    locate_inode(p, i_no, &inode_pos); // 将i结点的位置信息读入结构体中
    ASSERT(inode_pos.sec_lba <= (p->start_lba + p->sec_cnt));

    // 磁盘中的i结点不需要i_open_cnt和inode_tag等信息，将这些信息清空后再写入磁盘
    struct inode pure_inde;
    memcpy(&pure_inde, inode, sizeof(struct inode));

    pure_inde.i_open_cnt = 0;               // 在内存中被打开的次数
    pure_inde.write_deny = false;           // 保证下次硬盘读出时可写
    pure_inde.inode_tag.prev = pure_inde.inode_tag.next = NULL; // 用于加入已打开i结点等列表

    // 硬盘读写是以扇区为单位的，读出i结点所在硬盘后仅修改i结点信息，再写入硬盘中
    char* inode_buf = (char*)io_buf;        // 此缓冲区用于拼接同步的i结点数据

    if (inode_pos.two_secs) {
       ide_read(p->my_disk, inode_pos.sec_lba, inode_buf, 2);
       memcpy((inode_buf + inode_pos.off_size), &pure_inde, sizeof(struct inode)); // 修改i结点相关的信息
       ide_write(p->my_disk, inode_pos.sec_lba, inode_buf, 2);
    } else {
       ide_read(p->my_disk, inode_pos.sec_lba, inode_buf, 1);
       memcpy((inode_buf + inode_pos.off_size), &pure_inde, sizeof(struct inode));
       ide_write(p->my_disk, inode_pos.sec_lba, inode_buf, 1);
   }
}

// 根据i结点编号找到i结点
struct inode* inode_open(struct partition* p, uint32_t i_no)
{
    // 现在缓冲区（已打开i结点链表）中寻找
    struct list_elem* elem = p->open_inodes.head.next;
    struct inode* inode_found;
    while (elem != &p->open_inodes.tail) {
       inode_found = elem2entry(struct inode, inode_tag, elem);
       if (inode_found->i_no == i_no) {
          inode_found->i_open_cnt++;
          return inode_found;
       }
       elem = elem->next;
    }

    // 从硬盘中读入该i结点并加入已打开i结点的链表中
    struct inode_pos i_pos;
    locate_inode(p, i_no, &i_pos); // 获取该i结点在磁盘上的位置

    // inode队列中的结点应该被所有任务共享，需在内核的堆空间中创建
    struct task_struct* cur = running_thread();
    uint32_t* cur_pgdir_tmp = cur->pgdir;
    
    // sys_malloc中判断如果页表为空会在内核空间中进行分配，并没有真正地修改页表
    cur->pgdir = NULL;
    inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
    cur->pgdir = cur_pgdir_tmp;

    char* inode_buf;

    // 将i结点信息从磁盘中读入缓冲区
    if (i_pos.two_secs) {
       inode_buf = (char*)sys_malloc(1024);
       ide_read(p->my_disk, i_pos.sec_lba, inode_buf, 2);
    } else {
       inode_buf = (char*)sys_malloc(512);
       ide_read(p->my_disk, i_pos.sec_lba, inode_buf, 1);
    }

    // 将i结点信息从缓冲区中（以扇区为单位）复制到i结点结构体中
    memcpy(inode_found, inode_buf + i_pos.off_size, sizeof(struct inode));
 
    // 根据程序局部性原理，加入i结点队头
    list_push(&p->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnt = 1;

    sys_free(inode_buf);

    return inode_found;
}



// 减少i结点打开次数，如果为0则关闭
void inode_close(struct inode* inode) {
    enum intr_status old_status = intr_disable();
    inode->i_open_cnt--;
    // 该结点对应的文件不再被任何进程使用，则将该i结点从队列中去除并且释放内存空间
    if (inode->i_open_cnt == 0) {
       list_remove(&inode->inode_tag);

       // 该i结点的内存空间是在内核的堆空间中申请的
       struct task_struct* cur = running_thread();
       uint32_t* cur_pgdir_tmp = cur->pgdir;
       cur->pgdir = NULL;
       sys_free(inode);
       cur->pgdir = cur_pgdir_tmp;
    }
    intr_set_status(old_status);
}


// 将硬盘上i结点的数据清空
void inode_delete(struct partition* p, uint32_t i_no, void* io_buf) {
   ASSERT(i_no < 4096);
   struct inode_pos i_pos;

   locate_inode(cur_part, i_no, &i_pos);

   ASSERT(i_pos.sec_lba <= p->start_lba + p->sec_cnt);

   char* inode_buf = (char*)io_buf;
   if (i_pos.two_secs) {
      ide_read(cur_part->my_disk, i_pos.sec_lba, inode_buf, 2);
      memset(inode_buf + i_pos.off_size, 0, sizeof(struct inode));
      ide_write(cur_part->my_disk, i_pos.sec_lba, inode_buf, 2);
   } else {
      ide_read(cur_part->my_disk, i_pos.sec_lba, inode_buf, 1);
      memset(inode_buf + i_pos.off_size, 0, sizeof(struct inode));
      ide_write(cur_part->my_disk, i_pos.sec_lba, inode_buf, 1);
   }
}

// 回收i结点，包括它本身和占用了的数据块
void inode_release(struct partition* p, uint32_t i_no) {
   struct inode* inode_to_del = inode_open(p, i_no);
   ASSERT(inode_to_del->i_no == i_no);

   // 回收i结点占用的数据块
   uint8_t block_idx = 0, block_cnt = 12;
   uint32_t block_bitmap_idx;
   uint32_t all_blocks[140] = {0};

   while (block_idx < 12) {
      all_blocks[block_idx] = inode_to_del->i_sectors[block_idx];
      block_idx++;
   }

   if (inode_to_del->i_sectors[12] != 0) {
      // 一级块存在，将间接块地址收集到all_blocks数组中
      ide_read(p->my_disk, inode_to_del->i_sectors[12], all_blocks + 12, 1);
      block_cnt = 140;

      // 释放一级间接索引表本身的扇区地址
      block_bitmap_idx = inode_to_del->i_sectors[12] - p->sb->data_start_lba;
      ASSERT (block_bitmap_idx > 0);
      bitmap_set(&p->block_bitmap, block_bitmap_idx, 0);
      bitmap_sync(p, block_bitmap_idx, BLOCK_BITMAP);
   }

   /* 回收数据块占用的扇区
    * 普通文件数据是连续存储的不存在中间某个地址为空的情况
    * 目录文件会存在中间某个地址为空的情况 */
   block_idx = 0;
   while (block_idx < 139) {
      if (all_blocks[block_idx] != 0) {
         block_bitmap_idx = all_blocks[block_idx] - p->sb->data_start_lba;
         ASSERT(block_bitmap_idx > 0);
         bitmap_set(&p->block_bitmap, block_bitmap_idx, 0);
         bitmap_sync(p, block_bitmap_idx, BLOCK_BITMAP);
      }
      block_idx++;
   }

   // 回收占用的inode数据本身
   bitmap_set(&p->inode_map, i_no, 0);

   bitmap_sync(p, i_no, INODE_BITMAP);

   // 硬盘数据并不需要清0，下次使用时直接覆盖即可
   void* io_buf = sys_malloc(1024);
   inode_delete(p, i_no, io_buf);
   sys_free(io_buf);

   inode_close(inode_to_del);

}

// 初始化i结点
void inode_init(uint32_t i_no, struct inode* new_inode) {
    new_inode->i_no = i_no;
    new_inode->i_open_cnt = 0;
    new_inode->i_size = 0;
    new_inode->write_deny = false;

    uint8_t sec_idx = 0;
    // 文件/i结点被创建的时候并不用分配扇区，当写文件时才真正分配扇区
    while (sec_idx < 13) {
       new_inode->i_sectors[sec_idx] = 0;
       sec_idx++;
    }
}