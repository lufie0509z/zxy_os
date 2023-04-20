#include <kernel/debug.h>
#include <kernel/string.h>
#include <user/shell.h>
#include <user/assert.h>
#include <user/syscall.h>
#include <fs/fs.h>
#include <fs/dir.h>
#include <fs/file.h>
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

void buildin_pwd(uint32_t argc, char** argv UNUSED) {
    if (argc != 1) {
        printf("pwd: no argument support!\n");
        return;
    } else {
        if (getcwd(final_path, MAX_PATH_LEN) != NULL) printf("%s\n", final_path);
        else printf("pwd: get current work directory failed.\n");
    }
}

char* buildin_cd(uint32_t argc, char** argv) {
    if (argc > 2) {
        printf("cd: only support 1 argument!\n");
        return NULL;
    }

    if (argc == 1) {
        final_path[0] = '/';
        final_path[1] = 0;
    } else make_clear_abs_path(argv[1], final_path);

    if (chdir(final_path) == -1) {
        printf("cd: no such directory %s\n", final_path);
        return NULL;
    }
    return final_path;
}


void buildin_ls(uint32_t argc, char** argv) {
    char* pathname = NULL;
    struct stat file_stat;
    memset(&file_stat, 0, sizeof(struct stat));
    bool long_info = false;
    uint32_t arg_path_nr = 0;
    uint32_t arg_idx = 1;
    while (arg_idx < argc) {
        if (argv[arg_idx][0] == '-') { // 选项
            if (!strcmp(argv[arg_idx], "-l")) long_info = true;
            else if (!strcmp(argv[arg_idx], "-h")) {
                printf("usage: -l list all infomation about the file.\n");
                printf("-h for help\n");
                printf("list all files in the current dirctory if no option\n");
                return;
            } else { 
                // 只支持 -l 和 -h 两个选项
                printf("ls: invalid option %s\n", argv[arg_idx]);
                printf("Try `ls -h' for more information.\n");
                return;
            }
        } else {
            if (arg_path_nr == 0) { // ls 的路径参数
                pathname = argv[arg_idx];
                arg_path_nr = 1;
            } else {
                printf("ls: only support one path\n");
                return;
            }
        }
        arg_idx++;
    }
    
    if (pathname == NULL) {
       
        if (getcwd(final_path, MAX_PATH_LEN) != NULL) {
            pathname = final_path;
        } else {
            printf("ls: getcwd for default path failed\n");
            return;
        }
    } else {
        make_clear_abs_path(pathname, final_path);
        pathname = final_path;
    }
   
    if (stat(pathname, &file_stat) == -1) {
        printf("ls: cannot access %s: No such file or directory\n", pathname);
        return;
    } 
    // printf("FT_DIR\n");
    if (file_stat.st_filetype == FT_DIR) {
        struct dir* dir = opendir(pathname);
        struct dir_entry* dir_e = NULL;
        char subpath_name[MAX_PATH_LEN] = {0};
        uint32_t pathname_len = strlen(pathname);
        uint32_t last_char_idx = pathname_len - 1;
        memcpy(subpath_name, pathname, pathname_len);
        if (subpath_name[last_char_idx] != '/') {
            subpath_name[pathname_len] = '/';
            pathname_len++;
        }
        rewinddir(dir);
        
        if (long_info) {
            char ftype;
            printf("total: %d\n", file_stat.st_size);
            while ((dir_e = readdir(dir))) {
                ftype = 'd';
                if (dir_e->f_type == FT_REGULAR) ftype = '-';
                subpath_name[pathname_len] = 0;
                strcat(subpath_name, dir_e->filename);
                memset(&file_stat, 0, sizeof(struct stat));
                if (stat(subpath_name, &file_stat) == -1) {
                    printf("ls: cannot access %s: No such file or directory\n", dir_e->filename);
                    return;
                }
                printf("%c  %d  %d  %s\n", ftype, dir_e->i_no, file_stat.st_size, dir_e->filename);
            }
        } else {
            while ((dir_e = readdir(dir))) {
                printf("%s ", dir_e->filename);
            }
            printf("\n");
        }
        closedir(dir);
    } else {
        if (long_info) printf("-  %d  %d  %s\n", file_stat.st_ino, file_stat.st_size, pathname);
        else printf("%s\n", pathname);
    }
    
}

void  buildin_ps(uint32_t argc, char** argv UNUSED) {
    if (argc != 1) {
        printf("ps: no argument support\n");
        return NULL;
    }
    ps();
}

void  buildin_clear(uint32_t argc, char** argv) {
    if (argc != 1) {
        printf("clear: no argument support\n");
        return NULL;
    }
    clear();
}

int32_t buildin_mkdir(uint32_t argc, char** argv) {
    int32_t ret = -1;
    if (argc != 2)  printf("mkdir: only support 1 argument!\n");
    else {
        make_clear_abs_path(argv[1], final_path);
        if (strcmp(final_path, "/")) { // 创建的不是根目录
            if (mkdir(final_path) == 0) ret = 0;
            else printf("mkdir: create directory %s failed!\n", argv[1]);
        }
    }
    return ret;
}

int32_t buildin_rmdir(uint32_t argc, char** argv) {
    int32_t ret = -1;
    if (argc != 2) printf("rmdir: only support 1 argument!\n");
    else {
        make_clear_abs_path(argv[1], final_path);
        if (strcmp(final_path, "/")) { // 删除的不是根目录
            if (rmdir(final_path) == 0) ret = 0;
            else printf("rmdir: remove directory %s failed!\n", argv[1]);
        }
    }
    return ret;
}

int32_t buildin_rm(uint32_t argc, char** argv) {
    int32_t ret = -1;
    if (argc != 2) printf("rm: only support 1 argument!\n");
    else {
        make_clear_abs_path(argv[1], final_path);
        if (strcmp(final_path, "/")) { // 删除的不是根目录
            if (unlink(final_path) == 0) ret = 0;
            else printf("rm: delete file %s failed!\n", argv[1]);
        }
    }
    return ret;
}


void buildin_help(uint32_t argc, char** argv) {
    help();
}