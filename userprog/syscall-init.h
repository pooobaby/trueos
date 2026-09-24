#ifndef __USERPROG_SYSCALLINIT_H
#define __USERPROG_SYSCALLINIT_H

#include "stdint.h"

void syscall_init(void);
uint32_t sys_getpid(void);
char* sys_myname(void);
void sys_print_pcb(void);

#endif