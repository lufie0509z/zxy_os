#ifndef _DEVICE_TIMER_H
#define _DEVICE_TIMER_H
#include <kernel/global.h>
void timer_init();

void mtime_sleep(uint32_t m_seconds);

#endif