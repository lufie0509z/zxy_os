#include <lib/stdio.h>
#include <lib/kernel/stdint.h>
#include <kernel/global.h>

#define va_start(ap, v) ap = (va_list)&v  // 将 ap 指向栈中可变参数中的第一个参数v
/**
 *  使 ap 指向栈中的下一个参数，并返回它的值
 *  (ap+=4)指向下一个参数在栈中的地址，将其强制转换成 t 型指针(t*)，最后再用*号取值
 */ 
#define va_arg(ap, t)   *((t*)(ap += 4))   
#define va_end(ap)      ap = NULL


// 将整型转换为字符串，也就是 integer to ascii 
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base) {
    uint32_t m = value % base;
    uint32_t i = value / base;
    if (i) {
        itoa(i, buf_ptr_addr, base);
    }

    if (m < 10) {
        *((*buf_ptr_addr)++) = m + '0'; //
    } else {
        *((*buf_ptr_addr)++) = m - 10 + 'A'; 
    }
}

// 将参数 ap 按照格式 format 输出到字符串 str 并返回替换后 str 的长度。
uint32_t vsprintf(char* str, const char* format, va_list ap) {
    char* buf_ptr = str;
    const char* index_ptr = format;
    char index_char = *index_ptr;

    int32_t arg_int;
    char* arg_str;

    while (index_char)
    {
        if (index_char != '%') {
            *(buf_ptr++) = index_char;
            index_char = *(++index_ptr);
            continue;
        }

        index_char = *(++index_ptr); // % 后面的一个字符
        switch (index_char) {
            case 'x':
                arg_int = va_arg(ap, int); 
                itoa(arg_int, &buf_ptr, 16);
                index_char = *(++index_ptr);
                break;
            
            case 's':
                arg_str = va_arg(ap, char*);
                strcpy(buf_ptr, arg_str);
                buf_ptr += strlen(arg_str);
                index_char = *(++index_ptr);
                break;

            case 'c':
                *(buf_ptr++) = va_arg(ap, char);
                index_char = *(++index_ptr);
                break;
            
            case 'd':
                arg_int = va_arg(ap, int);
                // 负数
                if (arg_int < 0) {
                    arg_int = 0 - arg_int;
                    *buf_ptr++ = '-';
                }
                itoa(arg_int, &buf_ptr, 10);
                index_char = *(++index_ptr);
                break;
        }
    }
    return strlen(str);
}

// 将字符串写入缓冲区 buf 中
uint32_t sprintf(char* buf, const char* format, ...) {
    va_list args;
    va_start(args, format);
    uint32_t retval = vsprintf(buf, format, args);
    va_end(args);
    return retval;
}

uint32_t printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    char buf[1024] = {0};
    vsprintf(buf, format, args);
    va_end(args);
    return write(buf);
}

