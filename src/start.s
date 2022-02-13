; This is the kernel's entry point.
[BITS 32]
global __start

__start:
    mov esp, 0xc009f000     ; This points the stack to our new stack area
    jmp stublet


 
    ; EXTERN code, bss, end
    
    ; ; AOUT kludge - must be physical addresses. Make a note of these:
    ; ; The linker script fills in the data for these ones!
    ; dd code
    ; dd bss
    ; dd end
    dd __start


stublet:
    extern main
    call main
    jmp $