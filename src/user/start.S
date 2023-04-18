[bits 32]
extern main
extern exit
section .text
global _start
_start:
    push ebx    ; 压入 argv
    push ecx    ; 压入 argc
    call main

    push eax    ; 将 main 的返回值通过栈传参给 exit
    call exit   ; exit(eax) exit 不会返回
