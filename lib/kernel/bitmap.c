#include "bitmap.h"
#include "string.h"
#include "debug.h"

/* 将位图 btmp 初始化 */
void bitmap_init(struct bitmap *btmp) {
    memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/*
判断 bit_idx 位是否为 1, 若为 1 则返回 true, 否则返回 false
bits[0]:  [7] [6] [5] [4] [3] [2] [1] [0]
bits[1]:  [15][14][13][12][11][10][9] [8]
bits[2]:  [23][22][21][20][19][18][17][16]
...
bit_idx: 位图中按位编号的索引
*/
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {
    uint32_t byte_idx = bit_idx / 8;    // 向下取整用于索引数组下标
    uint32_t bit_odd = bit_idx % 8;     // 取余用于索引数组内的位
    return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd));
}

/* 在位图中申请连续 cnt 个位, 成功则返回其起始位下标, 失败返回 -1 */
int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {
    uint32_t idx_byte = 0;	 // 用于记录空闲位所在的字节
    // 先逐字节比较
    while ((0xFF == btmp->bits[idx_byte]) && (idx_byte < btmp->btmp_bytes_len))
        idx_byte++;

    ASSERT(idx_byte < btmp->btmp_bytes_len);
    // 若该内存池找不到可用空间	
    if (idx_byte == btmp->btmp_bytes_len) {	
        return -1;
    }

    // 在该字节内逐位比对, 返回空闲位的索引
    int32_t idx_bit = 0;
    while ((uint8_t)(BITMAP_MASK << idx_bit) & btmp->bits[idx_byte])
        idx_bit++;

    int bit_idx_start = idx_byte * 8 + idx_bit;     // 空闲位在位图内的下标
    if (cnt == 1) {
        return bit_idx_start;
    }

    uint32_t bit_left = (btmp->btmp_bytes_len * 8 - bit_idx_start);
    uint32_t next_bit = bit_idx_start + 1;
    uint32_t count = 1;	      // 用于记录找到的空闲位的个数

    bit_idx_start = -1;
    while (bit_left-- > 0) {
        // 若 next_bit 为 0, 则 count++
        if (!(bitmap_scan_test(btmp, next_bit)))
            count++;
        else
            count = 0;

        // 若找到连续的 cnt 个空位
        if (count == cnt) {
            bit_idx_start = next_bit - cnt + 1;
            break;
        }
        next_bit++;
    }
    return bit_idx_start;
}

/* 将位图 btmp 的 bit_idx 位设置为 value */
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {
    ASSERT(value == 0 || value == 1);
    uint32_t byte_idx = bit_idx / 8;
    uint32_t bit_odd  = bit_idx % 8;

    if (value)
        btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
    else
        btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
}