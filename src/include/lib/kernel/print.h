#ifndef _LIB_KERNEL_PRINT_H
#define _LIB_KERNEL_PRINT_H
#include <lib/kernel/stdint.h>

void put_char(uint8_t char_asci);
void put_str(char* message);
void put_int(uint32_t num); // 以十六进制打印
#endif