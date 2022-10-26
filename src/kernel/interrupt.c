# include <lib/kernel/stdint.h>
# include <kernel/global.h>
# include <kernel/io.h>
# include <kernel/interrupt.h>
# include <lib/kernel/print.h>
# include <kernel/debug.h>

# define IDT_DESC_CNT 0x81 //目前支持的中断数
# define PIC_M_CTRL 0x20
# define PIC_M_DATA 0x21
# define PIC_S_CTRL 0xa0
# define PIC_S_DATA 0xa1

#define EFLAGS_IF 0x00000200 // eflags 寄存器中的 if 位为 1 
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR))

struct gate_desc {
    uint16_t func_offset_low_word;
    uint16_t selector;
    uint8_t dcount;

    uint8_t attribute;
    uint16_t func_offset_high_word;
};

/**
 * 中断的名称.
 */ 
char* intr_name[IDT_DESC_CNT];
intr_handler idt_table[IDT_DESC_CNT];
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static void init_custom_handler_name();
static struct gate_desc idt[IDT_DESC_CNT];

extern intr_handler intr_entry_table[IDT_DESC_CNT];
extern uint32_t syscall_handler(void); //系统调用对应的中断入口程序

/**
 * 通用的中断处理函数.
 */ 
static void general_intr_handler(uint8_t vec_nr) {
    if (vec_nr == 0x27 || vec_nr == 0x2f) {
        // 伪中断，无需处理
        return;
    }
    // put_str("int vector: 0x");
    // put_int(vec_nr);
    // put_char('\n');

    //清理最上方四行四行空间用于打印错误信息
    set_cursor(0);
    int cursor_pos = 0;
    while (cursor_pos < 320)
    {
        put_char(' ');
        cursor_pos++;
    }

    //打印错误信息
    set_cursor(0);
    put_str("!!!!!!!      excetion message begin  !!!!!!!!\n");
    set_cursor(88);
    put_str(intr_name[vec_nr]);
    if (vec_nr == 14) {//缺页中断
        int page_fault_vaddr = 0;
        asm ("movl %%cr2, %0" : "=r" (page_fault_vaddr));	  // cr2是存放造成page_fault的地址
        put_str("\npage fault addr is ");
        put_int(page_fault_vaddr); 
    }
    put_str("\n!!!!!!!      excetion message end    !!!!!!!!\n");
    
    while (1);
    
}

/**
 * 通用(默认)的异常/中断处理器注册.
 */
static void exception_handler_init(void) {
    int i;
    for (i = 0; i < IDT_DESC_CNT; i++) {
        idt_table[i] = general_intr_handler;
        intr_name[i] = "unknown";
    }

    init_custom_handler_name();
}

/**
 * 设置需要自定义的中断名称.
 */ 
static void init_custom_handler_name() {
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
    intr_name[16] = "#MF 0x87 FPU Floating-Point Error";
    intr_name[17] = "#AC Alignment Check Exception";
    intr_name[18] = "#MC Machine-Check Exception";
    intr_name[19] = "#XF SIMD Floating-Point Exception";
}

/**
 * 创建中断门描述符.
 */ 
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
    p_gdesc->func_offset_low_word = (uint32_t) function & 0x0000FFFF;
    p_gdesc->selector = SELECTOR_K_CODE;
    p_gdesc->dcount = 0;
    p_gdesc->attribute = attr;
    p_gdesc->func_offset_high_word = ((uint32_t) function & 0xFFFF0000) >> 16;
}

/**
 * 初始化中断描述符表.
 */ 
static void idt_desc_init(void) {
    int i;
    for (i = 0; i < IDT_DESC_CNT; i++) {
        make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
    }

    //系统调用，需要指定描述符的dpl为用户级
    make_idt_desc(&idt[0x80], IDT_DESC_ATTR_DPL3, syscall_handler); 
    put_str("idt_desc_init done.\n");
}

static void pic_init(void) {
    // //初始化主片
    // outb(PIC_M_CTRL, 0x11);
    // outb(PIC_M_DATA, 0x20);

    // outb(PIC_M_DATA, 0x04);
    // outb(PIC_M_DATA, 0x01);

    // outb(PIC_S_CTRL, 0x11);
    // outb(PIC_S_DATA, 0x28);

    // outb(PIC_S_DATA, 0x02);
    // outb(PIC_S_DATA, 0x01);

    // outb(PIC_M_DATA, 0x0);
    // outb(PIC_S_DATA, 0x0);

    outb(0x20, 0x11);   // Master ICW1, cascade
    outb(0xA0, 0x11);   // Slave  ICW1  cascade

    outb(0x21, 0x20);   // Master ICW2, set begin interrupt number 32
    outb(0xA1, 0x28);   // Slave  ICW2, set begin interrupt number 40

    outb(0x21, 0x04);   // Master ICW3, IR2 connect slave
    outb(0xA1, 0x02);   // Slave  ICW3, connect to master IR2

    outb(0x21, 0x01);   // Master ICW4, 8086 mode, normal EOI
    outb(0xA1, 0x01);   // Slave  ICW4, 8086 mode, normal EOI

    /* Open all IR */
    // outb(0x21, 0xfe);
    // outb(0xA1, 0xff);
    // 打开时钟中断和键盘中断
    outb(0x21, 0xfe);
    outb(0xA1, 0xff);

    put_str("pic_init done.\n");
}


/**
 * 开中断并返回之前的状态.
 */ 
enum intr_status intr_enable() {
    enum intr_status old_status;
    
    if (INTR_ON == intr_get_status()) {
        old_status = INTR_ON;
	    return old_status;
    } else {
        old_status = INTR_OFF;
       
        asm volatile ("sti");
        
        return old_status;
    }
}



/**
 * 关中断并返回之前的状态.
 */
enum intr_status intr_disable() {
    enum intr_status old_status;
    if (INTR_OFF == intr_get_status()) {
        old_status = INTR_OFF;
        return old_status;
    }

    old_status = INTR_ON;
    asm volatile ("cli" : : : "memory");
    return old_status;
}

/**
 * 获取中断状态.
 */ 
enum intr_status intr_get_status() {
    uint32_t eflags = 0;
    GET_EFLAGS(eflags);
    return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

enum intr_status intr_set_status(enum intr_status status) {
    return status & INTR_ON ? intr_enable() : intr_disable();
}

//注册中断处理程序，将中断处理程序数组中第vecor_no个位置设置为函数function
void register_handler(uint8_t vector_no, intr_handler function) {
    idt_table[vector_no] = function;
}



void idt_init() {
    put_str("idt_init start.\n");
    idt_desc_init();
    exception_handler_init();
    pic_init();

    // 加载idt
    uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
    // uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t) ((uint32_t) idt << 16)));
    asm volatile ("lidt %0" : : "m" (idt_operand));
    put_str("idt_init done.\n");
}

