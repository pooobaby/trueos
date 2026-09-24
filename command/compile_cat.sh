#!/bin/bash
####  此脚本应该在command目录下执行

if [[ ! -d "../lib" || ! -d "../build" ]];then
   echo "dependent dir don\`t exist!"
   cwd=$(pwd)
   cwd=${cwd##*/}
   cwd=${cwd%/}
   if [[ $cwd != "command" ]];then
      echo -e "you\`d better in command dir\n"
   fi 
   exit
fi

BIN="cat"
#  GCC 默认开了 -fstack-protector 选项，需要关闭它
# 会在每个函数 prologue/epilogue 插入从 TCB（Thread Control Block） 读取 canary 值的代码。
# 在 Linux x86 上，%gs 寄存器专门指向 TCB（TLS 区域），偏移 0x14 就是 canary 值。
# 但我们的 TrueOS 没有 TCB/TLS 支持，所以任何 stack-protector 生成的 %gs 访问都会崩溃：
CFLAGS="-m32 -Wall -c -fno-builtin -W -Wstrict-prototypes \
      -Wmissing-prototypes -Wsystem-headers -fno-stack-protector"
LIBS="-I ../lib/ -I ../lib/kernel/ -I ../lib/user/ -I \
      ../kernel/ -I ../device/ -I ../thread/ -I \
      ../userprog/ -I ../fs/ -I ../shell/"
OBJS="../build/string.o ../build/syscall.o \
      ../build/stdio.o ../build/assert.o start.o"
DD_IN=$BIN
DD_OUT="/home/eric/trueos/hd60M.img" 

nasm -f elf ./start.s -o start.o
ar rcs simple_crt.a $OBJS start.o
gcc $CFLAGS $LIBS -o $BIN".o" $BIN".c"
ld -m elf_i386 $BIN".o" simple_crt.a -o $BIN
SEC_CNT=$(ls -l $BIN|awk '{printf("%d", ($5+511)/512)}')

if [[ -f $BIN ]];then
   dd if=./$DD_IN of=$DD_OUT bs=512 \
   count=$SEC_CNT seek=900 conv=notrunc
fi

