#include <device/ioqueue.h>
#include <kernel/sync.h>
#include <kernel/global.h>
#include <kernel/interrupt.h>
#include <kernel/debug.h>
#include <kernel/thread.h>

void ioqueue_init(struct ioqueue* ioq) {
    lock_init(&ioq->lock);
    ioq->producer = ioq->consumer = NULL;
    ioq->head = ioq->tail = 0;
}

static uint32_t next_pos(uint32_t pos) {
    return (pos + 1) % bufsize;
}

bool ioq_full(struct ioqueue* ioq) {
    ASSERT(intr_get_status() == INTR_OFF);
    return (next_pos(ioq->head) == ioq->tail);
}

bool ioq_empty(struct ioqueue* ioq) {
    ASSERT(intr_get_status() == INTR_OFF);
    return ioq->head == ioq->tail;
}

//使生产者或者消费者在该缓冲区上等待，传入的参数是线程指针的地址
static void ioq_wait(struct task_struct** waiter) {
    ASSERT(*waiter == NULL && waiter != NULL);
    *waiter = running_thread(); //将当前线程记录在缓冲区的 producer 或者 consumer 中
    thread_block(TASK_BLOCKED);
}

static void wakeup (struct task_struct** waiter) {
    ASSERT(*waiter != NULL);
    thread_unblock(*waiter);
    *waiter = NULL;
}


//消费者从缓冲区中取一个字符
char ioq_get_char(struct ioqueue* ioq) {
    ASSERT(intr_get_status() == INTR_OFF);

    //缓冲区为空
    while (ioq_empty(ioq))
    {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->consumer);
        lock_release(&ioq->lock);
    }

    char ch = ioq->buf[ioq->tail];
    ioq->tail = next_pos(ioq->tail);
    
    //唤醒生产者
    if (ioq->producer != NULL) {
        wakeup(&ioq->producer);
    }

    return ch;
}

//生产者往缓冲区中加入一个字符
void ioq_put_char(struct ioqueue* ioq, char ch) {
    ASSERT(intr_get_status() == INTR_OFF);

    //缓冲区已经满了
    while (ioq_full(ioq)) {
        lock_acquire(&ioq->lock);
        ioq_wait(&ioq->producer);
        lock_release(&ioq->lock);
    }

    ioq->buf[ioq->head] = ch;
    ioq->head = next_pos(ioq->head);

    //唤醒消费者
    if (ioq->consumer != NULL) {
        wakeup(&ioq->consumer);
    }

}

// 返回环形缓冲区的长度
uint32_t ioq_length(struct ioqueue* ioq) {
    uint32_t len = 0;
    if (ioq->head >= ioq->tail) {
        len = ioq->head - ioq->tail;
    } else {
        len = bufsize - (ioq->tail - ioq->head);
    }
    return len;
}