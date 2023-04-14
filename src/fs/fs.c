#include <fs/fs.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <fs/inode.h>
#include <fs/super_block.h>
#include <device/ide.h>
#include <device/console.h>
#include <device/ioqueue.h>
#include <device/keyboard.h>
#include <kernel/list.h>
#include <kernel/debug.h>
#include <kernel/global.h>
#include <kernel/string.h>
#include <kernel/memory.h>
#include <kernel/thread.h>
#include <kernel/interrupt.h>
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

   memcpy(d->filename, "..", 2);  // 初始化上一级目录..
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



// 目录路径解析，name_store存储最上层路径，返回除最上层外的剩余子路径
char* path_parse(char* pathname, char* name_store) {
    if (pathname[0] = '/') {  // 根目录不需要单独解析
        // 跳过连续的多个'/'，如 “///a/b”
        while (*(++pathname) == '/');
    }

    // 开始解析目录路径
    while (*pathname != '/' && *pathname != 0) {
        *name_store++ = *pathname++;
    }

    if (pathname[0] == 0) return NULL; // 指向末尾的结束字符'\0'

    return pathname;
}

// 返回路径深度
int32_t path_depth_cnt (char* pathname) {
    ASSERT(pathname != NULL) 
    char* p = pathname;
    char name[MAX_FILE_NAME_LEN];
    uint32_t depth = 0;

    p = path_parse(p, name);

    while (name[0]) {
        depth++;
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (p) p = path_parse(p, name);
    }
    return depth;
}

/* 搜索文件，文件路径是pathname，找到返回i结点编号，否则返回-1
 * path_search_record由主调函数提供，主调函数只关注该结构体信息 */
static int32_t search_file(const char* pathname, struct path_search_record* searched_record) {
    
    // 如果路径仅是根目录，不做查找直接返回
    if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")) {
        searched_record->parent_dir = &root_dir;
        searched_record->f_type = FT_DIR;
        searched_record->searched_path[0] = 0; // 搜索路径置空
        return 0;
    }

    uint32_t pathlen = strlen(pathname);
    ASSERT(pathname[0] == '/' && pathlen >= 1 && pathlen < MAX_PATH_LEN);

    char* sub_path = (char*)pathname;         // 目录解析过程中用来存储除最外层路径外的剩余目录
    struct dir* parent_dir = &root_dir;  
    struct dir_entry dir_e;                    // 存储根据name寻找到的目录项

    char name[MAX_FILE_NAME_LEN] = {0};        // 存储解析出来的各级路径名称
 
    searched_record->parent_dir = parent_dir;
    searched_record->f_type = FT_UNKOWN;
    uint32_t parent_dir_inode = 0;             // 已解析出来的路径父目录i结点号 
    
    /* /a/b/c 
     * name = "/a/b"
     * parent_dir_inode = a的i结点编号
     * parent_dir = b的dir结构体 */

    sub_path = path_parse(pathname, name);

    while (name[0]) {
        ASSERT(strlen(searched_record->searched_path) < 512);

        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name); // 记录下已经搜寻过的路径

        if(search_dir_entry(cur_part, parent_dir, name, &dir_e)) {

            memset(name, 0, MAX_FILE_NAME_LEN);
            if (sub_path) sub_path = path_parse(sub_path, name);

            if (dir_e.f_type == FT_DIR) {

                parent_dir_inode = parent_dir->inode->i_no;
                dir_close(parent_dir);
                parent_dir = dir_open(cur_part, dir_e.i_no);
                searched_record->parent_dir = parent_dir;
                continue;

            } else if (dir_e.f_type == FT_REGULAR) {

                searched_record->f_type = FT_REGULAR;
                return dir_e.i_no;
                // 主调函数会根据searched_path判断是否搜寻完成了，这里直接返回即可
                
            }
        } else return -1; // 查找失败
    }

    /* 只有当
     * 1 路径已经完整解析完成，且各级都存在
     * 2 路径的最后一层不是普通文件，而是目录
     * 才会执行到这里 */
    dir_close(searched_record->parent_dir);

    /* path_search_record.parent_dir 由主调函数负责关闭
     * 主调函数有可能会用到此目录如在该目录下创建文件 */
    searched_record->parent_dir = dir_open(cur_part, parent_dir_inode);  // 被查找目标的直接父目录
    searched_record->f_type = FT_DIR;
    return dir_e.i_no;

}

// 打开文件，成功返回文件描述符，否则返回-1
int32_t sys_open(const char* pathname, uint8_t flags) {
    if (pathname[strlen(pathname) - 1]  == '/') { // 不能打开目录比如/a/b/
        printk("can not open a directory %s\n", pathname);
        return -1;
    }

    ASSERT(flags <= 7);
    int32_t fd = -1;

    struct path_search_record searched_record;
    // 栈中内存不会自动清零
    memset(&searched_record, 0, sizeof(struct path_search_record));

    uint32_t pathname_depth = path_depth_cnt((char*)pathname);

    // 检查文件是否存在
    int32_t inode_no = search_file((char*)pathname, &searched_record);
    // printk("%d\n", inode_no);
    // printk("%s\n", &searched_record.searched_path);
    // printk("%d\n", searched_record.parent_dir->inode->i_size / sizeof(struct dir_entry));

    bool found = inode_no != -1 ? true : false;

    if (searched_record.f_type == FT_DIR) {
        printk("can not open a direcotry with open(), use opendir() to instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    uint32_t searched_path_depth = path_depth_cnt(searched_record.searched_path);

    if (pathname_depth != searched_path_depth) { // 说明中间某个目录就找不到
        printk("can not access %s: Not a directory, subpath %s is't exist\n",
                pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);   // parent_dir由主调函数关闭
        return -1;
    }

    // 若是在最后一个路径上没找到,并且并不是要创建文件,直接返回-1
    if (!found && !(flags & O_CREATE)) {
        printk("in path %s, file %s is't exist\n", searched_record.searched_path,
               (strrchr(searched_record.searched_path, '/') + 1));
        /* 
         * char *strrchr(const char *str, int c)
         * 搜索str所指向的字符串中搜索最后一次出现字符c的位置
         * 最终会转换回 char 指针形式返回
         * strrchr("/a/b/c", '/') = "c"
         */
        dir_close(searched_record.parent_dir);
        return -1;
    } else if (found && flags & O_CREATE) { // 需要创建的文件已存在
        printk("%s has already exist!\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    switch (flags & O_CREATE) {
        case O_CREATE:
            printk("creating file\n");
            fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1),flags);
            dir_close(searched_record.parent_dir);
            break;
        // 剩下是打开文件相关
        default:
            fd = file_open(inode_no, flags);

    }

    // fd是任务pcb->fd_table数组中的元素下标
    return fd;
}

// 输入pcb中的文件描述符， 返回全局文件表的下标
static uint32_t fd_local_to_global(uint32_t local_fd) {
   struct task_struct* cur = running_thread();
   int32_t global_fd = cur->fdtable[local_fd];
//    printk("fd_local_to_global %d %d\n", local_fd, global_fd);

   ASSERT(global_fd >= 0 && global_fd < MAX_FILES_OPEN);
   return (uint32_t)global_fd;
}

// 关闭文件文件描述符指向的文件
int32_t sys_close(int32_t fd) {
   int32_t ret = -1; // 默认关闭失败
   if (fd > 2) {
      uint32_t global_fd = fd_local_to_global(fd);
      ret = file_close(&file_table[global_fd]);
      running_thread()->fdtable[fd] = -1;  // 使该文件描述符位可用
      printk("fd: %d is closing\n", fd);
   }
   return ret;
}

// 往文件描述符所在文件写入cnt个字节
int32_t sys_write(int32_t fd, const void* buf, uint32_t cnt) {
    if (fd == std_out) { // 标准输出
        char tmp[1024] = {0};
        memcpy(tmp, buf, cnt);
        console_put_str(tmp);
        return cnt;
    }

    uint32_t global_fd = fd_local_to_global(fd); // 文件表中的下标

    struct file* f = &file_table[global_fd];

    if (f->fd_flag & O_WRONLY || f->fd_flag & O_RDWR) {
        return file_write(f, buf, cnt);
    } else {
        console_put_str("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n");
        return -1;
    }
}


int32_t sys_read(int32_t fd, void* buf, uint32_t cnt) {
    ASSERT(fd >= 0 && fd < MAX_FILES_OPEN_PER_PROC);
    ASSERT(buf != NULL);
    int32_t ret = -1;

    if (fd < 0 || fd == std_out || fd == std_err) printk("sys_read:fd error\n");
    else if (fd == stdin_no) { // 从键盘获取输入
        char* buffer = buf;
        uint32_t bytes_read = 0;
        while (bytes_read < cnt) {
            *buffer = ioq_get_char(&kbd_buf);
            bytes_read++;
            buffer++;
        }
        ret = bytes_read == 0 ? -1 : (int32_t)bytes_read;
    } else {
        uint32_t global_fd = fd_local_to_global(fd);
        // printk("ret:%d cnt:%d\n", ret, cnt);
        ret = file_read(&file_table[global_fd], buf, cnt);
        // printk("ret:%d cnt:%d\n", ret, cnt);
    }
 
    return ret;
}


// 重置文件读写操作的偏移指针，错误时返回-1
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence) {
    ASSERT(fd >= 0 && fd < MAX_FILES_OPEN_PER_PROC);
    ASSERT(whence > 0 && whence < 4);

    uint32_t global_fd = fd_local_to_global(fd);
    struct file* f = &file_table[global_fd];
    
    int32_t new_pos;
    int32_t file_size = f->fd_inode->i_size;

    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = offset + (int32_t)f->fd_pos;
            break;
        case SEEK_END:
            new_pos = file_size + offset;
            break;
    }

    if (new_pos < 0 || new_pos > (file_size - 1)) return -1;

    f->fd_pos = new_pos;
    return f->fd_pos;
    
}

// 删除绝对路径是pathname的文件
int32_t sys_unlink(const char* pathname) {
    ASSERT(strlen(pathname) < MAX_PATH_LEN);

    // 检查待删除的文件是否存在
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int32_t i_no = search_file(pathname, &searched_record);
    ASSERT(i_no != 0);
    if (i_no == -1) {
        printk("file: %s not found!\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    if (searched_record.f_type == FT_DIR) {
        printk("can't delete d directory with unlink(), please use rmdir() instead.\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // 检查待删除的文件是不是在已打开文件列表（文件表file_table）中
    uint32_t fd = 0;
    while (fd < MAX_FILES_OPEN) {
        if (file_table[fd].fd_inode != NULL && (uint32_t)i_no == file_table[fd].fd_inode->i_no)
            break;
        fd++;
    }
    if (fd < MAX_FILES_OPEN) {
        printk("file %s is in use, not allow to delete\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }
    ASSERT(fd == MAX_FILES_OPEN);

    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf == NULL) {
        printk("sys_unlink failed: malloc for io_buf failed\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    struct dir* parent_dir = searched_record.parent_dir;
    delete_dir_entry(cur_part, parent_dir, i_no, io_buf);
    inode_release(cur_part, i_no);
    sys_free(io_buf);
    dir_close(searched_record.parent_dir);
    return 0;
}

// 创建目录
int32_t sys_mkdir(const char* pathname) {
    uint8_t rollback_step = 0; // 用于操作失败时确定资源回滚的步骤
    void* io_buf = sys_malloc(1024);
    if (io_buf == NULL) {
        printk("sysmkdir(): sys_malloc for io_buf failed\n");
        return -1;
    }

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int32_t i_no = -1;
    i_no = search_file(pathname, &searched_record);
    if (i_no != -1) { // 找到了同名的文件或者目录
        printk("sysmkdir(): file or directory %s exist!\n", pathname);
        rollback_step = 1;
        goto rollback;
    } else {
        // 判断是不存在该文件还是中间目录不存在
        uint32_t pathname_depth = path_depth_cnt((char*)pathname);
        uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
        if (pathname_depth != path_searched_depth) {
            // 中间目录不存在
            printk("sysmkdir(): subpath %s not exist\n", searched_record.searched_path);
            rollback_step = 1;
            goto rollback;
        }
    }

    struct dir* parent_dir = searched_record.parent_dir;

    // pathname目录名称后可能会有字符'/'
    char* dirname = strrchr(searched_record.searched_path, '/') + 1;
    
    // 为新目录创建i结点
    i_no = inode_bitmap_alloc(cur_part);
    if (i_no == -1) {
        printk("sysmkdir(): allocate for inode failed\n");
        rollback_step = 1;
        goto rollback;
    }

    struct inode new_dir_inode;      
    inode_init(i_no, &new_dir_inode); // 初始化新目录的i结点
 
    uint32_t block_bitmap_idx = 0;    // 用来记录数据块对应于block_bitmap中的索引
    int32_t block_lba = -1;
    block_lba = block_bitmap_alloc(cur_part);
    if (block_lba == -1) {
        printk("sysmkdir(): block_bitmap allocate for create directory failed\n");
        rollback_step = 2;
        goto rollback;
    }

    new_dir_inode.i_sectors[0] = block_lba;

    // 分配块后将块位图同步到磁盘中
    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
    ASSERT(block_bitmap_idx != 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
 
    // 将 "." 和 "." 写入新的目录
    memset(io_buf, 0, 1024);
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;

    memcpy(dir_e->filename, ".", 1);
    dir_e->i_no = i_no;
    dir_e->f_type = FT_DIR;

    dir_e++;

    memcpy(dir_e->filename, "..", 2);
    dir_e->i_no = parent_dir->inode->i_no;
    dir_e->f_type = FT_DIR;

    ide_write(cur_part->my_disk, new_dir_inode.i_sectors[0], io_buf, 1);
    new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;

    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));
    create_dir_entry(dirname, i_no, FT_DIR, &new_dir_entry); // 初始化new_dir_entry目录项
    // 在父目录中添加新创建目录的目录项
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) { 
        printk("sys_mkdir(): sync_dir_entry to disk failed!\n");
        rollback_step = 2;
    }

    // 将父目录的i结点同步到硬盘
    memset(io_buf, 0, 1024);
    inode_sync(cur_part, parent_dir->inode, io_buf);
    
    // 将新创建目录的i结点同步到硬盘
    memset(io_buf, 0, 1024);
    inode_sync(cur_part, &new_dir_inode, io_buf);

    // 将i结点位图同步到磁盘
    bitmap_sync(cur_part, i_no, INODE_BITMAP);

    sys_free(io_buf);
    dir_close(searched_record.parent_dir); // 关闭所创建目录的父目录
    
    return 0;

    rollback:
        switch (rollback_step) {
            case 2:
                bitmap_set(&cur_part->inode_map, i_no, 0);
            case 1:
                dir_close(searched_record.parent_dir);
                break;
        }
        sys_free(io_buf);
        return -1;
}

// 打开目录，成功则返回其目录指针
struct dir* sys_opendir(const char* name) {
    ASSERT(strlen(name) < MAX_FILE_NAME_LEN);
    // 根目录直接返回
    if (name[0] == '/' && (name[1] == 0 || name[1] == '.')) return &root_dir;

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int32_t i_no = search_file(name, &searched_record); 
    struct dir* ret = NULL;
    if (i_no == -1) {
        printk("in %s, sub path %s not exist\n", name, searched_record.searched_path);
    } else {
        if (searched_record.f_type == FT_REGULAR) {
            printk("%s is regular file\n", name);
        } else if (searched_record.f_type == FT_DIR) {
            ret = dir_open(cur_part, i_no);
        }
    }
    dir_close(searched_record.parent_dir);
    return ret;
}

// 关闭目录
int32_t sys_closedir(struct dir* dir) {
    if (dir != NULL) {
        dir_close(dir);
        return 0;
    }
    return -1;
}

// 读取目录dir的一个目录项，成功返回其地址，读到目录尾或错误时返回NULL
struct dir_entry* sys_readdir(struct dir* dir) {
    ASSERT(dir != NULL);
    return dir_read(dir);
}

// 目录回绕，将目录dir的dir_pose值置0
void sys_rewinddir(struct dir* dir) {
    dir->dir_pose = 0;
}

// 删除空目录
int32_t sys_rmdir(const char* pathname) {
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int32_t inode_no = search_file(pathname, &searched_record);
    ASSERT(inode_no != 0);
    int32_t ret = -1;
    if (inode_no == -1) {
        printk("in %s, sub path %s not exist\n", pathname, searched_record.searched_path);
    } else {
        if (searched_record.f_type == FT_REGULAR) {
            printk("%s is regular file\n", pathname);
        } else {
            struct dir* dir = dir_open(cur_part, inode_no);
            if (!dir_is_empty(dir)) {
                printk("dir %s is not empty, it is not allowed to delete a nonempty directory!\n", pathname);
            } else {
                if (!dir_remove(searched_record.parent_dir, dir)) ret = 0;
            } 
            dir_close(dir);
        }
    }
    dir_close(searched_record.parent_dir);
    return ret;
}


// 获取父目录的i结点编号
static uint32_t get_parent_dir_i_no (uint32_t child_i_no, void* io_buf) {

    struct inode* child_dir_inode = inode_open(cur_part, child_i_no); // 子目录

    uint32_t block_lba = child_dir_inode->i_sectors[0];
    ASSERT(block_lba >= cur_part->sb->data_start_lba);

    inode_close(child_dir_inode);

    ide_read(cur_part->my_disk, block_lba, io_buf, 1);

    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    ASSERT(dir_e[1].i_no < 4096 && dir_e[1].f_type == FT_DIR);
    return dir_e[1].i_no; 
}

/* 在i结点号为parent_i_no的父目录中找到子结点号为child_i_no的子目录名字
 * 该函数每次只获取一层目录的名称
 * 传入的参数 path 用于拼接完整的绝对路径
 * 每次调用前，path 是非空的，里面已存储了部分路径到名称了 */
static int32_t get_child_dir_name(uint32_t parent_i_no, uint32_t child_i_no, char* path, void* io_buf) {
    struct inode* parent_dir_inode = inode_open(cur_part, parent_i_no);

    uint8_t block_idx = 0;
    uint32_t all_blocks[140] = {0};
    uint32_t block_cnt = 12;
    while (block_idx < 12) {
        all_blocks[block_idx] = parent_dir_inode->i_sectors[block_idx];
        block_idx++;
    }
    if (parent_dir_inode->i_sectors[12] != 0) {
        ide_read(cur_part->my_disk, parent_dir_inode->i_sectors[12], all_blocks + 12, 1);
        block_cnt = 140;
    }

    inode_close(parent_dir_inode);

    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
    uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);

    block_idx = 0;
    while (block_idx < block_cnt) {
        if (all_blocks[block_idx]) {
            ide_read(cur_part->my_disk, all_blocks[block_idx], io_buf, 1);
            uint8_t dir_entry_idx = 0;
            while (dir_entry_idx < dir_entrys_per_sec) {
                if ((dir_e + dir_entry_idx)->i_no == child_i_no) {
                    // 拼接路径名
                    strcat(path, "/");
                    strcat(path, (dir_e + dir_entry_idx)->filename);
                    return 0;
                }
                dir_entry_idx++;
            }
        }
        block_idx++;
    }
    return -1;
}


// 将当前工作目录的绝对路径写入buf，size是buf的大小
char* sys_getcwd(char* buf, uint32_t size) {
   // 当用户进程提供的buf为空的时候，会在系统调用getcwd中用malloc分配内存 
    ASSERT(buf != NULL);
    void* io_buf = sys_malloc(SECTOR_SIZE);
    if (io_buf == NULL) {
        return NULL;
    }

    struct task_struct* cur_thread = running_thread();
    int32_t parent_i_no = 0;
    int32_t child_i_no = cur_thread->cwd_inode_nr;
    ASSERT(child_i_no >= 0 && child_i_no < 4096); 
    
    // 根目录
    if (child_i_no == 0) {
        buf[0] = '/';
        buf[1] = 0;
        sys_free(io_buf);
        return buf;
    }

    memset(buf, 0, size);
    char full_path_reverse[MAX_PATH_LEN] = {0}; // 反转的绝对路径

    // 从下往上逐层找父目录,直到找到根目录为止.
    while (child_i_no) {
        parent_i_no = get_parent_dir_i_no(child_i_no, io_buf);
        if (get_child_dir_name(parent_i_no, child_i_no, full_path_reverse, io_buf) == -1) {
            sys_free(io_buf);
            return NULL;
        }
        child_i_no = parent_i_no;
    }
    ASSERT(strlen(full_path_reverse) <= size);

    /* full_path_reverse中的路径是反着的
     * 子目录在前(左)，父目录在后(右)
     * 反转目录顺序，目录名本身不反转
     * 如若原路径为“/ab/c”，在 full_path_reverse 的将是“/c/ab” */
    char* last_slash;	// 用于记录字符串中最后一个斜杠地址
    while ((last_slash = strrchr(full_path_reverse, '/'))) {
        uint16_t len = strlen(buf);
        strcpy(buf + len, last_slash);
        // 在full_path_reverse中添加结束字符，作为下一次执行strcpy中last_slash的边界
        *last_slash = 0;
    }
    sys_free(io_buf);
    return buf;
}

// 更改当前工作目录为绝对路径pathname，核心原理就是修改 cwd_inode_nr
int32_t sys_chdir(const char* pathname) {
    int32_t ret = -1;
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int32_t i_no = search_file(pathname, &searched_record);
    if (i_no != -1) {
        if (searched_record.f_type == FT_DIR) {
            running_thread()->cwd_inode_nr = i_no;
            ret = 0;
        } else  printk("sys_chdir: %s is regular file or other!\n", pathname); 
    }
    dir_close(searched_record.parent_dir);
    return ret;
}

// 将文件的属性相关信息填入buf
int32_t sys_stat(const char* path, struct stat* buf) {
    // 根目录
    // printk("path%s\n", path);
    if (!strcmp(path, "/") || !strcmp(path, "/.") || ! strcmp(path, "/..")) {
        buf->st_ino = 0;
        buf->st_size = root_dir.inode->i_size;
        buf->st_filetype = FT_DIR;
        return 0;
    }

    int32_t ret = -1;
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int32_t i_no = search_file(path, &searched_record);
    
    if (i_no != -1) {
        struct inode* inode = inode_open(cur_part, i_no);
        buf->st_size = inode->i_size;
        buf->st_ino = i_no;
        buf->st_filetype = searched_record.f_type;
        inode_close(inode);
        ret = 0;
    }else printk("sys_stat: %s not found\n", path);
    dir_close(searched_record.parent_dir);
    return ret;
}

void sys_putchar(char char_asci) {
   console_put_char(char_asci);
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
                 *此处用于判断分区是否存在 */
                if (p->sec_cnt != 0) {     
                    memset(sb_buf, 0, SECTOR_SIZE);
                    ide_read(hd, p->start_lba + 1, sb_buf, 1);  // 将超级块信息读入到缓存区中

                    // if (sb_buf->magic == 0x19590318) {          // 文件系统已经初始化完成了
                    //     printk("%s has filesystem\n", p->name);
                    // } else {                                    // 初始化文件系统
                        printk("formatting %s's partition%s\n", hd->name, p->name);
                        partition_format(p);
                    // }

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

    // 打开根目录
    open_root_dir(cur_part);

    // 初始化文件描述符表
    uint32_t fd_idx = 0; 
    while (fd_idx++ < MAX_FILES_OPEN) {
        file_table[fd_idx].fd_inode = NULL;
    }
}

