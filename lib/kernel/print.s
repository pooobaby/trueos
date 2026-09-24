section .data
put_int_buffer dq 0     ; 定义 8 字节缓冲区用于数字到字符的转换

[bits 32]
section .note.GNU-stack noalloc noexec nowrite progbits
section .text

global put_char, put_str, put_int, set_cursor, cls_screen, put_char_color, put_str_color

; ---------------------------------------------------------------
; 函数: void put_str(char *EEstr)
; 输入: 栈中参数为打印的字符串
; 输出: 无
; 功能: 以默认属性(黑底白字 0x07) 通过 put_char 来打印以 0 字符结尾的字符串
; ---------------------------------------------------------------  
put_str:
    mov eax, [esp + 4]          ; str
    push dword 0x07             ; attr
    push eax                    ; str
    call put_str_color
    add esp, 8
    ret

; ---------------------------------------------------------------
; 函数: void put_str_color(char *str, uint8_t attr)
; 功能: 以指定属性 attr 打印以 0 结尾的字符串
; ---------------------------------------------------------------
put_str_color:
    push ebx
    push ecx
    push esi
    mov esi, [esp + 16] ; str 地址
    mov ebx, [esp + 20] ; attr
    .goon:
        mov cl, [esi]
        cmp cl, 0       ; 如果处理到了 /0, 跳到 .str_over
        jz .str_over
        push ebx        ; attr
        push ecx        ; char_asci
        call put_char_color
        add esp, 8      ; 回收参数所占的栈空间
        inc esi         ; 使 esi 指向下一个字符
        jmp .goon
    .str_over:
        pop esi
        pop ecx
        pop ebx
        ret

; ---------------------------------------------------------------
; 函数: void put_char(uint8_t char_asci)
; 输入: 栈中参数为待打印的字符
; 输出: 无
; 功能: 把栈中的 1 个字符写入光标所在处
; ---------------------------------------------------------------
put_char:
    mov eax, [esp + 4]              ; char_asci
    push dword 0x07                 ; attr
    push eax                        ; char_asci
    call put_char_color
    add esp, 8
    ret

; ---------------------------------------------------------------
; 函数: void put_char_color(uint8_t char_asci, uint8_t attr)
; 功能: 以指定属性 attr 把 1 个字符写入光标所在处
; ---------------------------------------------------------------
put_char_color:
    pushad              ; 备份 32 位寄存器环境, 共 8 个
    mov ax, 0x18
    mov gs, ax

    ; 获取当前光标位置
    mov dx, 0x03d4      ; 索引寄存器端口地址
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5      ; 数据寄存器端口地址
    in al, dx           ; 获取光标位置的高位
    mov ah, al

    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx           ; 获取光标位置的低位

    mov bx, ax          ; 将光标位置保存到 bx

    ; 参数: char_asci -> cl, attr -> ch
    mov ecx, [esp + 36]
    mov ch, [esp + 40]

    cmp cl, 0xd
    jz .is_carriage_return  ; 处理回车键 CR
    cmp cl, 0xa
    jz .is_line_feed        ; 处理换行键 LF

    cmp cl, 0x8
    jz .is_backspace       ; 处理退格键 backspace
    jmp .put_other

    ; 处理退格符 backspace
    .is_backspace:
        dec bx
        shl bx, 1               ; bx = bx * 2
        mov byte [gs:bx], 0x20  ; 写入空格字符
        inc bx
        mov byte [gs:bx], ch    ; 使用指定属性
        shr bx, 1               ; bx = bx / 2
        jmp .set_cursor

    ; 处理可见字符
    .put_other:
        shl bx, 1
        mov byte [gs:bx], cl
        inc bx
        mov byte [gs:bx], ch    ; 使用指定属性
        shr bx, 1
        inc bx
        cmp bx, 2000            ; 2000 表示显存的最后
        jl .set_cursor
    
    ; 处理换行符和回车符
    ; LF(\n 把光标移到下一行行首)  CR(\r 把光标移到行首)
    ; 依效 linux, 把 \n 和 \r 都处理为 \n, 也就是下一行的行首
    .is_line_feed:
    .is_carriage_return:
        xor dx, dx              ; dx 是被除数的高 16 位, 清 0
        mov ax, bx              ; ax 是被除数的低 16 位
        mov si, 80
        div si                  ; ax = bx / 80, dx = bx % 80
        sub bx, dx              ; bx = bx - (bx % 80), bx 取整即为行首
    .is_carriage_return_end:    ; CR(\r) 处理结束

        add bx, 80              ; 处理 LF(\n), 将光标 +80 便移到下一行
        cmp bx, 2000
    .is_line_feed_end:          ; LF(\n) 处理结束
        jl .set_cursor

    ; 处理滚屏
    .roll_screen:
        cld
        ; 共有 2000 - 80 = 1920 个字符, 1920 * 2 = 3840 个字节
        ; 每次搬运 4 个字节, 共需要 960 次
        mov ecx, 960            
        mov esi, 0xc00b80a0        ; 第 1 行行首, 使用虚拟地址, 2026-09-08
        mov edi, 0xc00b8000        ; 第 0 行行首, 使用虚拟地址
        rep movsd

        ; 将最后一行填充为空白
        mov ebx, 3840           ; 最后一行首字符的第一个字节偏移 = 1920 * 2
        mov ecx, 80             ; 一需要移动 80 次
        .cls:
            mov word [gs:ebx], 0x0720
            add ebx, 2
            loop .cls
        
        mov bx, 1920            ; 将光标值重置为 1920, 最后一行的首字符

    .set_cursor:
        mov dx, 0x03d4          ; 设置高 8 位
        mov al, 0x0e
        out dx, al
        mov dx, 0x03d5
        mov al, bh
        out dx, al

        mov dx, 0x03d4          ; 设置低 8 位
        mov al, 0x0f
        out dx, al
        mov dx, 0x03d5
        mov al, bl
        out dx, al
    .put_char_done: 
        popad
        ret

; ---------------------------------------------------------------
; 函数: void set_cursor(uint32_t pos)
; 输入: 栈中参数为光标位置 pos (0~1999)
; 输出: 无
; 功能: 将光标设置到指定位置
; ---------------------------------------------------------------
set_cursor:
    push ebx
    mov ebx, [esp + 8]      ; 从栈中获取 pos 参数, push ebx + 返回地址 = 8 字节

    mov dx, 0x03d4          ; 设置光标高 8 位
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    mov al, bh
    out dx, al

    mov dx, 0x03d4          ; 设置光标低 8 位
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al

    pop ebx
    ret

; ---------------------------------------------------------------
; 函数: void put_int(uint32_t num);
; 输入: 栈中参数为待打印的数字
; 输出: 无
; 功能: 打印十六进制数字, 不会打印前缀 0x, 如打印 15, 只会打印 f, 不是 0xf
; ---------------------------------------------------------------
put_int:
    pushad
    mov ebp, esp
    mov eax, [ebp + 36]         ; pushad + 主调函数的返回地址 = 36 字节
    mov edx, eax
    mov edi, 7                  ; 在 put_int_buffer 中初始的偏移量, 偏移为 7
                                ; 表示是在该地址处存储数字最低 4 位二进制对应的字符。
    mov ecx, 8                  ; 32 位数字中,16 进制数字的位数是 8 个  
    mov ebx, put_int_buffer

    .16based_4bits:
        and edx, 0x0000000f     ; and 操作后, edx 只有低 4 位有效
        cmp edx, 9              ; 数字 0~9 和 A~F 需要分别处理成对应的字符
        jg .is_A2F
        add edx, '0'            ; ascii 码 是 8 位大小。add 求和操作后, edx 低 8 位有效。
        jmp .store

    .is_A2F:
        sub edx, 10             ; A~F 减去 10 再加上字符 A, 是 A~F 对应的 ascii 码
        add edx, 'A'

    ; 将每一位数字转换成对应的字符后, 按照类似“大端”的顺序存储到缓冲区 put_int_buffer
    ; 高位字符放在低地址, 低位字符要放在高地址, 这样和大端字节序类似, 只不过这里是字符序
    ; 此时 dl 中应该是数字对应的字符的 ascii 码
    .store:
        mov byte [ebx + edi], dl    ; 往 put_int_buffer 中写入转换好的字符
        dec edi
        shr eax, 4              ; 右移 4 位, 去掉已转换完成的低 4 位
        mov edx, eax            ; 进行下一轮的转换
        loop .16based_4bits

    ; 现在 put_int_buffer中 已全是字符, 打印之前把高位连续的字符去掉,比如把字符 000123 变成 123
    .ready_to_print:
        inc edi                 ; 这时 edi = -1 + 0 = 0
    .skip_prefix_0:
        cmp edi, 8              ; 若已经比较第 9 个字符了, 表示待打印的字符串为全 0 
        je .full0

    .go_on_skip:
        mov cl, [put_int_buffer + edi]
        inc edi
        cmp cl, '0'
        je .skip_prefix_0
        dec edi                 ; 将 edi 的位置 -1, 指向当前字符
        jmp .put_each_num

    .full0:
        mov cl, '0'             ; 输入的数字为全 0 时, 则只打印 0

    .put_each_num:
        push ecx
        call put_char
        add esp, 4
        inc edi                 ; 使 edi 指向下一个字符
        mov cl, [put_int_buffer + edi]       ; 获取下一个字符到 cl 寄存器
        cmp edi, 8
        jl .put_each_num

        popad
        ret
; ---------------------------------------------------------------
; 函数: void cls_screen (void);
; 输入: 无
; 输出: 无
; 功能: 清除屏幕, 并将光标设置到屏幕的首字符
; ---------------------------------------------------------------
cls_screen:
    pushad
    mov ax, 0x18                ; gs 视频内存段选择子
    mov gs, ax

    mov ebx, 0
    mov ecx, 80 * 25

    .cls:
        mov word [gs:ebx], 0x0720   ; 0x0720 是黑底白字的空格键
        add ebx, 2
        loop .cls
        mov ebx, 0

    .set_cursor:
        ; 1 先设置高8位
        mov dx, 0x03d4          ;索引寄存器 
        mov al, 0x0e            ;用于提供光标位置的高8位
        out dx, al
        mov dx, 0x03d5			;通过读写数据端口0x3d5来获得或设置光标位置 
        mov al, bh
        out dx, al

        ; 2 再设置低8位
        mov dx, 0x03d4
        mov al, 0x0f
        out dx, al
        mov dx, 0x03d5 
        mov al, bl
        out dx, al
        popad
        ret
