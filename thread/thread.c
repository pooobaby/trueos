#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "memory.h"
#include "list.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "process.h"
#include "sync.h"
#include "fs.h"
#include "stdio.h"
#include "file.h"

#define PAGE_SIZE 4096
uint8_t pid_bitmap_bits[128] = {0};     // pid的位图,最大支持1024个pid

// pid 池
struct pid_pool {
   struct bitmap pid_bitmap;  // pid 位图
   uint32_t pid_start;	      // 起始 pid
   struct lock pid_lock;      // 分配 pid 锁
} pid_pool;

struct task_struct* main_thread;        // 主线程 PCB
struct task_struct* idle_thread;        // idle 线程 PCB
struct list thread_ready_list;          // 就绪队列
struct list thread_all_list;            // 所有任务队列
struct lock pid_lock;                    // pid 锁, 用于保护 pid 分配
static struct list_elem* thread_tag;    // 用于保存队列中的线程结点

extern void switch_to(struct task_struct* cur, struct task_struct* next);
extern void init(void);

// 系统空闲时运行的线程
static void idle(void* arg UNUSED) {
    while(1) {
        thread_block(TASK_BLOCKED);
        __asm__ volatile ("sti; hlt" : : : "memory");
    }
}

// 获取当前线程 pcb 指针
struct task_struct* running_thread(void) {
    uint32_t esp;
    __asm__ ("mov %%esp, %0" : "=g" (esp));
    return (struct task_struct*)(esp & 0xFFFFF000); // 把任意地址向下对齐到 4KB 页边界
}

// 由kernel_thread 去执行 function(func_arg)
static void kernel_thread(thread_func* function, void* func_arg) {
    // 执行 function 前要开中断, 避免后面的时钟中断被屏蔽, 而无法调度其它线程
    intr_enable();
    function(func_arg); 
}

// 初始化 pid 池
static void pid_pool_init(void) { 
    pid_pool.pid_start = 1;
    pid_pool.pid_bitmap.bits = pid_bitmap_bits;
    pid_pool.pid_bitmap.btmp_bytes_len = 128;
    bitmap_init(&pid_pool.pid_bitmap);
    lock_init(&pid_pool.pid_lock);
}

// 分配 pid
static pid_t allocate_pid(void) {
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
    lock_release(&pid_pool.pid_lock);
    return (bit_idx + pid_pool.pid_start);
}

// 释放 pid
void release_pid(pid_t pid) {
    lock_acquire(&pid_pool.pid_lock);
    int32_t bit_idx = pid - pid_pool.pid_start;
    bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
    lock_release(&pid_pool.pid_lock);
}

// fork 进程时为其分配pid, 因为 allocate_pid 已经是静态的, 别的文件无法调用.
// 不想改变函数定义了, 故定义fork_pid函数来封装一下
pid_t fork_pid(void) {
   return allocate_pid();
}

// 初始化线程的内核栈 thread_stack, 将待执行的函数和参数放到 thread_stack 中相应的位置
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {
    // 预留中断栈的空间 intr_stack 76 字节
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread->self_kstack - sizeof(struct intr_stack));
    // 预留线程栈的空间 thread_stack 32 字节
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread->self_kstack - sizeof(struct thread_stack));
    // pthread->self_kstack -= struct intr_stack;
    // pthread->self_kstack -= struct thread_stack;
    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
    kthread_stack->eip = kernel_thread;     // ret 的落点
    // unused_retaddr 故意不填：它将被 kernel_thread 当作“自己的返回地址”, 但线程体按约定不应返回
    kthread_stack->function = function;     // 第 1 参数
    kthread_stack->func_arg = func_arg;     // 第 2 参数
    // 4 个 callee-saved 寄存器初值清零
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

// 初始化线程基本信息 PCB
void init_thread(struct task_struct *pthread, char *name, int prio) {
    memset(pthread, 0, sizeof(*pthread));
    pthread->pid = allocate_pid();
    strcpy(pthread->name, name);

    // main函数一直是运行的, 故将其直接设为 TASK_RUNNING
    if (pthread == main_thread)
        pthread->status = TASK_RUNNING;
    else
        pthread->status = TASK_READY;
    // self_kstack 是线程的内核栈顶地址
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PAGE_SIZE);
    pthread->priority = prio;
    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;
    pthread->pgdir = NULL;

    // 初始化文件描述符表前三个为标准输入、标准输出、标准错误
    pthread->fd_table[0] = 0;
    pthread->fd_table[1] = 1;
    pthread->fd_table[2] = 2;

    // 文件描述符数组其余的全置为-1
    uint8_t fd_idx = 3;
    while(fd_idx < MAX_FILES_OPEN_PER_PROC) {
        pthread->fd_table[fd_idx] = -1;
        fd_idx++;
    }

    pthread->cwd_inode_nr = 0;  // 以根目录做为默认工作路径
    pthread->parent_pid = -1;   // 父进程pid为-1, 表示是主进程
    pthread->stack_magic = 0x19750130;  // 自定义的魔数
}

// 创建线程, 线程所执行的函数是 function(func_arg)
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {
    // pcb 都位于内核空间, 包括用户进程的 pcb 也是在内核空间
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, prio);                // 初始化线程 PCB
    thread_create(thread, function, func_arg);      // 初始化线程的内核栈

    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);
    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);

    return thread;
}

// 将 kernel 中的 main 函数完善为主线程
static void make_main_thread(void) {
    main_thread = running_thread();
    init_thread(main_thread, "main", 8);
    // main 函数是当前线程, 当前线程不在 thread_ready_list 中
    // 所以只将其加在 thread_all_list 中
    ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
    list_append(&thread_all_list, &main_thread->all_list_tag);
}

void print_info(struct task_struct* cur, struct task_struct* next) {
    put_str(cur->name);
    put_str(" -> ");
    put_str(next->name);
    put_str("\n");
}

// 实现任务调度
void schedule(void) {
    ASSERT(intr_get_status() == INTR_OFF);

    struct task_struct* cur = running_thread();
    if (cur->status == TASK_RUNNING) {
        ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
        list_append(&thread_ready_list, &cur->general_tag);
        cur->ticks = cur->priority;
        cur->status = TASK_READY;
    } else {
        // 若此线程需要某事件发生后才能继续上cpu运行, 不需要将其加入队列, 因为当前线程不在就绪队列中
    }

    // 如果就绪队列中没有可运行的任务, 就唤醒 idle
    if (list_empty(&thread_ready_list))
        thread_unblock(idle_thread);

    ASSERT(!list_empty(&thread_ready_list));
    thread_tag = NULL;
    thread_tag = list_pop(&thread_ready_list);
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;

    // ASSERT(cur != next);    // 切换时一定不是同一个线程, 2026/09/03 在这里入坑很久, 之后发现是 list_push 导致的
    // if (cur != next) {
    //     print_info(cur, next);
    // }

    // printk("%s pid: %d -> %s pid: %d \n", cur->name, cur->pid, next->name, next->pid);
    process_activate(next);     // 击活任务页表

    switch_to(cur, next);
}

// 当前线程将自己阻塞, 标志其状态为 stat
void thread_block(enum task_status stat) {
    ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));
    enum intr_status old_status = intr_disable();
    struct task_struct* cur_thread = running_thread();
    cur_thread->status = stat;
    schedule();
    intr_set_status(old_status);
}

// 将线程 pthread 从阻塞队列中唤醒, 标志其状态为 TASK_READY
void thread_unblock(struct task_struct* pthread) {
    enum intr_status old_status = intr_disable();
    ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));
    if (pthread->status != TASK_READY) {
        ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
        if (elem_find(&thread_ready_list, &pthread->general_tag))
            PANIC("thread_unblock: blocked thread in ready_list\n");
        list_push(&thread_ready_list, &pthread->general_tag);
        pthread->status = TASK_READY;
    }
    intr_set_status(old_status);
}

// 主动让出 cpu, 换其它线程运行
// schedule() 是被动调度入口（由时钟中断 intr_timer_handler 或 thread_block 调用）
// 而 thread_yield 是线程主动让出的语义封装。
void thread_yield(void) {
    struct task_struct* cur = running_thread();
    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag); // 排到就绪队列尾部
    cur->status = TASK_READY;
    schedule();
    intr_set_status(old_status);
}

// 以填充空格的方式输出 buf
static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) {
    memset(buf, 0, buf_len);
    uint8_t out_pad_0idx = 0;
    switch(format) {
        case 's':
            out_pad_0idx = sprintf(buf, "%s", ptr);
            break;
        case 'd':
            out_pad_0idx = sprintf(buf, "%d", *((int16_t*)ptr));
            break;
        case 'x':
            out_pad_0idx = sprintf(buf, "%x", *((uint32_t*)ptr));
            break;
    }
    // 以空格填充
    while(out_pad_0idx < buf_len) {
        buf[out_pad_0idx] = ' ';
        out_pad_0idx++;
    }
    sys_write(stdout_no, buf, buf_len - 1);
}

// 用于在 list_traversal 函数中的回调函数, 用于针对线程队列的处理
static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    char out_pad[16] = {0};

    // 打印 PID/PPID 列
    int col_idx = 0;
    if (pthread->parent_pid == -1) {
        col_idx = sprintf(out_pad, "%d/NULL", pthread->pid);
    } else {
        col_idx = sprintf(out_pad, "%d/%d", pthread->pid, pthread->parent_pid);
    }
    while (col_idx < 16) {
        out_pad[col_idx] = ' ';
        col_idx++;
    }
    sys_write(stdout_no, out_pad, 15);   // 只输出前 15 字节, 与 pad_print 的 buf_len-1 保持一致

    // 打印 ADDR 列
    pad_print(out_pad, 16, &pthread, 'x');

    // 打印 STAT 列
    switch (pthread->status) {
        case 0:
            pad_print(out_pad, 16, "RUNNING", 's');
            break;
        case 1:
            pad_print(out_pad, 16, "READY", 's');
            break;
        case 2:
            pad_print(out_pad, 16, "BLOCKED", 's');
            break;
        case 3:
            pad_print(out_pad, 16, "WAITING", 's');
            break;
        case 4:
            pad_print(out_pad, 16, "HANGING", 's');
            break;
        case 5:
            pad_print(out_pad, 16, "DIED", 's');
    }
    // 打印 TICKS 列
    pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

    // 打印 COMMAND 列
    memset(out_pad, 0, 16);
    ASSERT(strlen(pthread->name) < 17);
    memcpy(out_pad, pthread->name, strlen(pthread->name));
    strcat(out_pad, "\n");
    sys_write(stdout_no, out_pad, strlen(out_pad));
    // 此处返回 false 是为了迎合主调函数 list_traversal, 只有回调函数返回 false 时才会继续调用此函数
    return false;	
}

// 打印任务列表
void sys_ps(void) {
   char* ps_title = "PID/PPID       ADDR           STAT           TICKS          COMMAND\n";
   char* ps_line = "--------------------------------------------------------------------\n";
   put_str_color(ps_title, 0x2);
   put_str_color(ps_line, 0x2);
//    sys_write(stdout_no, ps_title, strlen(ps_title));
//    sys_write(stdout_no, ps_line, strlen(ps_line));
   list_traversal(&thread_all_list, elem2thread_info, 0);
}

// 回收 thread_over 的 pcb 和页表, 并将其从调度队列中去除
void thread_exit(struct task_struct* thread_over, bool need_schedule) {
    // 要保证 schedule 在关中断情况下调用
    intr_disable();
    thread_over->status = TASK_DIED;

    // 如果 thread_over不是当前线程, 就有可能还在就绪队列中, 将其从中删除
    if (elem_find(&thread_ready_list, &thread_over->general_tag))
        list_remove(&thread_over->general_tag);

    // 如是进程, 回收进程的页表
    if (thread_over->pgdir)
        mfree_page(PF_KERNEL, thread_over->pgdir, 1);

    // 从 all_thread_list 中去掉此任务
    list_remove(&thread_over->all_list_tag);

    // 回收 pcb 所在的页, 主线程的 pcb 不不在堆中, 跨跨过
    if (thread_over != main_thread)
        mfree_page(PF_KERNEL, thread_over, 1);

    // 归还 pid
    release_pid(thread_over->pid);

    // 如果需要下一轮调度则主动调用 schedule
    if (need_schedule) {
        schedule();
        PANIC("thread_exit: should not be here\n");
    }
}

// 比对任务的 pid
static bool pid_check(struct list_elem* pelem, int32_t pid) {
    struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
    if (pthread->pid == pid)
        return true;
    return false;
}

// 根据 pid 找 pcb 所在的页, 若找到则返回该页, 否则返回 NULL
struct task_struct* pid2thread(int32_t pid) {
    struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
    if (pelem == NULL)
        return NULL;
    struct task_struct* thread = elem2entry(struct task_struct, all_list_tag, pelem);
    return thread;
}

// 初始化线程环境
void thread_init(void) {
    // put_str("- thread_init start...");

    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    // lock_init(&pid_lock);
    pid_pool_init();

    // 先创建第一个用户进程 init
    process_execute(init, "init");

    // 将当前 main 函数创建为线程 
    make_main_thread();

    // 创建idle线程, 空闲线程, 优先级最低, 用于处理空闲时间
    idle_thread = thread_start("idle", 4, idle, NULL);

    // put_str("done!\n");
}