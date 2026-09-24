#include "process.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"    
#include "list.h"    
#include "tss.h"    
#include "interrupt.h"
#include "string.h"
#include "console.h"
#include "stdint.h"

extern void intr_exit(void);

// 构建用户进程初始上下文信息
void start_process(void* filename_) {
    void* function = filename_;
    struct task_struct* cur = running_thread();
    // 跨过 thread_stack, 指向 intr_stack
    cur->self_kstack = (uint32_t*)((uint32_t)cur->self_kstack + sizeof(struct thread_stack));
    struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;
    proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
    proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
    // proc_stack->gs = 0;  // 不太允许用户态直接访问显存资源, 用户态用不上, 直接初始为 0
    // gs 也设为 SELECTOR_U_DATA，不能设为 0（空选择子会在用户态访问 GS 段前缀指令时触发 GP）
    // 内核态 gs 指向显存，用户态不使用，统一用 SELECTOR_U_DATA（覆盖整个 4GB 空间）
    proc_stack->gs = SELECTOR_U_DATA;
    proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
    proc_stack->eip = function; // 待执行的用户程序地址

    // put_str("eip: 0x");
    // put_int((uint32_t)proc_stack->eip);

    proc_stack->cs = SELECTOR_U_CODE;
    proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE);

    // put_str("  esp: 0x");
    // put_int((uint32_t)proc_stack->esp);
    // put_str("  phy: 0x");
    // put_int(addr_v2p(USER_STACK3_VADDR));
    // put_str("  kstack_top: 0x");
    // put_int((uint32_t)cur + PG_SIZE);
    // put_str("\n");
    
    proc_stack->ss = SELECTOR_U_DATA; 
    __asm__ volatile ("mov %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
}

// 激活页表
// 执行此函数时,当前任务可能是线程。
// 之所以对线程也要重新安装页表, 原因是上一次被调度的可能是进程
// 否则不恢复页表的话,线程就会使用进程的页表了。
void page_dir_activate(struct task_struct* p_thread) {
    // 若为内核线程, 需要重新填充页表为 0x100000
    uint32_t pagedir_phy_addr = 0x100000;   // 默认为内核的页目录物理地址,也就是内核线程所用的页目录表
    if (p_thread->pgdir != NULL) {
        pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
    }
    __asm__ volatile ("mov %0, %%cr3" : : "r" (pagedir_phy_addr) : "memory");
}

// 激活线程或进程的页表, 更新tss中的esp0为进程的特权级 0 的栈
void process_activate(struct task_struct* p_thread) {
    ASSERT(p_thread != NULL);
    page_dir_activate(p_thread);    // 激活该进程或线程的页表
    // 内核线程特权级本身就是0特权级, 处理器进入中断时并不会从tss中获取 0 特权级栈地址, 故不需要更新 esp0
    if (p_thread->pgdir != NULL)    // 是进程才更新 TSS.esp0
        update_tss_esp(p_thread);
}

// 创建页目录表, 将当前页表的表示内核空间的 pde 复制, 成功则返回页目录的虚拟地址,否则返回-1
uint32_t* create_page_dir(void) {
    // 用户进程的页表不能让用户直接访问到,所以在内核空间来申请
    uint32_t* page_dir_vaddr = get_kernel_pages(1);
    
    if (page_dir_vaddr == NULL) {
        console_put_str("create_page_dir: get_kernel_page failed!");
        return NULL;
    }

    // 1-先复制页表 PDE[768..1023]
    memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300 * 4), (uint32_t*)(0xFFFFF000 + 0x300 * 4), 1024);

    // 2-更新页目录地址
    uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
    // 页目录地址是存入在页目录的最后一项, 更新页目录地址为新页目录的物理地址
    page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;

    // 下面将页目录表项 PDE[0] 复制给用户进程后, 用户进程就可以访问到 0~4MB 的内存了, 不大安全
    // page_dir_vaddr[0] = *((uint32_t*)0xFFFFF000);   // 复制 PDE[0], 把 0~4MB 也映射进来

    return page_dir_vaddr;
}

// 创建用户进程虚拟地址位图
void create_user_vaddr_bitmap(struct task_struct* user_prog) {
    user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;   // 0x0804_8000
    uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xC0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
    user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
    user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xC0000000 - USER_VADDR_START) / PG_SIZE / 8;
    bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

void process_execute(void* filename, char* name) {
    // pcb 内核的数据结构,由内核来维护进程信息,因此要在内核内存池中申请
    struct task_struct* thread = get_kernel_pages(1);
    init_thread(thread, name, default_prio);    // pgdir 仍为 NULL（暂时）
    create_user_vaddr_bitmap(thread);
    thread_create(thread, start_process, filename); // start_process(filename)
    thread->pgdir = create_page_dir();   // ★ 从此 pgdir != NULL, 标志着它是"进程"

    block_desc_init(thread->u_block_desc);   // 初始化用户进程的内存块描述符数组

    enum intr_status old_status = intr_disable();
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);

    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);
    intr_set_status(old_status);
}


