#include <user/syscall.h>
#include <kernel/thread.h>
#include <lib/kernel/stdio-kernel.h>

/**
 * 无参数的系统调用 
 * eax 既用来保存子功能号，又作为函数调用的返回值
 */ 
#define _syscall0(NUMBER) ({			   \
   int retval;					           \
   asm volatile (					       \
   "int $0x80"						       \
   : "=a" (retval)					       \
   : "a" (NUMBER)					       \
   : "memory"						       \
   );							           \
   retval;						           \
})

// 一个参数的系统调用 
#define _syscall1(NUMBER, ARG1) ({		  \
   int retval;					          \
   asm volatile (					      \
   "int $0x80"						      \
   : "=a" (retval)					      \
   : "a" (NUMBER), "b" (ARG1)   	      \
   : "memory"						      \
   );							          \
   retval;						          \
})

// 两个参数的系统调用 
#define _syscall2(NUMBER, ARG1, ARG2) ({	\
   int retval;						        \
   asm volatile (					        \
   "int $0x80"						        \
   : "=a" (retval)					        \
   : "a" (NUMBER), "b" (ARG1), "c" (ARG2)   \
   : "memory"						        \
   );							            \
   retval;						            \
})

// 三个参数的系统调用 
#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({		        \
   int retval;						                        \
   asm volatile (					                        \
      "int $0x80"					                        \
      : "=a" (retval)					                    \
      : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3)    \
      : "memory"					                        \
   );							                            \
   retval;						                            \
})

// /**
//  * 利用栈传递参数
//  */
// #define _syscall0(NUMBER) ({				       \
//    int retval;					                   \
//    asm volatile (					               \
//    "pushl %[number]; int $0x80; addl $4, %%esp"	   \
//    : "=a" (retval)					               \
//    : [number] "i" (NUMBER)		  		           \
//    : "memory"						               \
//    );							                   \
//    retval;						                   \
// })

// #define _syscall1(NUMBER, ARG0) ({			                     \
//    int retval;					                                 \
//    asm volatile (					                             \
//    "pushl %[arg0]; pushl %[number]; int $0x80; addl $8, %%esp"   \
//    : "=a" (retval)					                             \
//    : [number] "i" (NUMBER), [arg0] "g" (ARG0)		             \
//    : "memory"						                             \
//    );							                                 \
//    retval;						                                 \
// })

// #define _syscall2(NUMBER, ARG0, ARG1) ({		       \
//    int retval;						                   \
//    asm volatile (					                   \
//    "pushl %[arg1]; pushl %[arg0]; "			           \
//    "pushl %[number]; int $0x80; addl $12, %%esp"	   \
//     : "=a" (retval)					                   \
//     : [number] "i" (NUMBER),				           \
// 	  [arg0] "g" (ARG0),				               \
// 	  [arg1] "g" (ARG1)				                   \
//     : "memory"					                       \
//    );							                       \
//    retval;						                       \
// })

// #define _syscall3(NUMBER, ARG0, ARG1, ARG2) ({		   \
//    int retval;						                   \
//    asm volatile (					                   \
//     "pushl %[arg2]; pushl %[arg1]; pushl %[arg0]; "    \
//     "pushl %[number]; int $0x80; addl $16, %%esp"	   \
//     : "=a" (retval)					                   \
//     : [number] "i" (NUMBER),				           \
// 	  [arg0] "g" (ARG0),				               \
// 	  [arg1] "g" (ARG1),				               \
// 	  [arg2] "g" (ARG2)				                   \
//     : "memory"					                       \
//    );							                       \
//    retval;						                       \
// })



uint32_t getpid(void) {
    return _syscall0(SYS_GETPID);
}

uint32_t write(int32_t fd, const void* buf, uint32_t cnt) {
    return _syscall3(SYS_WRITE, fd, buf, cnt);
}

void* malloc(uint32_t size) {
   return (void*)_syscall1(SYS_MALLOC, size);
}
void free(void* ptr) {
   _syscall1(SYS_FREE, ptr);
}

pid_t fork() {
   return _syscall0(SYS_FORK);
}

int32_t read(int32_t fd, void* buf, uint32_t cnt) {
   _syscall3(SYS_READ, fd, buf, cnt);
}

void putchar(char char_asci) {
   _syscall1(SYS_PUTCHAR, char_asci);
}

void clear() {
   _syscall0(SYS_CLEAR);
}