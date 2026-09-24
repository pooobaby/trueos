#include "sync.h"
#include "global.h"
#include "thread.h"
#include "list.h"
#include "debug.h"
#include "interrupt.h"

// 初始化信号量
void sema_init(struct semaphore* psema, uint8_t value) {
    psema->value = value;           // 为信号量赋初值
    list_init(&psema->waiters);     // 初始化等待队列
}

// 初始化锁 plock
void lock_init(struct lock* plock) {
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1);    // 信号量初值为1
}

// 信号量 down 操作 - P 操作
void sema_down(struct semaphore* psema) {
    enum intr_status old_status = intr_disable();
    // 若 value 为 0, 表示已经被别人持有
    while(psema->value == 0) {
        // 防御性编程：当前线程不应该已在信号量的 waiters 队列中
        ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
        if (elem_find(&psema->waiters, &running_thread()->general_tag))
	        PANIC("sema_down: thread blocked has been in waiters_list\n");
        
        // 若信号量的值等于 0, 则当前线程把自己加入该锁的等待队列, 然后阻塞自己
        list_append(&psema->waiters, &running_thread()->general_tag);
        thread_block(TASK_BLOCKED);
    }
    // 若 value 为 1 或被唤醒后, 会执行下面的代码, 也就是获得了锁
    psema->value--;
    ASSERT(psema->value == 0);
    intr_set_status(old_status);
}

// 信号量的 up 操作 - V 操作
void sema_up(struct semaphore* psema) {
    enum intr_status old_status = intr_disable();
    ASSERT(psema->value == 0);
    if (!list_empty(&psema->waiters)) {
        struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, list_pop(&psema->waiters));
        thread_unblock(thread_blocked);
    }
    psema->value++;
    ASSERT(psema->value == 1);
    intr_set_status(old_status);
}

// 获取锁 plock
void lock_acquire(struct lock* plock) {
    if (plock->holder != running_thread()) {
        // 如果当前线程不是锁的持有者, 则对信号量 P 操作, 原子操作
        // enum intr_status old_status = intr_disable();
        sema_down(&plock->semaphore);
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
        // intr_set_status(old_status);
    } else {
        plock->holder_repeat_nr++;
    }
}

// 释放锁 plock
void lock_release(struct lock* plock) {
    ASSERT(plock->holder == running_thread());
    if (plock->holder_repeat_nr > 1) {
        plock->holder_repeat_nr--;
        return;
    }
    ASSERT(plock->holder_repeat_nr == 1);

    plock->holder = NULL;	                // 把锁的持有者置空放在V操作之前
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);	   // 信号量的 V 操作,也是原子操作
}
