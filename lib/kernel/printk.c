#include "printk.h"
#include "stdio.h"
#include "console.h"
#include "global.h"

#define k_va_start(args, first_fix) args = (va_list)&first_fix
#define k_va_end(args) args = NULL

// 供内核使用的格式化输出函数
void printk(const char* format, ...) {
   va_list args;
   k_va_start(args, format);
   char buf[1024] = {0};
   vsprintf(buf, format, args);
   k_va_end(args);
   console_put_str(buf);
}

// 供内核使用的彩色格式化输出函数, attr 为 VGA 文本属性字节
void printkc(uint8_t attr, const char* format, ...) {
   va_list args;
   k_va_start(args, format);
   char buf[1024] = {0};
   vsprintf(buf, format, args);
   k_va_end(args);
   console_put_str_color(buf, attr);
}