NASM = nasm
LD = ld
AR = ar

OBJECTS = start.o main.o init.o interrupt.o print.o  kernel.o

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
	gcc $(CFLAGS) -I./include -c -o main.o   	main.c 		
	gcc $(CFLAGS) -I./include -c -o interrupt.o kernel/interrupt.c
	gcc $(CFLAGS) -I./include -c -o init.o      kernel/init.c
    
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
