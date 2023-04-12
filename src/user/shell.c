
#include <kernel/debug.h>
#include <kernel/string.h>
#include <kernel/global.h>
#include <user/syscall.h>
#include <user/buildin_cmd.h>
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
        argc = -1;
        argc = cmd_parse(cmd_line, argv, ' ');

        if (argc == -1) {
            printf("num of arguments exceed %d\n", MAX_ARG_NR);
            continue;

        }
        
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
        else printf("external command\n");
    }
    PANIC("my_shell: should not be here");
}
