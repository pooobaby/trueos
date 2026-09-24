#include "syscall-init.h"
#include "syscall.h"
#include "stdint.h"
#include "print.h"
#include "thread.h"
#include "console.h"
#include "fs.h"
#include "fork.h"
#include "print.h"
#include "exec.h"
#include "wait_exit.h"
#include "pipe.h"

#define syscall_nr 32 
typedef void* syscall;
syscall syscall_table[syscall_nr];

static char* task_status_str[] = {
    "RUNNING", "READY", "BLOCKED", "WAITING", "HANGING", "DIED"
};

uint32_t sys_getpid(void) {
    return running_thread()->pid;
}

char* sys_myname(void) {
    return MY_NAME;
}

void sys_print_pcb(void) {
    struct task_struct* cur = running_thread();
    console_put_str("\n===== PCB =====\n");
    console_put_str(" name: ");
    console_put_str(cur->name);
    console_put_char('\n');

    console_put_str(" pid: 0x");
    console_put_int((int)cur->pid);
    console_put_char('\n');

    console_put_str(" self_kstack: 0x");
    console_put_int((int)cur->self_kstack);
    console_put_char('\n');

    console_put_str(" pgdir: 0x");
    console_put_int((int)cur->pgdir);
    console_put_char('\n');

    console_put_str(" priority: 0x");
    console_put_int((int)cur->priority);
    console_put_char('\n');

    console_put_str(" ticks: 0x");
    console_put_int((int)cur->ticks);
    console_put_char('\n');

    console_put_str(" elapsed_ticks: 0x");
    console_put_int((int)cur->elapsed_ticks);
    console_put_char('\n');

    console_put_str(" status: ");
    console_put_str(task_status_str[cur->status]);
    console_put_char('\n');

    console_put_str(" u_vaddr_start: 0x");
    console_put_int((int)cur->userprog_vaddr.vaddr_start);
    console_put_char('\n');

    console_put_str("=== PCB End ===\n");
}

void sys_putstr_color(char* str, uint8_t attr) {
    console_put_str_color(str, attr);
}

void syscall_init(void) {
    // put_str("- syscall_init start...");
    syscall_table[SYS_GETPID] = sys_getpid;     // syscall_init.c
    syscall_table[SYS_WRITE] = sys_write;       // fs.c
    syscall_table[SYS_MYMALLOC] = sys_malloc;   // malloc.c
    syscall_table[SYS_MYFREE] = sys_free;       // memory.c
    syscall_table[SYS_FORK] = sys_fork;         // fork.c
    syscall_table[SYS_READ] = sys_read;         // fs.c
    syscall_table[SYS_PUTCHAR] = sys_putchar;   // fs.c
    syscall_table[SYS_CLEAR] = cls_screen;      // print.s
    syscall_table[SYS_GETCWD] = sys_getcwd;     // fs.c
    syscall_table[SYS_OPEN] = sys_open;         // fs.c
    syscall_table[SYS_CLOSE] = sys_close;       // fs.c
    syscall_table[SYS_LSEEK] = sys_lseek;       // fs.c
    syscall_table[SYS_UNLINK] = sys_unlink;     // fs.c
    syscall_table[SYS_MKDIR] = sys_mkdir;       // fs.c
    syscall_table[SYS_OPENDIR] = sys_opendir;   // fs.c
    syscall_table[SYS_CLOSEDIR] = sys_closedir; // fs.c
    syscall_table[SYS_CHDIR] = sys_chdir;       // fs.c
    syscall_table[SYS_RMDIR] = sys_rmdir;       // fs.c
    syscall_table[SYS_READDIR] = sys_readdir;   // fs.c
    syscall_table[SYS_REWINDDIR] = sys_rewinddir; // fs.c
    syscall_table[SYS_STAT] = sys_stat;         // fs.c
    syscall_table[SYS_PS] = sys_ps;             // shell.c
    syscall_table[SYS_EXECV] = sys_execv;       // exec.c
    syscall_table[SYS_EXIT] = sys_exit;         // wait_exit.c
    syscall_table[SYS_WAIT] = sys_wait;         // wait_exit.c
    syscall_table[SYS_PIPE] = sys_pipe;         // pipe.c
    syscall_table[SYS_FD_REDIRECT] = sys_fd_redirect; // pipe.c
    syscall_table[SYS_HELP] = sys_help;         // fs.c

    syscall_table[SYS_MYNAME] = sys_myname;     // syscall_init.c
    syscall_table[SYS_PRINT_PCB] = sys_print_pcb;       // syscall_init.c
    syscall_table[SYS_PUTS_COLOR] = sys_putstr_color;   // syscall_init.c
    // put_str("done!\n");
}
