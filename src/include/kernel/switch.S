[bits 32] 
section .text 
global switch_to 
switch_to: 
    ;栈中此处是返回地址
    push esi 
    push edi 
    push ebx 
    push ebp 
    mov eax, [esp + 20]; 得到栈中的参数 cur，cur = [esp+20] 
    mov [eax], esp ; 保存栈顶指针 esp.task_struct 的 self_kstack 字段
    
    ;------- 以上是备份当前线程的环境，下面是恢复下一个线程的环境 --------- 
    mov eax, [esp + 24] ; 得到栈中的参数 next，next = [esp+24] 
    mov esp, [eax] ; pcb 的第一个成员是 self_kstack 成员
    pop ebp 
    pop ebx 
    pop edi 
    pop esi 
    ret ; 返回到上面 switch_to 下面的那句注释的返回地址，
   