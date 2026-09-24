#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define PIC_M_CTRL 0x20     // 主片的控制端口是 0x20
#define PIC_M_DATA 0x21     // 主片的数据端口是 0x21
#define PIC_S_CTRL 0xa0     // 从片的控制端口是 0xa0
#define PIC_S_DATA 0xa1     // 从片的数据端口是 0xa1

#define IDT_DESC_CNT 0x81   // 目前总共支持的中断数 81 个

#define EFLAGS_IF 0x00000200    // eflags 中的 IF 位
// 获取 eflags 寄存器的值
#define GET_EFLAGS(EFLAG_VAR) __asm__ volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))

extern uint32_t syscall_handler(void);

/* 中断门描述符结构体 */
struct gate_desc {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount;
    uint8_t attribute;
    uint16_t func_offset_high_word;
};

static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];  // idt 是中断描述符表

// 用于保存 CPU 异常的名字
char* intr_name[IDT_DESC_CNT];              
// 定义中断处理程序数组, 这里保存的是中断处理函数的地址, 初始全为 general_intr_handler
intr_handler idt_table[IDT_DESC_CNT];
// 声明引用定义在 kernel.s 中的中断处理函数入口数组, 这里保存的是 intr0xXXentry 的地址
extern intr_handler intr_entry_table[IDT_DESC_CNT];

/* 初始化可编程中断控制器 8259A */
static void pic_init(void) {
    /* 初始化主片 */
    outb (PIC_M_CTRL, 0x11);   // ICW1: 边沿触发, 级联 8259, 需要 ICW4.
    outb (PIC_M_DATA, 0x20);   // ICW2: 起始中断向量号为 0x20, 也就是 IR[0-7] 为 0x20 ~ 0x27.
    outb (PIC_M_DATA, 0x04);   // ICW3: IR2 接从片. 
    outb (PIC_M_DATA, 0x01);   // ICW4: 8086 模式, 正常 EOI

    /* 初始化从片 */
    outb (PIC_S_CTRL, 0x11);	// ICW1: 边沿触发, 级联 8259, 需要 ICW4.
    outb (PIC_S_DATA, 0x28);	// ICW2: 起始中断向量号为 0x28, 也就是IR[8-15] 为 0x28 ~ 0x2F.  
    outb (PIC_S_DATA, 0x02);	// ICW3: 设置从片连接到主片的 IR2 引脚
    outb (PIC_S_DATA, 0x01);	// ICW4: 8086 模式, 正常 EOI

    // IRQ2用于级联从片,必须打开,否则无法响应从片上的中断
    // 主片上打开的中断有IRQ0的时钟,IRQ1的键盘和级联从片的IRQ2,其它全部关闭
    outb (PIC_M_DATA, 0xf8);
    // 打开从片上的IRQ14,此引脚接收硬盘控制器的中断
    outb (PIC_S_DATA, 0xbf);

    // 只打开时钟中断, 其它全部关闭
    // outb (PIC_M_DATA, 0xfe);
    // outb (PIC_S_DATA, 0xff);

    // put_str("  * pic_init done\n");
}

/* 创建中断门描述符, 用后面的两个参数构建第 1 个参数指向的中断门描述符 */
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
    p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;

}

/* 初始化中断描述符表 */
static void idt_desc_init(void) {
    int i, lastindex = IDT_DESC_CNT - 1;
    for (i = 0; i < IDT_DESC_CNT; i++)
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    // 单独处理系统调用, 系统调用对应的中断门 dpl 为 3
    make_idt_desc(&idt[lastindex], IDT_DESC_ATTR_DPL3, syscall_handler);
    // put_str("  * idt_desc_init done\n");
}

/* 通用的中断处理函数, 一般用在异常出现时的处理 */
static void general_intr_handler(uint8_t vec_nr) {
    if (vec_nr == 0x27 || vec_nr == 0x2f) {
        //IRQ7 和 IRQ15 会产生伪中断（spurious interrupt）, 无需处理
        //0x2f 是从片 8259A 上的最后一个 IRQ 引脚, 保留项
        return;
    }
    set_cursor(0);
    int cursor_pos = 0;
    while (cursor_pos < 320) {
        put_char(' ');
        cursor_pos++;
    }

    set_cursor(0);	 // 重置光标为屏幕左上角
    put_str("!!!!!!!      excetion message begin  !!!!!!!!\n");
    set_cursor(88); // 从第 2 行第 8 个字符开始打印
    put_str(intr_name[vec_nr]);

    // 若为Pagefault,将缺失的地址打印出来并悬停
    if (vec_nr == 14) {
        int page_fault_vaddr = 0;
        __asm__ ("movl %%cr2, %0" : "=r" (page_fault_vaddr));
        put_str("\npage fault addr is ");
        put_int(page_fault_vaddr);
    }
    put_str("\n!!!!!!!      excetion message end  !!!!!!!!\n");
    // 能进入中断处理程序就表示已经处在关中断情况下,
    // 不会出现调度进程的情况。故下面的死循环不会再被中断
    while(1);
}

/* 完成一般中断处理函数注册及异常名称注册 */
static void exception_init(void) {
    int i;
    for (i = 0; i < IDT_DESC_CNT; i++) {
        idt_table[i] = general_intr_handler;    // 暂时全部设置为 general_intr_handler
        intr_name[i] = "unknown";
    }
    intr_name[0] = "#DE Divide Error";
    intr_name[1] = "#DB Debug Exception";
    intr_name[2] = "NMI Interrupt";
    intr_name[3] = "#BP Breakpoint Exception";
    intr_name[4] = "#OF Overflow Exception";
    intr_name[5] = "#BR BOUND Range Exceeded Exception";
    intr_name[6] = "#UD Invalid Opcode Exception";
    intr_name[7] = "#NM Device Not Available Exception";
    intr_name[8] = "#DF Double Fault Exception";
    intr_name[9] = "Coprocessor Segment Overrun";
    intr_name[10] = "#TS Invalid TSS Exception";
    intr_name[11] = "#NP Segment Not Present";
    intr_name[12] = "#SS Stack Fault Exception";
    intr_name[13] = "#GP General Protection Exception";
    intr_name[14] = "#PF Page-Fault Exception";
    // intr_name[15] 第 15 项是 intel 保留项,未使用
    intr_name[16] = "#MF x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

/* 获取当前中断状态 */
enum intr_status intr_get_status(void) {
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (eflags & EFLAGS_IF) ? INTR_ON : INTR_OFF;
}

/* 开中断并返回开中断前的状态 */
enum intr_status intr_enable(void) {
    enum intr_status old_status = intr_get_status();
    if (old_status == INTR_OFF) {
        __asm__ volatile("sti");
    }
    return old_status;
}

/* 关中断并返回关中断前的状态 */
enum intr_status intr_disable(void) {
    enum intr_status old_status = intr_get_status();
    if (old_status == INTR_ON) {
        __asm__ volatile("cli");
    }
    return old_status;
}

/* 将中断状态设置为 status */
enum intr_status intr_set_status(enum intr_status status) {
    return status & INTR_ON ? intr_enable() : intr_disable();
}

// 在中断处理程序数组第 vector_no 个元素中注册安装中断处理程序 function
void register_handler(uint8_t vector_no, intr_handler function) {
    // idt_table 数组中的函数是在进入中断后根据中断向量号调用的
    // 见 kernel/kernel.S 的 call [idt_table + %1 * 4]
   idt_table[vector_no] = function; 
}

/* 完成有关中断的所有初始化工作 */
void idt_init(void) {
    // put_str_color("- idt_init start.\n", 0x03);
    idt_desc_init();
    exception_init();
    pic_init();

    // 加载idt
    uint64_t idt_operand = ((sizeof(idt) - 1) | (uint64_t)(uint32_t)idt << 16);
    __asm__ volatile("lidt %0" : : "m" (idt_operand));
    // put_str_color("- idt_init done\n", 0x03);
}