#ifndef __LIB_KERNEL_PRINTK_H
#define __LIB_KERNEL_PRINTK_H

#include "stdint.h"

void printk(const char* format, ...);
void printkc(uint8_t attr, const char* format, ...);

#endif