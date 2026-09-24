#include "timer.h"
#include "io.h"
// #include "print.h"
#include "thread.h"
#include "debug.h"
#include "interrupt.h"

#define IRQ0_FREQUENCY      100         // 要设置的时钟中断的频率, 我们要将它设为 100Hz。
#define INPUT_FREQUENCY     1193180     // 计数器 0 的工作脉冲信号频率
#define COUNTER0_VALUE      INPUT_FREQUENCY / IRQ0_FREQUENCY    // 计数器 0 的计数初值
#define CONTRER0_PORT       0x40        // 计数器 0 的端口号 0x40
#define COUNTER0_NO         0           // 用在控制字中选择计数器的号码, 其值为 0, 代表计数器 0
#define COUNTER_MODE        2           // 工作模式的代码, 其值为 2, 即方式 2
#define READ_WRITE_LATCH    3           // 读写方式, 其值为 3 表示先读写低 8 位, 再读写高 8 位
#define PIT_CONTROL_PORT    0x43        // 控制字寄存器端口 0x43

#define mil_seconds_per_intr (1000 / IRQ0_FREQUENCY)

uint32_t ticks;          // ticks 是内核自中断开启以来总共的嘀嗒数

/* 
把操作的计数器 counter_no, 读写锁属性 rwl, 
计数器模式 counter_mode 写入模式控制寄存器并赋予初始值 counter_value
此函数定义了五个参数。
    1.counter_port 是计数器的端口号, 用来指定初值 counter_value 的目的端口号。
    2.counter_no 用来在控制字中指定所使用的计数器号码, 对应于控制字中的 SC1 和 SC2 位。
    3.rwl 用来设置计数器的读/写/锁存方式, 对应于控制字中的 RW1 和 RW0 位。
    4.counter_mode 用来设置计数器的工作方式, 对应于控制字中的 M2～M0 位。
    5.counter_value 用来设置计数器的计数初值, 由于此值是 16 位, 所以我们用了 uint16_t 来定义它。
*/
static void frequency_set(uint8_t counter_port, \
			  uint8_t counter_no, \
			  uint8_t rwl, \
			  uint8_t counter_mode, \
			  uint16_t counter_value) {
    // 往控制字寄存器端口0x43中写入控制字    
    outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
    outb(counter_port, (uint8_t)counter_value);          // 先写入低8位
    outb(counter_port, (uint8_t)(counter_value >> 8));   // 再写入高8位
}

// 时钟的中断处理函数
static void intr_timer_handler(void) {
    struct task_struct* cur_thread = running_thread();
    ASSERT(cur_thread->stack_magic == 0x19750130);         // 检查栈是否溢出
    cur_thread->elapsed_ticks++;
    ticks++;

    if (cur_thread->ticks == 0)
        schedule();
    else
        cur_thread->ticks--;
}

// 以 tick 为单位的 sleep,任何时间形式的 sleep 会转换此 ticks 形式
static void ticks_to_sleep(uint32_t sleep_ticks) {
    uint32_t start_tick = ticks;
    // 若间隔的 ticks 数不够便让出 cpu
    while (ticks - start_tick < sleep_ticks) {	   
        thread_yield();
    }
}

// 以毫秒为单位的 sleep   1秒= 1000 毫秒
void mtime_sleep(uint32_t m_seconds) {
    uint32_t sleep_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
    ASSERT(sleep_ticks > 0);
    ticks_to_sleep(sleep_ticks); 
}

// 初始化 PIT8253
void timer_init() {
    // put_str("- timer_init start...");
    // 设置 8253 的定时周期, 也就是发中断的周期
    frequency_set(CONTRER0_PORT, COUNTER0_NO, \
    READ_WRITE_LATCH, COUNTER_MODE, \
    COUNTER0_VALUE);
    register_handler(0x20, intr_timer_handler);
    // put_str("done!\n");
}