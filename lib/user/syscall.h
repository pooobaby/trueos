#ifndef __LIB_USER_SYSSCALL_H
#define __LIB_USER_SYSSCALL_H

#include "stdint.h"
#include "fs.h"

enum SYSCALL_NR {
    SYS_GETPID,
    SYS_WRITE,
    SYS_MYMALLOC,
    SYS_MYFREE,
    SYS_FORK,
    SYS_READ,
    SYS_PUTCHAR,
    SYS_CLEAR,
    SYS_GETCWD,
    SYS_OPEN,
    SYS_CLOSE,
    SYS_LSEEK,
    SYS_UNLINK,
    SYS_MKDIR,
    SYS_OPENDIR,
    SYS_CLOSEDIR,
    SYS_CHDIR,
    SYS_RMDIR,
    SYS_READDIR,
    SYS_REWINDDIR,
    SYS_STAT,
    SYS_PS,
    SYS_EXECV,
    SYS_EXIT,
    SYS_WAIT,
    SYS_PIPE,
    SYS_FD_REDIRECT,
    SYS_HELP,
    SYS_MYNAME,
    SYS_PRINT_PCB,
    SYS_PUTS_COLOR
};

uint32_t getpid(void);
uint32_t write(int32_t fd, const void* buf, uint32_t count);
void* mymalloc(uint32_t size);
void myfree(void* ptr);
int16_t fork(void);
uint32_t read(int32_t fd, void* buf, uint32_t count);
void putchar(char char_asci);
void clear(void);
char* getcwd(char* buf, uint32_t size);
int32_t open(char* pathname, uint8_t flag);
int32_t close(int32_t fd);
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t unlink(const char* pathname);
int32_t mkdir(const char* pathname);
struct dir* opendir(const char* name);
int32_t closedir(struct dir* dir);
int32_t rmdir(const char* pathname);
struct dir_entry* readdir(struct dir* dir);
void rewinddir(struct dir* dir);
int32_t stat(const char* path, struct stat* buf);
int32_t chdir(const char* path);
void ps(void);
int32_t execv(const char* pathname, char** argv);
void exit(int32_t status);
int32_t wait(int32_t* status);
int32_t pipe(int32_t pipefd[2]);
void fd_redirect(uint32_t old_local_fd, uint32_t new_local_fd);
void help(void);

char* myname(void);
void print_pcb(void);
void putstr_color(char* str, uint8_t attr);

#endif