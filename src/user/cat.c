#include <lib/stdio.h>
#include <user/syscall.h>
#include <kernel/string.h>

int main(int argc, char** argv) {

    if (argc > 2) {
        printf("cat: only support 1 argument.\neg: cat filename\n");
        exit(-2);
    }

    if (argc == 1) { // 修改为从键盘获取数据
        char buf[512] = {0};
        read(0, buf, 512);
        printf("%s", buf);
        exit(0);
    }

    int buf_size = 1024;
    void* buf = malloc(buf_size);
    if (buf == NULL) {
        printf("cat: malloc memory failed\n");
        return -1;
    }

    char abs_path[512] = {0}; // 绝对路径

    if (argv[1][0] != '/') {
        getcwd(abs_path, 512);
        strcat(abs_path, '/');
        strcat(abs_path, argv[1]);
    } else {
        strcpy(abs_path, argv[1]);
    }
    // printf("abs path %s\n", abs_path);
    int fd = open(abs_path, O_RDONLY);
    // printf("open fd %d\n", fd);
    if (fd == -1) {
        printf("cat: open: open %s failed\n", argv[1]);
        return -1;
    }

    int read_bytes = 0;
    while(1) {
       
        read_bytes = read(fd, buf, buf_size);
        if (read_bytes == -1) {
            break;
        }
        write(1, buf, read_bytes);
    }

    free(buf);
    close(fd);
    return 66;
}
