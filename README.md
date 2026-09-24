# TrueOS —— x86 32位教学级操作系统

> 作者：Eric Cui  
> 基于《操作系统真象还原》教程从零实现的 x86 架构迷你操作系统，运行在 Bochs PC 模拟器上。

---

## ✨ 项目特性

TrueOS 是一个完整的教学级操作系统内核，覆盖了从引导启动到 Shell 用户态的全链路实现：

- 🔄 **引导流程**：MBR → Loader → 内核三段式启动，支持从实模式到保护模式的切换
- 💾 **内存管理**：基于位图的物理内存分页分配 + 基于 arena 的 slob 小块内存管理
- 🧵 **线程调度**：优先级调度 + 时间片轮转，支持内核线程和用户进程
- 🔒 **进程同步**：信号量、锁、条件变量
- 💥 **中断处理**：IDT 中断描述符表，支持时钟、键盘等外设中断
- 📂 **文件系统**：自实现简化文件系统（超级块 + inode + 数据块 + 位图），支持目录、文件创建/读写
- 👤 **用户进程**：`fork` / `execv` / `wait` / `exit` 完整生命周期
- 🖥️ **Shell**：支持内置命令、程序执行、管道
- ⚙️ **系统调用**：通过 `int 0x80` 实现 open/read/write/close/malloc/free/... 等

---

## 🗂️ 目录结构

```
c00/
├── boot/                # 引导程序（NASM 汇编）
│   ├── mbr.s           # 主引导记录（512 字节，加载 Loader）
│   └── loader.s        # Loader（进入保护模式，设置 GDT/IDT/页表，加载内核）
├── kernel/             # 内核核心
│   ├── main.c          # 内核入口 main()
│   ├── init.c          # 系统初始化 init_all()
│   ├── interrupt.c    # 中断处理框架 + int 0x80 系统调用入口
│   ├── memory.c        # 内存管理（物理页分配、虚拟地址池）
│   ├── kernel.s        # 中断 / 异常向量汇编入口
│   ├── global.h        # 全局宏、GDT/IDT/TSS 描述符属性定义
│   └── debug.c         # 调试辅助
├── thread/             # 线程与同步
│   ├── thread.c        # 线程创建、调度、上下文切换
│   ├── switch.s        # 线程切换汇编（保存/恢复寄存器）
│   └── sync.c          # 信号量、锁、条件变量
├── device/             # 设备驱动
│   ├── timer.c         # PIT 可编程间隔定时器
│   ├── keyboard.c      # PS/2 键盘驱动
│   ├── console.c       # 控制台输出
│   ├── ide.c           # IDE 硬盘驱动（PIO 模式）
│   └── ioqueue.c       # 键盘/串口 IO 队列
├── userprog/           # 用户进程管理
│   ├── process.c       # PCB、进程激活、地址空间
│   ├── fork.c          # fork() 实现（复制 PCB + 复制页表）
│   ├── exec.c          # execv() 实现（加载 ELF 可执行文件）
│   ├── wait_exit.c     # wait() / exit() 僵尸进程回收
│   ├── tss.c           # TSS（任务状态段）管理
│   └── syscall-init.c  # 系统调用注册表
├── fs/                 # 文件系统
│   ├── fs.c            # 文件系统挂载、路径解析、open
│   ├── file.c          # 文件操作、inode 位图、数据块分配
│   ├── dir.c           # 目录操作、目录项
│   ├── inode.c         # inode 内存缓存与磁盘同步
│   └── super_block.h   # 超级块结构体
├── shell/              # Shell 与用户态
│   ├── shell.c         # Shell 主循环、命令解析
│   ├── buildin_cmd.c   # 内置命令（ls / cd / mkdir ...）
│   └── pipe.c          # 管道实现
├── command/            # 用户态应用程序
│   ├── start.s         # C 运行时启动入口
│   ├── cat.c           # cat 命令
│   ├── prog_pipe.c     # 管道示例程序
│   ├── prog_arg.c      # 带参数程序示例
│   └── prog_no_arg.c   # 无参数程序示例
├── lib/                # 库函数
│   ├── kernel/         # 内核态库
│   │   ├── printk.c    # 内核 printk
│   │   ├── print.s     # 屏幕输出汇编
│   │   ├── list.c      # 双向链表
│   │   └── bitmap.c    # 位图操作
│   ├── user/           # 用户态库
│   │   ├── syscall.c   # 系统调用包装
│   │   └── assert.c
│   ├── stdio.c         # 用户态 printf
│   └── string.c        # 字符串/内存操作
├── tools/              # 开发辅助工具
│   ├── run_loader      # 一键构建并启动 Bochs 的 Shell 脚本
│   ├── compile_flags.txt # clangd / VSCode 用的编译参数提示
│   └── bochsrc.disk    # Bochs 模拟器配置文件（含双硬盘）
├── build/              # 编译输出目录
├── makefile            # 完整构建脚本
└── README.md

### 项目根目录（/home/eric/trueos/）

```
trueos/
├── code/c00/           # 本仓库（内核源码）
├── bochs/              # Bochs 安装目录（含 bin/bochs 和 BIOS）
├── hd60M.img           # 60MB 主硬盘镜像（ATA master）
└── hd80M.img           # 80MB 从硬盘镜像（ATA slave，供文件系统实验用）
```

### 磁盘镜像

| 文件 | 大小 | Bochs 角色 | 用途 |
|------|------|------------|------|
| `hd60M.img` | 60 MB | `ata0-master` | 存放 MBR、Loader、内核二进制 |
| `hd80M.img` | 80 MB | `ata0-slave` | 从硬盘，供 IDE 驱动读写测试 |
```

---

## 🛠️ 构建与运行

### 前置工具

- **nasm** — 汇编器
- **gcc** — C 编译器（需支持 `-m32`）
- **ld** — 链接器
- **bochs** — x86 PC 模拟器（带调试器）
- **dd** — 写磁盘镜像

### 一键构建（makefile 完整版）

```bash
cd /home/eric/trueos/code/c00
make          # 编译全部模块 + 写磁盘镜像
```

### 一键构建（run_loader 轻量版）

项目还提供了一个简化版的构建脚本 `tools/run_loader`，适合早期调试（只编译 boot + kernel 核心模块）：

```bash
cd /home/eric/trueos
./code/c00/tools/run_loader      # 构建并自动启动 Bochs
./code/c00/tools/run_loader clean # 仅清理 build/ 下的 .o 和 .bin
```

构建会依次完成：
1. `boot/mbr.s` → `build/mbr.bin`
2. `boot/loader.s` → `build/loader.bin`
3. 所有 `.c` + `.s` → `build/kernel.bin`（入口 `0xC0010000`）
4. 将三者写入磁盘镜像 `hd60M.img`
5. 生成 `build/system.map` 符号表

### 运行

```bash
# 方式一：用项目自带的 bochsrc.disk
bochs -f /home/eric/trueos/code/c00/tools/bochsrc.disk

# 方式二：用 run_loader 脚本（自动启动）
/home/eric/trueos/code/c00/tools/run_loader
```

Bochs 启动后会自动进入 TrueOS，在 shell 中键入 `help` 或 `ls /` 开始体验。

### 单独编译用户态程序

```bash
cd command/
bash compile_cat.sh       # 编译 cat 程序
bash compile_arg.sh       # 编译带参数示例
bash compile_no_arg.sh    # 编译无参数示例
bash compile_pipe.sh      # 编译管道示例
```

编译产物需要用 `dd` 写入磁盘镜像的预留扇区，内核启动后可通过 shell 的 `exec` 命令加载执行。

### 常用命令

```bash
make clean              # 清理构建产物
make build              # 只编译不写盘
make disk               # 仅写盘
```

---

## 🧠 核心设计

### 启动流程

```
BIOS
 │
 ▼
MBR (boot/mbr.s)          0x7C00  实模式
 ├─ 初始化段寄存器、栈
 ├─ 清屏
 └─ int 0x13 读磁盘扇区 → 加载 Loader 到 0x1000
 │
 ▼
Loader (boot/loader.s)    0x1000  实模式 → 保护模式
 ├─ 构建 GDT（内核代码段/数据段/显存段/TSS）
 ├─ 构建 IDT（256 项，全部默认中断门）
 ├─ 启动 A20
 ├─ 构建页表（PDE[0]=PDE[768] 同页表，0~4MB 双向映射）
 ├─ 进入保护模式（CR0.PE=1）
 └─ 跳转到内核入口 0xC0010000
 │
 ▼
Kernel (kernel/main.c)   0xC0010000  保护模式
 ├─ init_all()：idt / console / mem / thread / timer / keyboard / tss / syscall / ide
 ├─ intr_enable()：开中断
 └─ thread_exit() → 调度 init 线程
```

### 内存布局

| 范围 | 用途 |
|------|------|
| `0xC0000000 ~ 0xFFFFFFFF` | 内核空间（直接映射物理内存） |
| `0x08048000 ~ 0xBFFFFFFF` | 用户虚拟地址（代码/数据/堆从低向高增长） |
| `物理 0x00000000 ~ 0x000FFFFF` | 内核物理池起始，包含 PCB / 页表 / 位图等 |
| `物理 0x01100000+` | 用户物理池 |

### 进程创建内存消耗

| 类型 | PCB | 位图 | 页目录 | 页表 |
|------|-----|------|--------|------|
| 内核线程 | 1 页 | — | 复用内核 PDT | 按需 |
| 用户进程 | 1 页 | 23 页 | 1 页（复制内核高半 PDE） | 按需 |

### 文件系统结构

```
磁盘镜像（60MB）
├─ 超级块 (1 块)
├─ inode 位图 (N 块)
├─ 数据块位图 (N 块)
├─ inode 表 (N 块)
└─ 数据区
```

inode 使用 **12 直接块 + 1 一级间接块**，单文件最大约 70KB（140 块 × 512B）。

### 系统调用约定

- 所有系统调用通过 `int 0x80` 触发
- `eax` 寄存器存放子功能号（syscall 编号）
- 参数通过 `ebx`, `ecx`, `edx`, `esi`, `edi` 传递
- 返回值在 `eax` 中

支持的系统调用：
- `write` / `read` / `open` / `close` / `lseek`
- `fork` / `execv` / `wait` / `exit`
- `malloc` / `free`
- `pipe`

---

## 🧪 调试技巧

项目编译时开启了 `-g` 调试信息和 `system.map` 符号表，可结合 Bochs 内置调试器使用：

```bash
# Bochs 启动时自动进入调试器
bochs -f /home/eric/trueos/code/c00/tools/bochsrc.disk
```

常用 Bochs 调试命令：

```
lb main            # 在内核 main 函数设断点
lb sys_open        # 在 sys_open 设断点
x /32bx 0xB8000    # 查看显存
info cr3           # 查看当前页目录
info idt           # 查看中断描述符表
```

---

## 📚 参考

- 《操作系统真象还原》郑钢 著
- Intel® 64 and IA-32 Architectures Software Developer Manuals
- [bochs.sourceforge.io](https://bochs.sourceforge.io/) — Bochs PC 模拟器

---

## 📝 License

此 README 由 AI 生成，仅用于学习交流。基于该书配套代码进行改写与扩展。