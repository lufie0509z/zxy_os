NASM = nasm
LD = ld
AR = ar

OBJECTS = start.o main.o init.o interrupt.o print.o  kernel.o timer.o debug.o string.o bitmap.o \
          memory.o thread.o list.o switch.o console.o sync.o keyboard.o

CFLAGS = -Wall -fno-pie -O -fstrength-reduce -fomit-frame-pointer \
		 -finline-functions -nostdinc -fno-builtin  -fno-stack-protector -m32
		 

all: kernel.bin kernel.elf libkernel.a
	mv *.o *.a ./build/

%.o: %.s
	mkdir build
	mkdir build/user/
	$(NASM) -f elf -o start.o start.s
	$(NASM) -f elf -o print.o lib/kernel/print.s
	$(NASM) -f elf -o kernel.o kernel/kernel.S
	$(NASM) -f elf -o switch.o kernel/switch.s
	gcc $(CFLAGS) -I./include -c -o main.o   	main.c 		
	gcc $(CFLAGS) -I./include -c -o interrupt.o kernel/interrupt.c
	gcc $(CFLAGS) -I./include -c -o init.o      kernel/init.c
	gcc $(CFLAGS) -I./include -c -o timer.o     device/timer.c
	gcc $(CFLAGS) -I./include -c -o debug.o     kernel/debug.c
	gcc $(CFLAGS) -I./include -c -o string.o    kernel/string.c
	gcc $(CFLAGS) -I./include -c -o bitmap.o    lib/kernel/bitmap.c
	gcc $(CFLAGS) -I./include -c -o memory.o    kernel/memory.c
	gcc $(CFLAGS) -I./include -c -o thread.o   	kernel/thread.c 
	gcc $(CFLAGS) -I./include -c -o list.o   	lib/kernel/list.c  
	gcc $(CFLAGS) -I./include -c -o sync.o  	kernel/sync.c 
	gcc $(CFLAGS) -I./include -c -o console.o   device/console.c
	gcc $(CFLAGS) -I./include -c -o keyboard.o  device/keyboard.c

libkernel.a: $(OBJECTS)
	$(AR) -r libkernel.a $(OBJECTS)

kernel.bin: $(OBJECTS)
	$(LD) -m elf_i386 -T link.ld -o ../bin/kernel.bin $(OBJECTS)

kernel.elf: $(OBJECTS)
	$(LD) -m elf_i386 -T link_elf.ld -o kernel.elf $(OBJECTS)
	objdump -d kernel.elf > kernel.s

clean:
	rm kernel.elf kernel.s
	rm -r build

beauty:
	astyle  --style=google --recursive "*.c,*.h" -n

done: echo "DONE!"
