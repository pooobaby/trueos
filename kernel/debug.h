#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

void panic_spin(char* filename, int line, const char* func, const char* condition);

/* 
__VA_ARGS__ 是预处理器层面宏的可变参数, 在宏展开时就已确定
__VA_ARGS__ 展开后将作为 panic_spin 函数的实参
*/
#define PANIC(...) panic_spin (__FILE__, __LINE__, __func__, __VA_ARGS__)

    #ifdef NDEBUG
        #define ASSERT(CONDITION) ((void)0)
    #else
        #define ASSERT(CONDITION)       \
            if (CONDITION) {} else {    \
                PANIC(#CONDITION);      \
            }
    #endif
#endif
