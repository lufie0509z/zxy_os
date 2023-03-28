
#include <kernel/debug.h>
#include <kernel/string.h>
#include <kernel/global.h>
#include <user/syscall.h>
#include <lib/stdio.h>
#include <lib/kernel/stdint.h>
#include <fs/file.h>

#define cmd_len    128  // 命令行参数的最多有128个字符
#define MAX_ARG_NR 16   // 包括命令名最多支持的参数个数

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

void my_shell() {
    cwd_cache[0] = '/';
    while (1) {
        print_prompt();
        memset(cmd_line, 0, cmd_len);
        readline(cmd_line, cmd_len);
        if (cmd_line[0] == 0) continue; // 只敲了回车键
    }
    PANIC("my_shell: should not be here");
}