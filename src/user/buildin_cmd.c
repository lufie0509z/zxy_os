#include <kernel/debug.h>
#include <kernel/string.h>
#include <fs/fs.h>
#include <fs/dir.h>
#include <fs/file.h>
#include <user/syscall.h>
#include <lib/stdio.h>

// 将 old_abs_path 中的 . 和 .. 转换为实际路径后存入 new_abs_path 中
static void wash_path(char* old_abs_path, char* new_abs_path) {
    ASSERT(old_abs_path[0] == '/');
    char name[MAX_FILE_NAME_LEN] = {0};
    char* subpath = old_abs_path;
    subpath = path_parse(subpath, name);
    if (name[0] == 0) {
        // 只键入了 "/"
        new_abs_path[0] = '/';
        new_abs_path[1] = 0;
        return;
    }
    new_abs_path[0] = 0; // 避免缓冲区不干净
    strcat(new_abs_path, "/");
    while (name[0]) {
  
        if (!strcmp("..", name)) { // 回退到上级目录
            char* slash_ptr = strrchr(new_abs_path, '/');  // 找到 new_abs_path 中最右边的 “/” 的地址
            if (slash_ptr != new_abs_path) *slash_ptr = 0; // "/a/b" -> "/a" 
            else *(slash_ptr + 1) = 0;                     // "/a"   -> "/"
        } else if (strcmp(".", name)) {
            // 如果 new_abs_path 不是"/", 就拼接一个 "/"
            if (strcmp(new_abs_path, "/")) strcat(new_abs_path, "/"); 
            strcat(new_abs_path, name);
        }
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (subpath) subpath = path_parse(subpath, name);
       
    }
}

// 将path处理成不含 . 和 .. 的绝对路径，存储到final_path
void make_clear_abs_path(char* path, char* final_path) {
    char abs_path[MAX_PATH_LEN] = {0};
    // 如果输入的是相对路径，就拼接成绝对路径
    if (path[0] != '/') {
        memset(abs_path, 0, MAX_PATH_LEN);
        if (getcwd(abs_path, MAX_PATH_LEN) != NULL) { // 得到当前目录
            if (!((abs_path[0] == '/') && (abs_path[1] == 0))) { // 不是根目录
                strcat(abs_path, "/");
            }
        }
    }
    strcat(abs_path, path);
    wash_path(abs_path, final_path);
}
