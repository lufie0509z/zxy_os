#include <lib/kernel/bitmap.h>
#include <lib/kernel/stdint.h>
#include <lib/kernel/print.h>
#include <kernel/string.h>
#include <kernel/interrupt.h>
#include <kernel/debug.h>

void bitmap_init(struct bitmap* btmap) {
    memset(btmap->bits, 0, btmap->btmp_bytes_len);
}

/**
 * 检测指定位是否为1,如果是,返回1.
 */
uint8_t bitmap_scan_test(struct bitmap* btmap, uint32_t index) {
    uint32_t byte_index = (index / 8);
    uint32_t bit_odd = index % 8;

    return (btmap->bits[byte_index] & BITMAP_MASK << bit_odd);
}

/**
 * 在位图中申请连续的cnt个位.
 */
int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {
    uint32_t idx_byte = 0;
    /* 先逐字节比较，蛮力法 */ 
    while (( 0xff == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len)) { 
        idx_byte++; //1 表示该位已分配，若为 0xff，则表示该字节内已无空闲位，向下一字节继续找 */ 
    } 

    ASSERT(idx_byte < btmp->btmp_bytes_len); 
    if (idx_byte == btmp->btmp_bytes_len) { // 若该内存池找不到可用空间
        return -1; 
    } 
/* 若在位图数组范围内的某字节内找到了空闲,在该字节内逐位比对，返回空闲位的索引。*/ 
    int idx_bit = 0; 
    /* 和 btmp->bits[idx_byte]这个字节逐位对比 */ 
    while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte]) { 
        idx_bit++; 
    } 
    
    int bit_idx_start = idx_byte * 8 + idx_bit; // 空闲位在位图内的下标
    if (cnt == 1) { 
        return bit_idx_start; 
    } 
    
    uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start); // 记录还有多少位可以判断
    uint32_t next_bit = bit_idx_start + 1; 
    uint32_t count = 1; // 用于记录找到的空闲位的个数

    bit_idx_start = -1; // 先将其置为−1，若找不到连续的位就直接返回
    while (bit_left-- > 0) { 
    if (!(bitmap_scan_test(btmp, next_bit))) { // 若 next_bit 为 0 
        count ++; 
    } else { 
        count = 0; 
    } 
    if (count == cnt) { // 若找到连续的 cnt 个空位
        bit_idx_start = next_bit - cnt + 1; 
        break; 
    } 
    next_bit ++; 
    } 
    return bit_idx_start;
}


void bitmap_set(struct bitmap* btmap, uint32_t index, int8_t value) {
    ASSERT(value == 0 || value == 1);

    uint32_t byte_index = index / 8;
    uint32_t bit_odd = index % 8;

    if (value) {
        btmap->bits[byte_index] |= (BITMAP_MASK << bit_odd);
    } else {
        btmap->bits[byte_index] &= ~(BITMAP_MASK << bit_odd);
    }
}
