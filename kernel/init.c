#include "init.h"
// #include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"

/*负责初始化所有模块 */
void init_all() {
   idt_init();          // 初始化中断
   console_init();      // 初始化控制台
   mem_init();          // 初始化内存管理系统
   thread_init();       // 初始化线程相关结构
   timer_init();        // 初始化 PIT
   keyboard_init();     // 初始化键盘
   tss_init();          // 初始化 TSS
   syscall_init();      // 初始化系统调用
   ide_init();          // 初始化硬盘
   filesys_init();      // 初始化文件系统
}