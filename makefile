.DEFAULT_GOAL := all			# 设置默认目标
BUILD_DIR = ./build
DISK_IMG = /home/eric/trueos/hd60M.img
SYSTEM_MAP = build/system.map	# 系统映射文件路径
ENTRY_POINT = 0xc0010000
DEPS = $(OBJS:.o=.d)

AS = nasm
CC = gcc
LD = ld

ASFLAGS = -f elf 
CFLAGS = -m32 -Wall $(LIB) -c -std=c99 -fno-pic -fno-pie -fno-builtin -fno-stack-protector -nostdinc -MMD -MP
LDFLAGS = -m elf_i386 -Ttext $(ENTRY_POINT) -e main -Map $(BUILD_DIR)/kernel.map

LIB = -I lib/ -I lib/kernel/ -I kernel/ -I device/ -I thread/ -I userprog/ -I lib/user/ -I fs/ -I shell/

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/interrupt.o \
      $(BUILD_DIR)/init.o $(BUILD_DIR)/timer.o $(BUILD_DIR)/print.o $(BUILD_DIR)/debug.o \
	  $(BUILD_DIR)/string.o $(BUILD_DIR)/bitmap.o $(BUILD_DIR)/memory.o \
	  $(BUILD_DIR)/thread.o $(BUILD_DIR)/list.o $(BUILD_DIR)/switch.o  \
	  $(BUILD_DIR)/console.o $(BUILD_DIR)/sync.o $(BUILD_DIR)/keyboard.o \
	  $(BUILD_DIR)/ioqueue.o $(BUILD_DIR)/tss.o $(BUILD_DIR)/process.o \
	  $(BUILD_DIR)/syscall.o $(BUILD_DIR)/syscall-init.o $(BUILD_DIR)/stdio.o \
	  $(BUILD_DIR)/printk.o $(BUILD_DIR)/ide.o $(BUILD_DIR)/file.o $(BUILD_DIR)/dir.o \
	  $(BUILD_DIR)/fs.o $(BUILD_DIR)/inode.o $(BUILD_DIR)/fork.o $(BUILD_DIR)/assert.o \
	  $(BUILD_DIR)/shell.o $(BUILD_DIR)/buildin_cmd.o $(BUILD_DIR)/exec.o \
	  $(BUILD_DIR)/wait_exit.o $(BUILD_DIR)/pipe.o

-include $(DEPS)

# -------------------------------------------
# boot
$(BUILD_DIR)/mbr.bin: boot/mbr.s 
	$(AS) $< -o $@
$(BUILD_DIR)/loader.bin: boot/loader.s 
	$(AS) $< -o $@

# -------------------------------------------
# kernel 中的汇编代码
$(BUILD_DIR)/kernel.o: kernel/kernel.s
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/print.o: lib/kernel/print.s
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD_DIR)/switch.o: thread/switch.s
	$(AS) $(ASFLAGS) $< -o $@

# -------------------------------------------
# kernel 中的 c 代码
$(BUILD_DIR)/main.o: kernel/main.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/init.o: kernel/init.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/timer.o: device/timer.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/debug.o: kernel/debug.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/string.o: lib/string.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/memory.o: kernel/memory.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/thread.o: thread/thread.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/sync.o: thread/sync.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/console.o: device/console.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/keyboard.o: device/keyboard.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ioqueue.o: device/ioqueue.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/tss.o: userprog/tss.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/process.o: userprog/process.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall.o: lib/user/syscall.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/syscall-init.o: userprog/syscall-init.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/stdio.o: lib/stdio.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/printk.o: lib/kernel/printk.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/ide.o: device/ide.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fs.o: fs/fs.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/file.o: fs/file.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/dir.o: fs/dir.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/inode.o: fs/inode.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/fork.o: userprog/fork.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/assert.o: lib/user/assert.c
	$(CC) $(CFLAGS) $< -o $@
	
$(BUILD_DIR)/shell.o: shell/shell.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/buildin_cmd.o: shell/buildin_cmd.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/exec.o: userprog/exec.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/wait_exit.o: userprog/wait_exit.c
	$(CC) $(CFLAGS) $< -o $@

$(BUILD_DIR)/pipe.o: shell/pipe.c
	$(CC) $(CFLAGS) $< -o $@

# -------------------------------------------
# 链接
$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

# 生成 system.map
$(SYSTEM_MAP): $(BUILD_DIR)/kernel.bin
	@nm "$(BUILD_DIR)/kernel.bin" | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)' | sort > $@

.PHONY : disk clean build all

disk:
	dd if=$(BUILD_DIR)/mbr.bin of=$(DISK_IMG) bs=512 count=1  conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=$(DISK_IMG) bs=512 count=4 seek=2 conv=notrunc
	dd if=$(BUILD_DIR)/kernel.bin of=$(DISK_IMG) bs=512 count=200 seek=9 conv=notrunc
	@echo "-------- Write disk done -----------------"

clean:
	@find "build" -type f \( -name '*.o' -o -name '*.bin' -o -name '*.d' \) -print -exec rm -f {} +
	@echo "* Clean done."

build: $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin $(BUILD_DIR)/kernel.bin
	@echo "---- Build done. -------------------------"

all: build disk $(SYSTEM_MAP)
	@echo "* Good!!! compile done."
	@echo "* System.map: $(SYSTEM_MAP)"
	@echo "* Please run: bochs -f ~/trueos/bochsrc.disk"