#include <kernel/sync.h>
#include <kernel/list.h>
#include <kernel/global.h>
#include <kernel/debug.h>
#include <kernel/interrupt.h>
#include <lib/kernel/print.h>

void sema_init(struct semaphore* psema, uint8_t value) {
    psema->value = value;
    list_init(&psema->waiters);
}

void lock_init(struct lock* plock) {
    plock->holder = NULL;
    plock->holder_repeat_nr = 0;
    sema_init(&plock->semaphore, 1);
}

//信号量减少
void sema_down(struct semaphore* psema) {
    //原子操作，关中断
    enum intr_status old_status = intr_disable();

    while (psema->value == 0) {//锁还在被别人持有

        ASSERT(!elem_find(&psema->waiters, &running_thread()->general_tag));
        if (elem_find(&psema->waiters, &running_thread()->general_tag)) {
            PANIC("sema down: blocked thread has been in the waiter_lists");
        }

        list_append(&psema->waiters, &running_thread()->general_tag);
        thread_block(TASK_BLOCKED);
    }
    psema->value--;
    ASSERT(psema->value == 0);  
    
    intr_set_status(old_status);
}

//信号量增加
void sema_up(struct semaphore* psema) {
    enum intr_status old_status = intr_disable();

    ASSERT(psema->value == 0);
    if (!list_empty(&psema->waiters)) {
        struct list_elem* node = list_pop(&psema->waiters);
        struct task_struct* thread_blocked = elem2entry(struct task_struct, general_tag, node);
        thread_unblock(thread_blocked);
    }
    
    psema->value++;
    ASSERT(psema->value == 1);
    intr_set_status(old_status);
}

void lock_acquire(struct lock* plock) {
    if (plock->holder != running_thread()) {
        sema_down(&plock->semaphore);
        plock->holder = running_thread();
        ASSERT(plock->holder_repeat_nr == 0);
        plock->holder_repeat_nr = 1;
    } else {
        plock->holder_repeat_nr++;
    }
}

void lock_release(struct lock* plock) {
    ASSERT(plock->holder == running_thread());
    if (plock->holder_repeat_nr > 1) {
        plock->holder_repeat_nr--;
        return;
    }

    ASSERT(plock->holder_repeat_nr == 1);
    plock->holder = NULL; //先将锁的持有者置空再操作信号量
    plock->holder_repeat_nr = 0;
    sema_up(&plock->semaphore);
}
