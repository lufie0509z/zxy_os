#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H

#include <kernel/sync.h>
#include <kernel/thread.h>

#define bufsize 2048

struct ioqueue
{
    struct lock lock; //对缓冲区的访问需要互斥，操作缓冲区前需要先申请锁

    struct task_struct* producer; //缓冲区满时在此缓冲区睡眠的生产者
    struct task_struct* consumer; //缓冲区空时在此缓冲区睡眠的消费者

    char buf[bufsize];
    int32_t head, tail;
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
char ioq_get_char(struct ioqueue* ioq);
void ioq_put_char(struct ioqueue* ioq, char ch);

uint32_t ioq_length(struct ioqueue* ioq);

#endif

