#include "string.h"
// #include "global.h"
#include "assert.h"

/* 将 dst_ 起始的 size 个字节置为 value */
void memset(void* dst_, uint8_t value, uint32_t size) {
    assert(dst_ != NULL);
    // uint8_t* 是字节指针, 清晰地表达了"我要逐字节操作内存"的意图
    uint8_t* dst = (uint8_t*) dst_;
    while (size-- > 0)
        *dst++ = value;
}

/* 将 src_ 起始的 size 个字节复制到 dst_ */
void memcpy(void* dst_, const void* src_, uint32_t size) {
    assert(dst_ != NULL && src_ != NULL);
    uint8_t* dst = dst_;
    const uint8_t* src = src_;
    while (size-- > 0)
        *dst++ = *src++;
}

/*
连续比较以地址 a_ 和地址 b_ 开头的 size 个字节, 
相等则返回 0, a_ 大于 b_返回 +1, 否则返回 -1
*/
int memcmp(const void* a_, const void* b_, uint32_t size) {
    const char* a = a_;
    const char* b = b_;
    assert(a != NULL || b != NULL);
    while (size-- > 0) {
        if(*a != *b) {
            return *a > *b ? 1 : -1; 
        }
        a++;
        b++;
    }
    return 0;
}

/* 将字符串从 src_ 复制到 dst_ */
char* strcpy(char* dst_, const char* src_) {
    assert(dst_ != NULL && src_ != NULL);
    char* r = dst_;
    while((*dst_++ = *src_++));
    return r;
}

/* 返回字符串长度 */
uint32_t strlen(const char* str) {
    assert(str != NULL);
    const char* p = str;
    while(*p++);
    return (p - str - 1);
}

/*
比较两个字符串, 若 a_ 中的字符大于 b_ 中的字符返回 1, 
相等时返回 0, 否则返回 -1
*/
int8_t strcmp(const char *a, const char *b) {
    assert(a != NULL && b != NULL);
    while (*a != 0 && *a == *b) {
        a++;
        b++;
    }
    return *a < *b ? -1 : *a > *b;
}

/* 从左到右查找字符串 str 中首次出现字符 ch 的地址(不是下标, 是地址) */
char* strchr(const char* str, const uint8_t ch) {
    assert(str != NULL);
    while (*str != 0) {
        if (*str == ch) {
            return (char*) str;
        }
        str++;
    }
    return NULL;
}

/* 从后往前查找字符串 str 中首次出现字符 ch 的地址(不是下标, 是地址) */
char* strrchr(const char* str, const uint8_t ch) {
    assert(str != NULL);
    const char* last_char = NULL;
    // 从头到尾遍历一次,若存在 ch 字符, last_char 总是该字符最后一次出现在串中的地址(不是下标,是地址)
    while (*str != 0) {
        if (*str == ch) {
            last_char = str;
        }
        str++;
    }
    return (char*) last_char;
}

/* 将字符串 src_ 拼接到 dst_ 后, 返回拼接后的串地址 */
char* strcat(char* dst_, const char* src_) {
    assert(dst_ != NULL && src_ != NULL);
    char* str = dst_;
    while (*str++);         // 先找到 dst_ 的结束地址
    --str;                  // 指针回退一步, 指向 dst_ 的最后一个字符
    // 当 *str 被赋值为 0 时, 此时表达式不成立, 正好添加了字符串结尾的 0
    while ((*str++ = *src_++));
    return dst_;
}

/* 在字符串 str 中查找指定字符 ch 出现的次数 */
uint32_t strchrs(const char* str, uint8_t ch) {
    assert(str != NULL);
    uint32_t ch_cnt = 0;
    const char* p = str;
    while (*p != 0) {
        if (*p == ch) ch_cnt++;
        p++;
    }
    return ch_cnt;
}
