[bits 32]
section .note.GNU-stack noalloc noexec nowrite progbits
section .text
global switch_to
switch_to:
    ; 栈中此处是[返回地址]
    push esi
    push edi
    push ebx
    push ebp

    mov eax, [esp + 20] ; 得到栈中的参数cur, cur = [esp + 20]
    mov [eax], esp      ; 保存栈顶指针esp. task_struct 的 self_kstack 字段
                        ; self_kstack 在task_struct 中的偏移为 0, 所以直接往 thread 开头处存 4 字节便可
    ; ------------------  以上是备份当前线程的环境
    ; 下面是恢复下一个线程的环境  ------------------
    mov eax, [esp + 24]
    mov esp, [eax];      ; 从栈中恢复下一个线程的栈顶指针esp
    pop ebp
    pop ebx
    pop edi
    pop esi
    ret                 ; 返回到 switch_to 下面的那句注释的[返回地址],
                        ; 未由中断进入,第一次执行时会返回到 kernel_thread