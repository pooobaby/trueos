#ifndef __LIB_KERNEL_IO_H__
#define __LIB_KERNEL_IO_H__
#include "stdint.h"

/*  向端口 port 写入一个字节 */
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile ("outb %b0, %w1" : : "a" (data), "Nd" (port));
    /*
    # DX <- port, AL <- data
    outb dx, al        ; 把 AL 中的一个字节写到 DX 指定的端口
    */
}

/*  将 addr 处起始的 word_cnt 个字写入端口 port */
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt) {
    __asm__ volatile ("cld; rep outsw" : "+S" (addr), "+c" (word_cnt) : "d" (port));
    /*
    # ESI <- addr, ECX <- word_cnt, DX <- port
    cld
    rep outsw
    */
}

/* 将从端口 port 读入的一个字节返回 */
static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    __asm__ volatile ("inb %w1, %b0" : "=a" (data) : "Nd" (port));
    return data;
    /*
    # DX <- port
    inb dx, al        ; 把 DX 指定的端口中的字节读到 AL 中
    */
}

/* 将从端口 port 读入的 word_cnt 个字写入 addr */
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) {
    __asm__ volatile ("cld; rep insw" : "+D" (addr), "+c" (word_cnt) : "d" (port) : "memory");
    /*
    # EDI <- addr, ECX <- word_cnt, DX <- port
    cld
    rep insw
    */
}

#endif
