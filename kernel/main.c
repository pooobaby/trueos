// #include "global.h"
#include "print.h"
#include "init.h"
#include "interrupt.h"
#include "stdio.h"
#include "stdint.h"
#include "syscall.h"
#include "shell.h"
// #include "ide.h"
// #include "printk.h"
#include "assert.h"
#include "thread.h"

void init(void);

int main(void) {
	put_str_color("\n----------------------------------------------------\n", 0x03);
    put_str_color("this is the kernel.\n", 0x03);
	put_str_color("init_all: ", 0x03);
	put_str_color("idt,console,mem,thread,timer,keyboard,tss,syscall,ide...", 0x07);
	put_str_color("all done!", 0x03);
	put_str_color("\n----------------------------------------------------------------------------\n", 0x03);

	init_all();
    intr_enable();	// 打开中断

	/* 生成测试文件
	int fd = sys_open("/dir/dir_file.txt", O_CREAT|O_RDWR);
	if (fd == -1) {
		printk("file open error!\n");
		while(1);
	}
	if(sys_write(fd, "this is dir's file.\n", 20) == -1) {
		printk("file write error!\n");
			while(1);
	}
	*/

	/************* 写入 prog_pipe 程序 *************/
	// uint32_t file_size_cat = 14476;
	// uint32_t sec_cnt_cat = DIV_ROUND_UP(file_size_cat, 512);
	// struct disk* sda_cat = &channels[0].devices[0];
	// void* prog_buf_cat = sys_malloc(file_size_cat);
	// ide_read(sda_cat, 1200, prog_buf_cat, sec_cnt_cat);
	// int32_t fd_cat = sys_open("/prog_pipe", O_CREAT|O_RDWR);
	// if (fd_cat != -1) {
	// 	if(sys_write(fd_cat, prog_buf_cat, file_size_cat) == -1) {
	// 		printk("file write error!\n");
	// 		while(1);
	// 	}
	// }
	/************* 写入应用程序结束 *************/

	/************* 写入 cat 程序 *************/
	// uint32_t file_size_cat = 14472;
	// uint32_t sec_cnt_cat = DIV_ROUND_UP(file_size_cat, 512);
	// struct disk* sda_cat = &channels[0].devices[0];
	// void* prog_buf_cat = sys_malloc(file_size_cat);
	// ide_read(sda_cat, 900, prog_buf_cat, sec_cnt_cat);
	// int32_t fd_cat = sys_open("/cat", O_CREAT|O_RDWR);
	// if (fd_cat != -1) {
	// 	if(sys_write(fd_cat, prog_buf_cat, file_size_cat) == -1) {
	// 		printk("file write error!\n");
	// 		while(1);
	// 	}
	// }
	/************* 写入应用程序结束 *************/

	/************* 写入 prog_arg 程序 *************/
	// uint32_t file_size = 14476;
	// uint32_t sec_cnt = DIV_ROUND_UP(file_size, 512);
	// struct disk* sda = &channels[0].devices[0];
	// void* prog_buf = sys_malloc(file_size);
	// ide_read(sda, 600, prog_buf, sec_cnt);
	// int32_t fd = sys_open("/prog_arg", O_CREAT|O_RDWR);
	// if (fd != -1) {
	// 	if(sys_write(fd, prog_buf, file_size) == -1) {
	// 		printk("file write error!\n");
	// 		while(1);
	// 	}
	// }
	/************* 写入应用程序结束 *************/

	/************* 写入 prog_no_arg 程序 *************/
	// uint32_t file_size_no_arg = 14436;
	// uint32_t sec_cnt_no_arg = DIV_ROUND_UP(file_size_no_arg, 512);
	// struct disk* sda_no_arg = &channels[0].devices[0];
	// void* prog_buf_no_arg = sys_malloc(file_size_no_arg);
	// printf("prog_buf_no_arg: 0x%x\n", prog_buf_no_arg);
	// ide_read(sda_no_arg, 300, prog_buf_no_arg, sec_cnt_no_arg);
	// int32_t fd_no_arg = sys_open("/prog_no_arg", O_CREAT|O_RDWR);
	// if (fd_no_arg != -1) {
	// 	if(sys_write(fd_no_arg, prog_buf_no_arg, file_size_no_arg) == -1) {
	// 		printk("file write error!\n");
	// 		while(1);
	// 	}
	// }
	/************* 写入应用程序结束 *************/

    // while (1) ;
	thread_exit(running_thread(), true);
    return 0;
}

// 初始化 fork 进程
void init(void) {
	printf("\n");
	uint32_t ret_pid = fork();
	if (ret_pid) {
		int status;
		int child_pid;
		// init 在此处不停地回收僵尸进程
		while(1) {
			child_pid = wait(&status);
			printf("I`m init, My pid is 1, I recieve a child, It`s pid is %d, status is %d\n", child_pid, status);
		}
	} else {
		my_shell();
	}
	panic("init: should not be here");
}
