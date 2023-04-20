#include <fs/fs.h>
#include <fs/file.h>
#include <user/pipe.h>
#include <device/ioqueue.h>
#include <kernel/global.h>
#include <lib/kernel/stdint.h>
#include <lib/kernel/stdio-kernel.h>

bool is_pipe(uint32_t local_fd) {
    uint32_t global_fd = fd_local_to_global(local_fd);
    return file_table[global_fd].fd_flag == PIPE_FLAG;
}

// 创建管道
int32_t sys_pipe(int32_t pipefd[2]) {
    int32_t global_fd = get_free_slot_in_global(); 
    file_table[global_fd].fd_inode = get_kernel_pages(1); // 申请一页内核空间作为环形缓冲区

    ioqueue_init((struct ioqueue*)file_table[global_fd].fd_inode);

    if (file_table[global_fd].fd_inode == NULL) return -1;

    file_table[global_fd].fd_flag = PIPE_FLAG; // 将 fd_flag 位复用为管道标识
    file_table[global_fd].fd_pos = 2;          // 将 fd_pos  位复用为管道的打开数

    pipefd[0] = pcb_fd_install(global_fd);  // 将文件描述符修改为管道文件结构在 file_table 中的下标
    pipefd[1] = pcb_fd_install(global_fd);
    // printk("pipefd %d %d global_fd %d\n", pipefd[0], pipefd[1], global_fd);
    return 0;
}

// 从文件描述符 fd 指向的管道中读取 cnt 个字节到缓冲区 buf 中
uint32_t pipe_read(int32_t local_fd, void* buf, uint32_t cnt) {
    char* buffer = buf;
    uint32_t bytes_read = 0;
    uint32_t global_fd = fd_local_to_global(local_fd);

    // 获取管道的环形缓冲区
    struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;

    // 选择较小的数据大小读取避免阻塞
    uint32_t ioq_len = ioq_length(ioq);
    uint32_t size = (ioq_len > cnt) ? cnt : ioq_len;

    while (bytes_read < size) {
        *buffer = ioq_get_char(ioq);
        bytes_read++;
        buffer++;
    }
    // printk("pipe_read %s\n", (char*)buf);
    return bytes_read;
}

// 从文件描述符 fd 指向的管道中写入 cnt 个字节到缓冲区 buf 中
uint32_t pipe_write(int32_t local_fd, const void* buf, uint32_t cnt) {
    const char* buffer = buf;
    uint32_t bytes_write = 0;
    uint32_t global_fd = fd_local_to_global(local_fd);

    // 获取管道的环形缓冲区
    struct ioqueue* ioq = (struct ioqueue*)file_table[global_fd].fd_inode;

    // 选择较小的数据大小读取避免阻塞
    uint32_t ioq_len = bufsize - ioq_length(ioq);
    uint32_t size = (ioq_len > cnt) ? cnt : ioq_len;

    while (bytes_write < size) {
        ioq_put_char(ioq, *buffer);
        buffer++;
        bytes_write++;
    }
    // printk("pipe_write %s\n", (char*)buf);
    return bytes_write;
}

// 文件描述符重定向
void sys_fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd) {
    struct task_struct* cur = running_thread();
    if (new_local_fd < 3) { // 预留的标准输入输出和错误的
        cur->fdtable[old_local_fd] = new_local_fd; 
    } 
    else {
        uint32_t new_global_fd = cur->fdtable[new_local_fd];
        cur->fdtable[old_local_fd] = new_global_fd;
    }
}