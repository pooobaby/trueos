#ifndef __LIB_KERNEL_PRINT_H
#define __LIB_KERNEL_PRINT_H

#include "stdint.h"

// VGA 文本属性字节: bit7=闪烁, bits6-4=背景色, bit3=前景高亮, bits2-0=前景色
// [bits2-0=前景色] 000 黑色 001 蓝色 010 绿色 011 青色 100 红色 101 紫色 110 棕色 111 灰色
#define FG_BLACK    0x0
#define FG_BLUE     0x1
#define FG_GREEN    0x2
#define FG_CYAN     0x3
#define FG_RED      0x4
#define FG_MAGENTA  0x5
#define FG_BROWN    0x6
#define FG_LGRAY    0x7
#define FG_DGRAY    0x8
#define FG_LBLUE    0x9
#define FG_LGREEN   0xA
#define FG_LCYAN    0xB
#define FG_LRED     0xC
#define FG_LMAGENTA 0xD
#define FG_YELLOW   0xE
#define FG_WHITE    0xF

#define BG_BLACK    (0x0 << 4)
#define BG_BLUE     (0x1 << 4)
#define BG_GREEN    (0x2 << 4)
#define BG_CYAN     (0x3 << 4)
#define BG_RED      (0x4 << 4)
#define BG_MAGENTA  (0x5 << 4)
#define BG_BROWN    (0x6 << 4)
#define BG_LGRAY    (0x7 << 4)

// 用前景色和背景色组合出属性字节
#define MAKE_ATTR(fg, bg)  ((fg) | (bg))

void put_char(uint8_t char_asci);       // 打印一个字符
void put_char_color(uint8_t char_asci, uint8_t attr); // 打印一个字符, 并指定属性
void put_str(char *str);                // 打印一个字符串
void put_str_color(char *str, uint8_t attr); // 打印一个字符串, 并指定属性
void put_int(uint32_t num);             // 打印一个16进制的数字
void set_cursor(uint32_t pos);          // 设置光标位置
void cls_screen(void);                  // 清除屏幕, 并将光标设置到屏幕的首字符

#endif