#### 功能模块一提示词

````text
[PROMPT] 实现 kern/trap/trapentry.S 中 RESTORE_ALL 宏和 __alltraps 函数

[CONTEXT]
SAVE_ALL 已在当前栈上建立 36*REGBYTES 的 trapframe，sp 指向其起始地址：
0*: x0，1*: ra，2*: 中断前的 sp，3*~31*: x3~x31
32*: sstatus，33*: sepc，34*: sbadaddr，35*: scause

[TASK]
RESTORE_ALL：
借 t0 中转恢复 sstatus、sepc（32*、33*）
恢复 x1、x3~x31（1*、3~31）
最后恢复 sp（2*），因为它是访问基址
不恢复 x0；sbadaddr/scause 只读，不恢复

__alltraps：SAVE_ALL 之后 mv a0, sp，call trap；返回后顺序执行 __trapret（RESTORE_ALL + sret）

[CONSTRAINTS]
使用 riscv.h 的 LOAD 宏；严格按上述布局；不修改 SAVE_ALL、struct trapframe 和 trap() 定义

[OUTPUT]
生成完整的 RESTORE_ALL 宏定义和 __alltraps 函数中 #lab3 function1 标记后的代码
````

#### 功能模块二提示词

```text
请实现 kern/trap/trap.c 中 interrupt_handler(struct trapframe *tf)，只修改该函数及其所需的头文件引用/宏定义，不修改 exception_handler、trap_dispatch、trap 及其他组员代码。

上下文：tf->cause 来自 RISC-V scause。最高位为 1 表示中断，低位为中断原因号；libs/riscv.h 定义 IRQ_S_TIMER、IRQ_S_SOFT 等常量。kern/driver/clock.h 提供 volatile size_t ticks 和 clock_set_next_event()；libs/sbi.h 提供 sbi_shutdown()。文件已有 TICK_NUM 及 print_ticks()，DEBUG_GRADE 分支输出测试结束信息并调用 panic()。

要求：
1. 对 IRQ_S_TIMER 先设置下一次时钟事件，再将 ticks 增加 1。
2. ticks 每达到 TICK_NUM 的整数倍时调用 print_ticks()；统计输出次数达到 SHUTDOWN_AFTER 后调用 sbi_shutdown()。
3. 如果 SBI 关机调用返回，不得继续返回到正常内核执行流。
4. 对其他已定义中断输出类型提示；未知中断输出原因并调用 print_trapframe(tf)。
5. 不修改 tf->epc，不让非时钟中断误改 ticks；考虑原因号宽度和整数打印格式。
6. 保留原有函数签名和 DEBUG_GRADE 测试行为。

请先检查项目已有宏、接口和输出风格，再给出最小实现，并说明计数边界和关机路径。
```


#### 功能模块三提示词

````text
[PROMPT] 实现 kern/trap/trap.c 中的 exception_handler(struct trapframe *tf)，正确处理非法指令、断点和其他异常，并保证 Trap 返回位置正确。

[CONTEXT]
汇编入口已经保存处理器上下文，并通过 kern/trap/trap.h 中的 struct trapframe 把 Trap 信息传给 trap(struct trapframe *tf)。tf->cause 保存 scause 的异常原因码，tf->epc 保存触发异常的指令地址，tf->status 保存陷入前的 sstatus。kern/trap/trap.c 已有 print_trapframe(tf) 和 trap_in_kernel(tf) 可用；libs/riscv.h 定义了 CAUSE_ILLEGAL_INSTRUCTION 和 CAUSE_BREAKPOINT。

[TASK]
实现：
void exception_handler(struct trapframe *tf);

1. 当 tf->cause == CAUSE_ILLEGAL_INSTRUCTION 时，输出包含 "Illegal instruction at" 及 tf->epc 的诊断信息，然后根据该指令编码推进 tf->epc。
2. 当 tf->cause == CAUSE_BREAKPOINT 时，输出包含 "Breakpoint at" 及 tf->epc 的诊断信息，然后根据断点指令编码推进 tf->epc。
3. 对其他异常调用 print_trapframe(tf) 作为兜底；按本实验接口约定从处理函数正常返回，不修改未知异常的 EPC。
4. 对本实验范围内的压缩和普通指令正确区分 2/4 字节长度，禁止对所有异常无条件固定加 4。

[CONSTRAINTS]
- 保持 exception_handler(struct trapframe *tf) 
- 不修改 interrupt_handler()、trap_dispatch()、trap()、Trap frame 结构体、trapentry.S 或其他组员负责的代码。
- 不修改中断处理逻辑和时钟计数。

````