# include <kernel/io.h>
# include <lib/kernel/print.h>
# include <device/timer.h>

# define IRQ0_FREQUENCY 1000
# define INPUT_FREQUENCY 1193180
# define COUNTER0_VALUE INPUT_FREQUENCY / IRQ0_FREQUENCY
# define COUNTER0_PORT 0x40
# define COUNTER_MODE 3
# define COUNTER0_NO 0
# define READ_WRITE_LATCH 3
# define PIT_CONTROL_PORT 0x43


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

/**
 * 初始化PIT 8253.
 */ 
void timer_init() {
    put_str("timer_init start.\n");
    frequency_set(COUNTER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
    put_str("timer_init done.\n");
}