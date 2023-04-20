#include <lib/stdio.h>
#include <user/syscall.h>
#include <kernel/string.h>

int main(int argc, char** argv) {
    int32_t fd[2] = {-1};
    pipe(fd); // 创建管道
    int32_t pid = fork();
    if (pid) { // 父进程
        close(fd[0]); // 关闭读取数据
        write(fd[1], "Hi son, this is father", 23);
        printf("This is father, my pid is %d\n", getpid());
        return 8;
    } else {
        close(fd[1]); // 关闭子进程的写入数据
        char buf[24] = {0};
        read(fd[0], buf, 24);
        printf("This is son, my pid is %d\n", getpid());
        printf("My father said to me: \"%s\"\n", buf);
        return 9;
    }
}