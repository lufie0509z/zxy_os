[bits 32]

; 对于CPU会自动压入错误码的中断类型，无需额外的操作
%define ERROR_CODE nop
; 如果CPU没有压入错误码，为了保持处理逻辑的一致性，我们需要手动压入一个0
%define ZERO push 0

extern put_str
; 中断处理函数数组
extern idt_table

section .data
intr_str db "interrupt occur!", 0xa, 0
global intr_entry_table
intr_entry_table:

; 中断处理程序宏定义
%macro VECTOR 2
section .text
intr%1entry:
    
    %2
    ; 保存上下文
    push ds
    push es
    push fs
    push gs
    pushad

    mov al, 0x20
    out 0xa0, al
    out 0x20, al

    push %1

    ; 调用C的中断处理函数
    call [idt_table + 4 * %1]
    jmp intr_exit

section .data
    dd intr%1entry

%endmacro

section .text
global intr_exit
intr_exit:
    add esp, 4
    popad
    pop gs
    pop fs
    pop es
    pop ds
    add esp, 4
    iretd


VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO 
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO 
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE 
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO 
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO 
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO 
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE 
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO 
VECTOR 0x20,ZERO	;时钟中断对应的入口
VECTOR 0x21,ZERO	;键盘中断对应的入口
VECTOR 0x22,ZERO	;级联用的
VECTOR 0x23,ZERO	;串口2对应的入口
VECTOR 0x24,ZERO	;串口1对应的入口
VECTOR 0x25,ZERO	;并口2对应的入口
VECTOR 0x26,ZERO	;软盘对应的入口
VECTOR 0x27,ZERO	;并口1对应的入口
VECTOR 0x28,ZERO	;实时时钟对应的入口
VECTOR 0x29,ZERO	;重定向
VECTOR 0x2a,ZERO	;保留
VECTOR 0x2b,ZERO	;保留
VECTOR 0x2c,ZERO	;ps/2鼠标
VECTOR 0x2d,ZERO	;fpu浮点单元异常
VECTOR 0x2e,ZERO	;硬盘
VECTOR 0x2f,ZERO	;保留

; 0x80 号中断
; 系统调用
[bits 32]
extern syscall_table ;系统调用中子功能对应的处理函数
section .text
global syscall_handler ;中断例程
syscall_handler:
  push 0

  push ds
  push es
  push fs
  push gs
  pushad  ;其入栈顺序是: EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
  push 0x80

;   ; 为系统调用子功能函数准备参数
;   push edx 
;   push ecx
;   push ebx

;   call [syscall_table + eax * 4] 
;   add esp, 12 ; 跨过上面的三个参数

;   mov [esp + 8 * 4], eax ;将函数返回值写到了栈(此时是内核栈)中保存 eax 的那个内存空间
;   jmp intr_exit

; 利用栈传递参数
; 从内核栈中获取 cpu 自动压入的用户栈指针 esp 的值
; 中断发生后，处理器由低特权进入高特权级，它会把 ss3、esp3、eflag、cs、eip 依次压入栈中，
  mov ebx, [esp + 4 + 48 + 4 + 12] 

  
; 将用户栈空间中的参数读出再压入内核栈中
   push dword [ebx + 12]	   
   push dword [ebx + 8]		    
   push dword [ebx + 4]		   
   mov edx, [ebx]		    ;系统调用的子功能号

   
   call [syscall_table + edx * 4]
   add esp, 12			    ; 跨过上面的三个参数

   mov [esp + 8 * 4], eax	
   jmp intr_exit		    

