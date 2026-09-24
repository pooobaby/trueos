#include "console.h"
#include "print.h"
#include "stdint.h"
#include "sync.h"

static struct lock console_lock;    // 控制台锁

// 初始化终端
void console_init() {
   // put_str("- console_init...");
   lock_init(&console_lock);
   // put_str("done!\n");
}

// 获取终端
void console_acquire() {
   lock_acquire(&console_lock);
}

// 释放终端
void console_release() {
   lock_release(&console_lock);
}

// 终端中输出字符串
void console_put_str(char* str) {
   console_acquire(); 
   put_str(str); 
   console_release();
}

// 终端中以指定属性输出字符串
void console_put_str_color(char* str, uint8_t attr) {
   console_acquire();
   put_str_color(str, attr);
   console_release();
}

// 终端中输出字符
void console_put_char(uint8_t char_asci) {
   console_acquire(); 
   put_char(char_asci); 
   console_release();
}

// 终端中以指定属性输出字符
void console_put_char_color(uint8_t char_asci, uint8_t attr) {
   console_acquire();
   put_char_color(char_asci, attr);
   console_release();
}

// 终端中输出 16 进制整数
void console_put_int(uint32_t num) {
   console_acquire(); 
   put_int(num); 
   console_release();
}
