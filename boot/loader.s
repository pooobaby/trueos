LOADER_BASE_ADDR    equ 0x1000            ; loader 的加载地址
KERNEL_BASE_ADDR    equ 0x70000           ; kernel 的加载地址
; 下面的地址必须与链接地址 ENTRY_POINT 一致, 否则会导致 kernel 的 main 入口错误
; 2026/09/18 这个错误导致进程切换时发生 #PF 异常，排查很久才发现
KERNEL_START_ADDR   equ 0xc0010000           ; kernel 的 main 入口, 必须与链接地址 ENTRY_POINT 一致
KERNEL_STACK_ADDR   equ 0xc009f000           ; kernel 的栈地址

PAGE_DIR_TABLE_ADDR   equ 0x100000          ; 页目录表的地址
PG_P     equ  1b    ; 页存在
PG_RW_R	 equ  00b   ; 只读
PG_RW_W	 equ  10b   ; 可读可写
PG_US_S	 equ  000b  ; 系统级
PG_US_U	 equ  100b  ; 用户级

; 加载程序开始
section loader vstart=LOADER_BASE_ADDR

; ---------------------------------------------------------------
; 构建 gdt 及其内部的描述符
; ---------------------------------------------------------------
    GDT_BASE:
        dd 0x00000000 
	    dd 0x00000000
    CODE_DESC:
        dd 0x0000FFFF 
        dd 0x00CF9800
    DATA_STACK_DESC:
        dd 0x0000FFFF
        dd 0x00CF9200
    VIDEO_DESC:
        dd 0x80000007
        dd 0xC0C0920B       ; 2026-09-08 原为 0x00C0920B, 将 VIDEO_DESC 段基址从 0x000B8000 改为 0xC00B8000

    GDT_SIZE equ $ - GDT_BASE   ; gdt 大小
    GDT_LIMIT equ GDT_SIZE - 1  ; gdt 界限

    times 60 dq 0       ; 预留 60 个描述符的 slot

    ; 上面 gdt 区的字节一共 0x200b
    
    total_mem dd 0    ; 总内存大小, 单位 KB, 此处地址为 0x1200
    ; 定义 gdt 的指针
    gdt_ptr dw GDT_LIMIT    ; 2 字节界限
            dd GDT_BASE     ; 4 字节起始地址

    ; 上面 gdt 区的字节一共 0x200b, total_mem 4 字节, gdt_ptr 6 字节, 共 0x20a 个字节
    ; 因此从 loader_start 开始的地址是 0x120a

loader_start:
; ---------------------------------------------------------------
; 获取物理内存大小, 保存到 total_mem_bytes(0x1200)
; 使用 int 0x15 的 0x88 子功能, 只能获取64M内的内存大小
; 未来可以扩展 ax=0xE801 或 eax=0xE820 子功能, 获取4G以内的内存大小
; ---------------------------------------------------------------
    mov ah, 0x88
    int 0x15
    add eax, 0x400   ; 需要加上 1 MB, 因为 0x88 只会返回 1 MB 以上的内存
    mov [total_mem], eax

; ---------------------------------------------------------------
; 输出背景色黑色, 绿色的字符串"this is in real mode. "
; ---------------------------------------------------------------
    ; 获取光标位置
    mov ah, 3       ; 3 号子功能是获取光标位置
    mov bh, 0       ; bh 寄存器存储的是待获取光标的页号
    int 0x10        ; 输出: ch = 光标开始行, cl = 光标结束行, dh = 光标所在行号, dl = 光标所在列号

    ; 输出背景色黑色, 前景青色的字符串"this is a loader!"
    add dh, 1               ; 行号 +1, 显示在 MBR 的下一行
    mov al, dh              ; ax = 行号
    mov cl, 160             ; 每行 80 字符 * 2 字节 = 160 字节
    mul cl                  ; ax = 行号 * 160 = 最终显存偏移, 显示在行的最前面, 列号为 0
    mov di, ax              ; di 保存起始显存偏移 
    mov si, msg_real         ; si 指向字符串首地址
    mov cx, msg_real_end - msg_real   ; 循环次数 = 字符串长度
    .putc_real:
        mov al, [si]            ; 取字符
        mov [gs:di], al         ; 写字符到显存
        mov byte [gs:di+1], 0x2 ; 写属性0_000_0_011 = 闪烁(0) + 黑底(000) + 高亮(0) + 绿(0010)
        inc si
        add di, 2
        loop .putc_real

    ; 根据 di 反推光标位置(di 是打印结束后的显存偏移, 避免硬编码字符串长度)
    mov ax, di          ; ax = di 终值 = 起始偏移 + 字符串长度 * 2
    mov cl, 160
    div cl              ; al = ax / 160 = 行号, ah = ax % 160 = 列字节偏移
    mov dh, al          ; dh = 行号
    shr ah, 1           ; ah = 列字节偏移 / 2 = 列号
    mov dl, ah          ; dl = 列号

    ; 设置光标位置
    mov ah, 2           ; 2 号子功能是设置光标位置
    mov bh, 0           ; 显式设为第 0 页
    int 0x10

; ---------------------------------------------------------------
; 准备进入保护模式, 共 4 步
; ---------------------------------------------------------------
    ; 1. 打开 A20 地址线
    cli                 ; 显式关闭中断, Linux 0.11、xv6 等都是这么做的

    in al, 0x92
    or al, 0x2
    out 0x92, al

    ; 2. 加载 gdt
    lgdt [gdt_ptr]

    ; 3. 将 cr0 的第 0 位(PE)设置为 1
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    ; 4. 刷新流水线, 避免分支预测的影响,这种 cpu 优化策略, 最怕 jmp 跳转, 
    jmp dword 0x08:prot_mode    ; 代码段选择子, (0x08 = 0000_0000_0000_1000b, index = 1)

; ---------------------------------------------------------------
; 进入保护模式
; ---------------------------------------------------------------
[bits 32]
prot_mode:
    mov ax, 0x10    ; 数据段选择子 (0x10 = 0000_0000_0001_0000b, index = 2)
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, LOADER_BASE_ADDR   ; 栈顶地址 = 加载器加载地址
    mov ax, 0x18                ; 视频段选择子 (0x18 = 0000_0000_0001_1000b, index = 3)
    mov gs, ax

    ; 从硬盘载入 kernel.bin, 未来的内核文件会有近 100K
    mov eax, 0x9                ; 从第 9 个扇区开始加载
    mov ebx, KERNEL_BASE_ADDR   ; 将数据写入的内存地址
    mov ecx, 200                ; 读入的扇区数为 200 个

    ; 读取 kernel.bin 到内存
    call read_kernel             

; ---------------------------------------------------------------
; 创建页目录表及页表, 并开启分页
; ---------------------------------------------------------------
    call setup_page         ; 创建页目录表及页表

    ; 将 gdtr 地址及偏移量写入内存 gdt_ptr
    sgdt [gdt_ptr]          

    ; 将 gdt 中视频段的基址修改为虚拟地址 0xc00b8000, 
    mov ebx, [gdt_ptr + 2]
    or dword [ebx + 0x1c], 0xc0000000

    ; 将 gdt 的基址加上 0xc0000000 使其成为内核所在的高地址, 修改后的 gdt 基址是 0xc0001000
    add dword [gdt_ptr + 2], 0xc0000000

    ; 将栈指针同样映射到内核地址
    add esp, 0xc0000000     

    ; 把页目录地址赋给 cr3
    mov eax, PAGE_DIR_TABLE_ADDR
    mov cr3, eax

    ; 打开 cr0 的 pg 位(第 31 位)
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; 在开启分页后用 gdt 新的地址重新加载
    lgdt [gdt_ptr]

    ; 初始化 kernel
    call kernel_init
    mov esp, KERNEL_STACK_ADDR  ; 栈需要重新规划, 原来的栈在 0x1000, 这里要使用虚拟地址

; ---------------------------------------------------------------
; 输出背景色黑色, 前景绿色的字符串"Now in protected mode. Page..."
; dh 和 dl 在上面设置完光标位置后, 并没有改变
; 保护模式下 int 0x10 不能用, 因为没有建立 idt 表
; ---------------------------------------------------------------

    mov al, 2               ; ax = 行号
    mov cl, 160             ; 每行 80 字符 * 2 字节 = 160 字节
    mul cl                  ; ax = 行号 * 160 = 最终显存偏移, 显示在行的最前面, 列号为 0
    mov di, ax              ; di 保存起始显存偏移 
    mov si, msg_prot        ; si 指向字符串首地址
    mov ecx, 0
    mov cx, msg_prot_end - msg_prot   ; 循环次数 = 字符串长度
    .putc_prot:
        mov al, [si]            ; 取字符
        mov [gs:di], al         ; 写字符到显存
        mov byte [gs:di+1], 0x2 ; 写属性 0_000_1100 = 闪烁(0) + 黑底(000) + 绿(0010)
        inc si
        add di, 2
        loop .putc_prot

; ---------------------------------------------------------------
; 设置光标显示的位置
; 把 di 转成字符偏移：字符偏移 = di / 2, 拆成高/低字节
; 依次写入 0x3D4 → 0x0E, 0x3D5 → 高字节, 0x3D4 → 0x0F, 0x3D5 → 低字节
; ---------------------------------------------------------------
    movzx eax, di           ; di 是 16 位字节偏移, 零扩展到 32 位 eax（edi 高 16 位可能脏, 必须清掉）
    shr eax, 1              ; 字节偏移 / 2 = 字符偏移, 这就是光标的线性位置
    mov ebx, eax            ; 保存光标位置性偏移到 ebx

    mov dx, 0x3D4           ; 0x3D4 是 CRT 索引寄存器, 指定要访问的内部寄存器号
    mov al, 0x0E            ; 选 0x0E 号寄存器, 光标高字节
    out dx, al
    inc dx                  ; 0x3D5 是 CRT 数据寄存器, 读写上面选中的寄存器
    mov al, bh              ; bh = 光标位置的 15..8 位
    out dx, al

    mov dx, 0x3D4           ; 同样将光标位置的低字节写入
    mov al, 0x0F
    out dx, al
    inc dx
    mov al, bl
    out dx, al

; ---------------------------------------------------------------
; loader 结束, 跳转到内核入口地址 main
; ---------------------------------------------------------------
    nop
    jmp KERNEL_START_ADDR

; ---------------------------------------------------------------
; 函数: 初始化 kernel, 将内核文件中的 segment 展开到(复制到)内存中的相应位置
; ---------------------------------------------------------------
kernel_init:
    xor eax, eax
    xor ebx, ebx    ; ebx 记录程序头表地址
    xor ecx, ecx    ; cx 记录程序头表中的 program header 数量
    xor edx, edx    ; dx 记录 e_phentsize, 即 program header 大小 

    mov dx, [KERNEL_BASE_ADDR + 42]     ; e_phentsize
    mov ebx, [KERNEL_BASE_ADDR + 28]    ; e_phoff
    add ebx, KERNEL_BASE_ADDR
    mov cx, [KERNEL_BASE_ADDR + 44]     ; e_phnum

    .each_segment:
        cmp byte [ebx + 0], 0x0
        je .next_segment

        push dword [ebx + 16]       ; p_filesz,  参数 3 size
        mov eax, [ebx + 4]          ; p_offset
        add eax, KERNEL_BASE_ADDR
        push eax                    ; 参数 2 src
        push dword [ebx + 8]        ; p_vaddr, 参数 1 dst

        call mem_cpy
        add esp, 12                 ; 清理栈

    .next_segment:
        add ebx, edx        ; 移动到下一个 program header
        loop .each_segment
    ret

; ---------------------------------------------------------------
; 函数: 逐字节拷贝 mem_cpy (dst, src, size) 
; ---------------------------------------------------------------
mem_cpy:
    cld     ; 将 eflags 中的方向标志位 DF 置为 0, 表示从低地址向高地址拷贝

    push ebp
    mov ebp, esp
    push ecx
    mov edi, [ebp + 8]      ; dst
    mov esi, [ebp + 12]     ; src
    mov ecx, [ebp + 16]     ; size

    rep movsb               ; 逐字节拷贝

    ; 恢复栈环境
    pop ecx
    pop ebp
    ret

; ---------------------------------------------------------------
; 函数: 读取硬盘 n 个扇区, 我们这里采用 LBA28 模式
;       eax = LBA 扇区号
;       ebx = 将数据写入的内存地址
;       ecx = 读入的扇区数
; ---------------------------------------------------------------
read_kernel:
    mov esi, eax	  ; 备份 eax
    mov di, cx		  ; 备份 cx

; 读写硬盘:
; 第 1 步, 设置要读取的扇区数
    mov dx, 0x1f2
    mov al, cl
    out dx, al          ; 读取的扇区数

    mov eax, esi        ; 恢复 ax

; 第 2 步, 将LBA地址存入 0x1f3 ~ 0x1f6
    ; LBA地址 7 ~ 0 位写入端口 0x1f3
    mov dx, 0x1f3
    out dx, al

    ; LBA地址 8 ~ 15 位写入端口 0x1f4
    mov cl, 8
    shr eax, cl
    mov dx, 0x1f4
    out dx, al

    ; LBA地址 16 ~ 23 位写入端口 0x1f5
    shr eax, cl
    mov dx, 0x1f5
    out dx, al

    ; LBA地址 24 ~ 27 位写入端口 0x1f6
    shr eax, cl
    and al, 0x0f
    or al, 0xe0     ; 设置 4 - 7 位为 1110, 表示 LBA 模式
    mov dx, 0x1f6
    out dx, al

; 第 3 步, 向 0x1f7 端口写入读命令 0x20 
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

; 第 4 步, 从 0x1f7 端口读取硬盘状态 
    .not_ready:
        ; dx 同一端口, 写时表示写入命令字, 读时表示读入硬盘状态
        nop
        in al, dx
        and al, 0x88        ; 第 4 位为 1 表示硬盘控制器已准备好数据传输, 第 7 位为 1 表示硬盘忙
        cmp al, 0x08
        jnz .not_ready	    ; 若未准备好, 继续等
        
; 第 5 步, 从 0x1f0 端口读数据
    mov ax, di      ; di 为要读取的扇区数, 一个扇区有 512 字节, 每次读入一个字, 
			        ; 共需 di * 512 / 2 次, 所以 di * 256
    mov dx, 256
    mul dx
    mov cx, ax
    mov dx, 0x1f0
    .go_on_read:
        in ax, dx
        mov [ebx], ax
        add ebx, 2
        loop .go_on_read
    ret

; ---------------------------------------------------------------
; 函数: setup_page 创建页目录及页表(原理: 物理页表交错 + 虚拟地址隔离 + 共享内核页表)
; ---------------------------------------------------------------
; 1. 先把页目录表占用的空间逐字节清 0
setup_page:
    mov ecx, 4096
    mov esi, 0
    .clear_page_dir:
        mov byte [PAGE_DIR_TABLE_ADDR + esi], 0
        inc esi
        loop .clear_page_dir

; 2. 创建第 0 , 768, 1023 个页目录项(PDE)
    ; .create_pde:
    mov eax, PAGE_DIR_TABLE_ADDR
    add eax, 0x1000     ; eax = 0x0010_1000
    mov ebx, eax        ; 为 .create_pte 做准备, ebx 为基址

    ; -----------------------------------------------------------
    ; 下面将页目录项 0 和 0xc00 都存为第一个页表的地址, 0xc00 表示第 768 个页目录项, 
    ; 第 0 个页目录项代表的页表, 其表示的空间是 0 ~ 0x3ffff(包括了 1M 的内核空间)
    ; 这样虚拟地址 3G ~ 3G + 4M 和 0 ~ 4M 都会指向相同的页表, 
    ; -----------------------------------------------------------
    or eax, PG_US_U | PG_RW_W | PG_P    ; eax = 0x0010_100_7, 表示页表的地址在 1M 物理地址开始
    mov [PAGE_DIR_TABLE_ADDR + 0x0], eax
    mov [PAGE_DIR_TABLE_ADDR + 0xC00], eax
        ; 0xc00 以上的页目录项用于内核空间      
        ; 也就是页表的 0xc0000000 ~ 0xffffffff 共计 1G 属于内核,0x0 ~ 0xbfffffff 共计 3G 属于用户进程.
    sub eax, 0x1000
    mov [PAGE_DIR_TABLE_ADDR + 4092], eax   ; 使最后一个目录项指向页目录表自己的地址

; 3. 创建页表 0 的前 256 项(PTE), 用来分配物理地址范围 0 ~ 4M 之间的物理页
    mov ecx, 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P    ; edx = 0x0000_000_7, 表示从 0 开始的物理地址
    .create_pte:
        mov [ebx + esi * 4], edx
        add edx, 0x1000
        inc esi
        loop .create_pte

; 4. 创建内核(3G以上)的其它页目录项(PDE), 为了实现内核共享
    mov eax, PAGE_DIR_TABLE_ADDR
    add eax, 0x2000 		            ; eax = 0x0010_2000
    or eax, PG_US_U | PG_RW_W | PG_P    ; eax = 0x0010_200_7
    mov ebx, PAGE_DIR_TABLE_ADDR
    mov ecx, 254			            ; 第 769 ~ 1022 的页目录项数量
    mov esi, 769
    .create_kernel_pde:
        mov [ebx + esi * 4], eax
        inc esi
        add eax, 0x1000
        loop .create_kernel_pde
    ret

; ---------------------------------------------------------------
; 数据定义区域
; ---------------------------------------------------------------


    msg_real db "this is in real mode."
    msg_real_end:
    msg_prot db "Now in protect mode. Page opened. Kernel is loaded."
    msg_prot_end:


; ---------------------------------------------------------------
; 附表: 页目录表及页表的布局
; 物理地址   内容               值            对应 / 基址
; -- 页目录表 ----------------------------------------------------
; 0x100000  PDE[0]          0x00101007      第 0 个页表
;           PDE[1~767]      空
;           PDE[768]        0x00101007      与 PDE[0] 同指向第 0 个页表
;           PDE[769]        0x00102007      第 1 个内核页表(空)
;           PDE[770]        0x00103007      第 2 个内核页表(空)
;           ...
;           PDE[1022]       0x001FF007      第 254 个内核页表(空)
;           PDE[1023]       0x00100007      页目录表自身映射
; -- 第 0 个页表 -------------------------------------------------
; 0x101000  PTE[0]          0x00000007      0x00000000
;           PTE[1]          0x00001007      0x00001000
;           PTE[2]          0x00002007      0x00002000
;           ...
;           PTE[255]        0x000FF007      0x000FF000
;           PTE[256~1023]   0 (未填)
; -- 第 n 个页表 -------------------------------------------------
; 0x102000  第 1 个内核页表(空)
; 0x103000  第 2 个内核页表(空)
; ...
; 0x1FF000  第 254 个内核页表(空)
; -- 2M 以上空间 -------------------------------------------------
; 0x200000  可用物理内存 (Free), 将来给内核, 用户页表和数据页分配

; 虚拟内存与物理内存的映射区域
; 0x00000000 - 0x000FFFFF -> 0x00000000 - 0x000FFFFF
; 0xC0000000 - 0xc00FFFFF -> 0x00000000 - 0x000FFFFF
; 0xFFC00000 - 0xFFC00FFF -> 0x00101000 - 0x00101FFF
; 0xFFF00000 - 0xFFFFEFFF -> 0x00101000 - 0x001FFFFF
; 0xFFFFF000 - 0xFFFFFFFF -> 0x00100000 - 0x00100FFF
