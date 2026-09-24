#ifndef __USERPROG_PROCESS_H 
#define __USERPROG_PROCESS_H

#include "thread.h"
#include "stdint.h"

#define default_prio 8
// 0xC0000000  ← 内核空间起始（用户态不可访问）
// 0xBFFFF000  ← 用户栈初始栈顶 ESP = USER_STACK3_VADDR
//     │   栈向下增长（push）
#define USER_STACK3_VADDR (0xC0000000 - 0x1000)
// 0x8048000 是 32 位 x86 Linux ELF 可执行文件的标准 text 段加载基址, 
// c00 沿用它既保证了与标准 ELF/ld 链接约定的兼容（未来加载标准 ELF 无需重定位）, 
// 又给 NULL 保护区、用户堆向上增长、用户栈向下增长三者在 3GB 用户空间中留出了合理分布。
#define USER_VADDR_START 0x8048000

void process_execute(void* filename, char* name);
void start_process(void* filename_);
void process_activate(struct task_struct* p_thread);
void page_dir_activate(struct task_struct* p_thread);
uint32_t* create_page_dir(void);
void create_user_vaddr_bitmap(struct task_struct* user_prog);

#endif
