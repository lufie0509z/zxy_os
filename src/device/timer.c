# include <kernel/io.h>
# include <lib/kernel/print.h>
# include <device/timer.h>
# include<kernel/thread.h>
# include<kernel/interrupt.h>
# include <kernel/debug.h>

# define IRQ0_FREQUENCY 1000
# define INPUT_FREQUENCY 1193180
# define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
# define COUNTER0_PORT 0x40
# define COUNTER_MODE 3
# define COUNTER0_NO 0
# define READ_WRITE_LATCH 3
# define PIT_CONTROL_PORT 0x43

uint32_t ticks;//内核自中断开启以来总共的嘀嗒数

static void frequency_set(uint8_t counter_port,
                          uint8_t counter_no,
                          uint8_t rwl,
                          uint8_t counter_mode,
                          uint16_t counter_value) {
    int divisor = 1193180 / 1000;       /* Calculate our divisor */
    outb(PIT_CONTROL_PORT, (uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));
    outb(counter_port, counter_value);
    
    outb(counter_port, counter_value >> 8);

    //outb(0x43, 0x36);             /* Set our command byte 0x36 */
    // outb(0x40, divisor & 0xFF);   /* Set low byte of divisor */
    // outb(0x40, divisor >> 8);     /* Set high byte of divisor */
    // outb(PIT_CONTROL_PORT, (uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));
    // put_int((uint8_t) (counter_no << 6 | rwl << 4 | counter_mode << 1));

    
    
    // outb(counter_port, 169 );
    // outb(counter_port, 169 >> 8);
    // put_int((uint8_t) (169 >> 8));

    //while (1);
}
//时钟中断处理函数
static void intr_timer_handler(void) {
    struct task_struct* cur_thread = running_thread();
    // put_int(cur_thread->stack_magic);
    ASSERT(cur_thread->stack_magic == 0x20000509);
    cur_thread->elapsed_ticks++;

    ticks++;

    if (cur_thread->ticks == 0) schedule();
    else cur_thread->ticks--;
}


/**
 * 初始化PIT 8253.
 */ 
void timer_init() {
    put_str("timer_init start.\n");
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
    //注册时钟中断函数
    register_handler(0x20, intr_timer_handler);
    put_str("timer_init done.\n");
}