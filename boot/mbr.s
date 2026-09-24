LOADER_BASE_ADDR equ 0x1000     ; loader 的加载地址
LOADER_START equ 0x120a         ; loader_start 的地址

; 主引导程序
SECTION MBR vstart=0x7c00
    mov ax, cs      ; 用 cs 寄存器的值去初始化其他寄存器
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov sp, 0x7c00  ; 初始化栈指针, 栈从 0x7c00 开始向下增长是安全的
    mov ax, 0xb800  ; 初始化 gs 寄存器, 指向 VGA 文本模式内存
    mov gs, ax

; ---------------------------------------------------------------
    ; 清屏
    mov ax, 0x600         ; AH 子功能号 = 0x06 AL = 上卷的行数(如果为0, 表示全部)
    mov bx, 0x700         ; BH = 0x70 黑底浅灰字（默认）
    mov cx, 0             ; 左上角: (0, 0)
    mov dx, 0x184f        ; 右下角: (80, 25)
    int 0x10
    
    ; 获取光标位置
    ; mov ah, 3       ; 3 号子功能是获取光标位置
    ; mov bh, 0       ; bh 寄存器存储的是待获取光标的页号
    ; int 0x10        ; 输出: ch = 光标开始行, cl = 光标结束行, dh = 光标所在行号, dl = 光标所在列号

    ; 设置光标位置
    mov ah, 2           ; 2 号子功能是设置光标位置
    mov dl, 0           ; 列号设为 0
    mov dh, 0           ; 行号设为 0
    mov bh, 0           ; 显式设为第 0 页
    int 0x10

    ; 以下部分注释掉, 因为采用了直接向 0xB800 写入字符的方法, 而不是使用中断 0x10 输出
    ; 设置光标位置

    ; mov ah, 2       ; 2 号子功能是设置光标位置
    ; add dh, 1       ; dh = 光标所在行号 + 1
    ; int 0x10

    ; 向屏幕输出字符串
    ; mov ax, message
    ; mov bp, ax      ; es:bp 为串首地址, es 程序开始时已初始化为 cs
    ; mov cx, 11      ; cx 为串长度,不包括结束符 0 的字符个数
    ; mov ax, 0x1301  ; 0x13 子功能号是显示字符串, al = 0x01 设置写字符方式为光标跟随移动
    ; mov bx, 0xa     ; bh 背景色(0 为黑色), bl 前景色(a = 10 为亮绿)
    ; int 0x10

    ; 注释部分结束
; ---------------------------------------------------------------

; ---------------------------------------------------------------
; 输出背景色黑色, 前景绿色的字符串"Eric's OS, this is a MBR!"
; ---------------------------------------------------------------
    mov al, dh              ; ax = 行号
    mov cl, 160             ; 每行 80 字符 * 2 字节 = 160 字节
    mul cl                  ; ax = 行号 * 160
    mov cl, dl              ; cl = 列号
    shl cl, 1               ; cl = 列号 * 2（寄存器不能用 dl * 2, 必须用指令）
    add al, cl              ; ax = 行号 * 160 + 列号 * 2 = 最终显存偏移
    mov di, ax              ; di 保存起始显存偏移
    mov si, message         ; si 指向字符串首地址
    mov cx, msg_end - message   ; 循环次数 = 字符串长度
    .putc_mbr:
        mov al, [si]            ; 取字符
        mov [gs:di], al         ; 写字符到显存
        mov byte [gs:di+1], 0x2 ; 写属性0_000_1010 = 闪烁(0) + 黑底(000) + 绿(0010)
        inc si
        add di, 2
        loop .putc_mbr

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

    ; 从硬盘载入 loader.s
    mov eax, 0x2                    ; 从第 2 个扇区开始加载
    mov bx, LOADER_BASE_ADDR        ; 将数据写入的内存地址
    mov cx, 3                       ; 读入的扇区数为 3 个
    call read_loader                ; 读取加载器扇区 loader.s

    ; 跳转到加载内存地址 0x1200
    jmp LOADER_START

; ---------------------------------------------------------------
; 函数: 读取硬盘 n 个扇区, 这里采用 LBA 28 模式
;       eax = LBA 扇区号
;       ebx = 将数据写入的内存地址
;       ecx = 读入的扇区数
; ---------------------------------------------------------------
read_loader:
    mov esi, eax	  ; 备份 eax
    mov di, cx		  ; 备份 cx

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
        mov [bx], ax
        add bx, 2
        loop .go_on_read
    ret

message db "Eric's OS, this is a MBR!"
msg_end:
times 510-($-$$) db 0
db 0x55,0xaa