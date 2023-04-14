
if [[ ! -d "../lib" || ! -d "../build" ]]; then
    echo "dependent dir don't exist"
    cwd=$(pwd)
    cwd=${cwd##*/}
    cwd=${cwd%/}
    if [[ $cwd != "user" ]]; then
        echo -e "you'd better in user dir\n"
    fi
    exit
fi



BIN="a"
CFLAGS="-Wall -fno-pie -c -fno-stack-protector -nostdinc -fno-builtin -W \
       -fomit-frame-pointer -Wstrict-prototypes -Wmissing-prototypes -Wsystem-headers -m32"
LIB="../include/"
OBJS="../build/string.o ../build/syscall.o \
      ../build/stdio.o  ../build/assert.o"
DD_IN=$BIN
DD_OUT="../../hd60.img" 


gcc $CFLAGS -I $LIB -o $BIN".o" $BIN".c"
ld -m elf_i386 -e main $BIN".o" $OBJS -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

if [[ -f $BIN ]];then
   dd if=./$DD_IN of=$DD_OUT bs=512 \
   count=$SEC_CNT seek=300 conv=notrunc
fi


gcc $CFLAGS -I $LIB -o $BIN".o" $BIN".c"
ld -m elf_i386 -e main $BIN".o" $OBJS -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')


##########   以上核心就是下面这三条命令   ##########
#gcc -Wall -c -fno-builtin -W -Wstrict-prototypes -Wmissing-prototypes \
#   -Wsystem-headers -I ../lib -o prog_no_arg.o prog_no_arg.c
#ld -e main prog_no_arg.o ../build/string.o ../build/syscall.o\
#   ../build/stdio.o ../build/assert.o -o prog_no_arg
#dd if=prog_no_arg of=/home/work/my_workspace/bochs/hd60M.img \
#   bs=512 count=10 seek=300 conv=notrunc
