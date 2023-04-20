
#include <kernel/debug.h>
#include <kernel/string.h>
#include <kernel/global.h>
#include <user/assert.h>
#include <user/syscall.h>
#include <user/buildin_cmd.h>
#include <user/wait_exit.h>
#include <lib/stdio.h>
#include <lib/kernel/stdint.h>
#include <fs/fs.h>
#include <fs/file.h>


#define cmd_len    128  // 命令行参数的最多有128个字符
#define MAX_ARG_NR 16   // 包括命令名最多支持的参数个数

char final_path[MAX_PATH_LEN] = {0};      // 全局的，存储路径清晰转化后的结果

// 存储输入的命令
static char cmd_line[cmd_len] = {0};

// 用来记录当前目录，执行cd后会更新
char cwd_cache[64] = {0};

// 输出提示符
void print_prompt() {
    printf("[zzzzzxy@localhost %s]$ ", cwd_cache);
}

// 从键盘缓冲区中读入cnt个字节到buf
static void readline(char* buf, int32_t cnt) {
    ASSERT(buf != NULL && cnt > 0);
    char* pos = buf;
    while (read(stdin_no, pos, 1) != -1 && (pos - buf) < cnt) {
        // 直到读到回车键后返回
        switch(*pos) {
            case '\n':     // 换行符（回车）
            case '\r':
                *pos = 0;  // 回车键，添加终止字符
                putchar('\n');
                return;
            case '\b':  // 退格符
                if (buf[0] != '\b') { // 阻止删除非本次删除的信息
                    pos--;
                    putchar('\b');
                }
                break;

            case 'l' - 'a':  // ctrl+l 清屏
                *pos = 0;
                clear();
                print_prompt();
                printf("%s", buf); // 将之前输入的命令打印出来
                break;
            case 'u' - 'a': // ctrl+u 清除输入
                while (buf != pos) {
                    putchar('\b');
                    *(pos--) = 0;
                }
                break;

            default:    // 非控制键
                putchar(*pos);
                pos++;
        }

    }

    printf("readline: can not find enter_key in the cmd_line, max num of char is 128\n");

}

// 遍历字符串，将cmd_str中以token为分隔符的单词的指针存入数组argv中
static int32_t cmd_parse(char* cmd_str, char** argv, char token) {
    ASSERT(cmd_str != NULL);
    int32_t arg_idx = 0;
    while (arg_idx < MAX_ARG_NR) {
        argv[arg_idx] = NULL;
        arg_idx++;
    }

    char* next = cmd_str;
    int32_t argc = 0;

    while (*next) {
        while (*next == token) next++;

        if (*next == 0) break; // 最后一个参数后有空格，如‘ls dir ’

        argv[argc] = next;    // 每找出一个字符串就在将它在cmd_str中的起始next存入数组中

        while (*next && *next != token) next++;

        // 如果cmd_str未结束，人为添加结束字符
        if (*next) *next++ = 0;

        if (argc > MAX_ARG_NR) return -1;

        argc++;
    }
    return argc;
}


static void cmd_execute(uint32_t argc, char** argv) {
    if (!strcmp(argv[0], "ls")) {
        buildin_ls(argc, argv);
    } 
    else if (!strcmp(argv[0], "cd")) {
        
        if (buildin_cd(argc, argv) != NULL) { // 更新当前目录
            memset(cwd_cache, 0, MAX_PATH_LEN);
            strcpy(cwd_cache, final_path);
        }
    } else if (!strcmp(argv[0], "pwd")) buildin_pwd(argc, argv);
    else if (!strcmp(argv[0], "ps"))    buildin_ps(argc, argv);
    else if (!strcmp(argv[0], "clear")) buildin_clear(argc, argv);
    else if (!strcmp(argv[0], "mkdir")) buildin_mkdir(argc, argv);
    else if (!strcmp(argv[0], "rmdir")) buildin_rmdir(argc, argv);
    else if (!strcmp(argv[0], "rm"))    buildin_rm(argc, argv);
    else { // 执行外部命令，先 fork 出一个子进程然后调用 execv 去执行
        pid_t pid = fork();
        if (pid) { // 父进程
            // /* 父进程一般先于子进程执行，如果不加
            //  * 在进入下一轮循环中会将 final_path 清空
            //  * 那么子进程无法从 final_path 中获取参数 */
            // while(1); 
            int32_t status;
            /* 阻塞父进程等到子进程 exit 后返回其状态
                * 如果所有子进程都还在运行，my_shell 会被阻塞 */
            int32_t child_pid = wait(&status);  
            if (child_pid == -1) {
                panic("my_shell: no child\n");
            }
            printf("child_pid: %d, it's status: %d\n", child_pid, status);
        } else {
            make_clear_abs_path(argv[0], final_path);
            argv[0] = final_path;
            struct stat file_stat;
            memset(&file_stat, 0, sizeof(struct stat));
            // 判断文件是否存在
            if (stat(argv[0], &file_stat) == -1) {
                printf("my_shell: cannot access %s: No such file or directory\n", argv[0]);
                exit(-1);
            } else {
                execv(argv[0], argv);
            }

        }

    }
}

char* argv[MAX_ARG_NR];
int32_t argc = -1;


void my_shell() {
    cwd_cache[0] = '/';
    // cwd_cache[1] = 0;
    while (1) {
        
        print_prompt();
        memset(cmd_line, 0, cmd_len);
        memset(final_path, 0, MAX_PATH_LEN);
        readline(cmd_line, cmd_len);
        if (cmd_line[0] == 0) continue; // 只敲了回车键

        char* pipe_symbol = strchr(cmd_line, '|');  // 此时 pipe_symbo 的值是 cmd_line 中 ｜ 的下标
        if (pipe_symbol) { // 管道
            int32_t pipefd[2] = {-1};
            pipe(pipefd); // 生成管道
            fd_redirect(1, pipefd[1]); // 将标准输出重定向到写管道

            // cmd1
            char* each_cmd = cmd_line;
            pipe_symbol = strchr(each_cmd, '|');
			*pipe_symbol = 0;

            argc = -1;
            argc = cmd_parse(each_cmd, argv, ' ');
            cmd_execute(argc, argv); // 执行第一个命令

            // cmd2 ~ cmdn-1
            each_cmd = pipe_symbol + 1; // 跨过 '|' 处理下一个命令
            fd_redirect(0, pipefd[0]);   // 将标准输入重定向到读管道

            // 除了 cmd1 和 cmdn 外输入输出都需要重定向到管道缓冲区中
            while ((pipe_symbol = strchr(each_cmd, '|'))) {
                *pipe_symbol = 0;
                argc = -1;
                argc = cmd_parse(each_cmd, argv, ' ');
                cmd_execute(argc, argv); 
                each_cmd = pipe_symbol + 1;
            }

            // cmdn
            fd_redirect(1, 1); // 标准输出恢复到在屏幕上显示

            // 执行最后一个命令
            argc = -1;
            argc = cmd_parse(each_cmd, argv, ' ');
            cmd_execute(argc, argv); // 执行第一个命令

            fd_redirect(0, 0); // 标准输入恢复到从键盘中读取

            // 关闭管道
            close(pipefd[0]); close(pipefd[1]);

        } else { // 普通命令不含管道
            argc = -1;
            argc = cmd_parse(cmd_line, argv, ' ');
            if (argc == -1) {
				printf("num of arguments exceed %d\n", MAX_ARG_NR);
				continue;
			}
            cmd_execute(argc, argv); 

        }     

    }
    PANIC("my_shell: should not be here");
}
