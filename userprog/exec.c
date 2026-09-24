#include "exec.h"
#include "global.h"
#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "fs.h"
#include "memory.h"
#include "list.h"
#include "process.h"


extern void intr_exit(void);
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

// 32 位 elf 头
struct Elf32_Ehdr {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

// 程序头表 Program header, 就是段描述头
struct Elf32_Phdr {
    Elf32_Word p_type;      // 见下面的 enum segment_type
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

// 段类型
enum segment_type {
   PT_NULL,            // 忽略
   PT_LOAD,            // 可加载程序段
   PT_DYNAMIC,         // 动态加载信息 
   PT_INTERP,          // 动态加载器名称
   PT_NOTE,            // 一些辅助信息
   PT_SHLIB,           // 保留
   PT_PHDR             // 程序头表
};

// 将文件描述符 fd 指向的文件中, 偏移为 offset, 大小为 filesz 的段加载到虚拟地址为 vaddr 的内存
static bool segment_load(int32_t fd, uint32_t offset, uint32_t filesz, uint32_t vaddr) {
    uint32_t vaddr_first_page = vaddr & 0xfffff000;    // vaddr 地址所在的页框
    uint32_t size_in_first_page = PG_SIZE - (vaddr & 0x00000fff);     // 加载到内存后, 文件在第一个页框中占用的字节大小
    uint32_t occupy_pages = 0;
    // 若一个页框容不下该段
    if (filesz > size_in_first_page) {
        uint32_t left_size = filesz - size_in_first_page;
        occupy_pages = DIV_ROUND_UP(left_size, PG_SIZE) + 1;    // 1 是指 vaddr_first_page
    } else {
        occupy_pages = 1;
    }

    // 为进程分配内存
    uint32_t page_idx = 0;
    uint32_t vaddr_page = vaddr_first_page;
    while (page_idx < occupy_pages) {
        uint32_t* pde = pde_ptr(vaddr_page);
        uint32_t* pte = pte_ptr(vaddr_page);

        // execv 场景下, 原进程的用户堆 arena 可能恰好在 ELF 段地址上,
        // 必须先释放旧物理页再重新分配, 否则会覆盖 arena 数据
        if ((*pde & 0x00000001) && (*pte & 0x00000001)) {
            mfree_page(PF_USER, (void*)vaddr_page, 1);
        }
        if (get_a_page(PF_USER, vaddr_page) == NULL)
            return false;

        vaddr_page += PG_SIZE;
        page_idx++;
    }
    sys_lseek(fd, offset, SEEK_SET);
    sys_read(fd, (void*)vaddr, filesz); 
    return true;
}

// 从文件系统上加载用户程序 pathname, 成功则返回程序的起始地址, 否则返回-1
static int32_t load(const char* pathname) {
    int32_t ret = -1;
    struct Elf32_Ehdr elf_header;
    struct Elf32_Phdr prog_header;
    memset(&elf_header, 0, sizeof(struct Elf32_Ehdr));

    int32_t fd = sys_open(pathname, O_RDONLY);
    if (fd == -1)
        return -1;

    if (sys_read(fd, &elf_header, sizeof(struct Elf32_Ehdr)) != sizeof(struct Elf32_Ehdr)) {
      ret = -1;
      goto done;
   }

    // 校验 elf 头
    if (memcmp(elf_header.e_ident, "\177ELF\1\1\1", 7) \
        || elf_header.e_type != 2 \
        || elf_header.e_machine != 3 \
        || elf_header.e_version != 1 \
        || elf_header.e_phnum > 1024 \
        || elf_header.e_phentsize != sizeof(struct Elf32_Phdr)) {
        ret = -1;
        goto done;
    }

    Elf32_Off prog_header_offset = elf_header.e_phoff; 
    Elf32_Half prog_header_size = elf_header.e_phentsize;

    // 遍历所有程序头
    uint32_t prog_idx = 0;
    while (prog_idx < elf_header.e_phnum) {
        memset(&prog_header, 0, prog_header_size);
        // 将文件的指针定位到程序头位置
        sys_lseek(fd, prog_header_offset, SEEK_SET);
        // 只获取程序头
        if (sys_read(fd, &prog_header, prog_header_size) != prog_header_size) {
            ret = -1;
            goto done;
        }
        // 如果是可加载段就调用 segment_load 加载到内存
        if (PT_LOAD == prog_header.p_type) {
            if (!segment_load(fd, prog_header.p_offset, prog_header.p_filesz, prog_header.p_vaddr)) {
                ret = -1;
                goto done;
            }
        }
        // 更新下一个程序头的偏移
        prog_header_offset += elf_header.e_phentsize;
        prog_idx++;
    }
    ret = elf_header.e_entry;

done:
    sys_close(fd);
    return ret;
}

// 用 path 指向的程序替换当前进程
int32_t sys_execv(const char* path, const char* argv[]) {
    uint32_t argc = 0;
    while (argv[argc])
        argc++;

    struct task_struct* cur = running_thread();

    /* 这里与书中的源码不同：
    USER_VADDR_START = 0x08048000（用户虚拟地址池起始地址）与 ELF 第一个 PT_LOAD 段的 vaddr 相同。
    fork 后子进程继承了父进程在 0x08048000 处的 arena 映射，execv 的 segment_load 检查 pde/pte 发现已存在，
    跳过 get_a_page，直接用同一个物理页写入 ELF 数据，覆盖了 arena。
    后续 sys_malloc/sys_free 操作被破坏的 arena 导致 #PF。
    */
    
    // execv 重置用户堆: 旧进程的 arena 可能和 ELF 段地址重叠(都从 0x08048000 起),
    // 重置 free_list 避免 sys_malloc/sys_free 操作被覆盖的 arena
    uint16_t desc_idx;
    for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
        list_init(&cur->u_block_desc[desc_idx].free_list);
    }

    int32_t entry_point = load(path);
    if (entry_point == -1)
        return -1;
    
    memcpy(cur->name, path, TASK_NAME_LEN);
    cur->name[TASK_NAME_LEN - 1] = 0;

    // 修改栈中参数: 仿照 start_process 完整初始化 intr_stack
    struct intr_stack* intr_0_stack = (struct intr_stack*)((uint32_t)cur + PG_SIZE - sizeof(struct intr_stack));

    // 通用寄存器(除 ebx/ecx 外)清 0, 避免 popad 后带垃圾值
    intr_0_stack->eax = 0;
    intr_0_stack->edx = 0;
    intr_0_stack->ebp = 0;
    intr_0_stack->esi = 0;
    intr_0_stack->edi = 0;
    intr_0_stack->esp_dummy = 0;

    // ebx=argv, ecx=argc, 供新程序读取
    intr_0_stack->ebx = (int32_t)argv;
    intr_0_stack->ecx = argc;

    // 段寄存器: 与 start_process 保持一致
    intr_0_stack->gs = SELECTOR_U_DATA;
    intr_0_stack->ds = SELECTOR_U_DATA;
    intr_0_stack->es = SELECTOR_U_DATA;
    intr_0_stack->fs = SELECTOR_U_DATA;

    // iretd 必需的 5 个字段
    intr_0_stack->eip = (void*)entry_point;
    intr_0_stack->cs = SELECTOR_U_CODE;
    intr_0_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
    intr_0_stack->esp = (void*)USER_STACK3_VADDR + PG_SIZE;
    intr_0_stack->ss = SELECTOR_U_DATA;

    // exec 不同于 fork, 为使新进程更快被执行, 直接从中断返回
    __asm__ volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (intr_0_stack) : "memory");
    return 0;
}