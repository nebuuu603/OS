#### 功能模块一提示词

````text
[PROMPT] 实现 kern/trap/trapentry.S 中 RESTORE_ALL 宏和 __alltraps 函数

[CONTEXT]
RISC-V Supervisor 模式下，Trap（中断或异常）发生时硬件会自动：
1. 保存当前 PC 到 sepc
2. 保存 Trap 原因到 scause
3. 保存出错地址到 sbadaddr  
4. 更新 sstatus 中的相关位（SPP、SPIE 等）
5. 跳转到 stvec 指定的处理入口（即 __alltraps）

但硬件不会保存通用寄存器 x0-x31。软件必须手动保存和恢复这些寄存器，
形成完整的 Trap frame，才能确保被中断的程序能够正确恢复执行。

已有的 SAVE_ALL 宏（位于 trapentry.S 开头）执行以下操作：
1. 将原始 sp 保存到 sscratch
2. 调整 sp，分配 36 * REGBYTES 的栈空间用于 Trap frame
3. 保存通用寄存器 x0, x1, x3-x31 到栈上对应位置（注意 x2 即 sp 跳过）
4. 从 sscratch 读取原始 sp 值到 s0，并清零 sscratch
5. 读取 sstatus、sepc、sbadaddr、scause 到 s1-s4
6. 将 s0-s4 保存到 Trap frame 的 2*REGBYTES、32-35*REGBYTES 位置

执行 SAVE_ALL 后，sp 指向 Trap frame 起始地址，其布局为：
- 0*REGBYTES:  x0
- 1*REGBYTES:  x1 (ra)
- 2*REGBYTES:  原始 sp（Trap 发生前的栈指针）
- 3*REGBYTES:  x3 (gp)
- ...
- 31*REGBYTES: x31
- 32*REGBYTES: sstatus
- 33*REGBYTES: sepc
- 34*REGBYTES: sbadaddr
- 35*REGBYTES: scause

__alltraps 的作用是调用 SAVE_ALL 后，将 Trap frame 地址传递给 C 函数 trap()，
trap() 函数签名为：void trap(struct trapframe *tf);
RISC-V 调用约定规定第一个参数通过 a0 寄存器传递。

[TASK]
1. 实现 RESTORE_ALL 宏，从 Trap frame 恢复处理器状态：
   - 恢复特权 CSR：sstatus、sepc（从 32*、33*REGBYTES 位置）
   - 恢复通用寄存器 x1, x3-x31（从 1*、3*-31*REGBYTES 位置）
   - 最后恢复 sp（从 2*REGBYTES 位置，即原始栈指针）
   - 注意：x0 硬连线到 0，无需恢复；x2(sp) 的处理方式与其他寄存器不同
   - 注意：sbadaddr 和 scause 是只读 CSR，无需恢复

2. 实现 __alltraps 函数（在已有 SAVE_ALL 调用之后）：
   - 将当前 sp（Trap frame 起始地址）传递给 a0
   - 调用 trap 函数
   - trap 返回后会跳转到 __trapret 执行 RESTORE_ALL 和 sret

[CONSTRAINTS]
- 必须严格按照 SAVE_ALL 建立的 Trap frame 布局进行恢复
- RESTORE_ALL 中恢复 sp 必须在最后，使用 sp 作为基址访问其他寄存器时 sp 还指向 Trap frame
- 恢复 CSR 时需要使用临时寄存器（如 t0），因为不能直接从内存写入 CSR
- 不能修改 SAVE_ALL、struct trapframe 或 trap() 的定义
- __alltraps 中调用 trap 前必须确保 sp 指向有效的 Trap frame
- RISC-V 调用约定要求 trap() 必须保持 sp 不变，因此返回时 sp 仍指向 Trap frame

[OUTPUT]
生成完整的 RESTORE_ALL 宏定义和 __alltraps 函数中 #lab3 function1 标记后的代码。
代码应使用 RISC-V 汇编语法，正确使用 LOAD/STORE 宏（已在 riscv.h 中定义），
确保所有寄存器恢复顺序正确，最终能够通过 sret 正确返回到被中断的程序。
````