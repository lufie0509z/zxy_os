#include <lib/stdio.h>
#include <user/syscall.h>
#include <kernel/string.h>

int main(int argc, char** argv) {
    int arg_idx = 0;
    // argv[0]是本程序的名称，argv[1]是让程序去执行的可执行文件的路径
    while (arg_idx < argc) {
        printf("argv[%d] is %s\n", arg_idx, argv[arg_idx]);
        arg_idx++;
    }

    int pid = fork();
    if (pid) {
        int delay = 900000;
        while (delay--);
        printf("    I am father, my pid is %d\n", getpid());
        printf("    I will show process list\n");
        ps();
    } else {
        char abs_path[512] = {0};
        printf("    I am child, my pid is %d\n", getpid());
        printf("    I will exec %s right now\n", argv[1]);
        if (argv[1][0] != '/') {
            getcwd(abs_path, 512);
            strcat(abs_path, "/");
            strcat(abs_path, argv[1]);
            execv(argv[1], argv);
        } else execv(argv[1], argv);

    }
    while(1);
    return 0;
}
