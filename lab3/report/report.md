# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| 实验名称 | Lab 3：中断与异常处理 |
| 小组成员 | 2411843-王鹏钧、2412854-于子朔、2412855-张雅程 |
| 实验日期 | 2026-11-3 |
| 源码目录 | `OS/lab3/code` |

### 小组分工

| 成员 | 负责的练习/模块 | 主要成果 |
|------|----------------|----------|
| 2411843-王鹏钧 | 功能模块一：Trap 上下文恢复与 Trap 入口功能 | 完成 `RESTORE_ALL` 宏与 `__alltraps` 入口函数的实现，实现 Trap 从进入内核到返回原执行流的完整上下文管理 |
| [成员2] | 功能模块二：中断与异常处理功能 | [待填写] |
| [成员3] | [功能模块三] | [待填写] |

---

## 一、实验目的




---

## 二、实验环境

### 2.1 软件与工具

| 项目 | 本次实验环境 |
|------|--------------|
| 宿主系统 | Windows，使用 WSL 2 |
| Linux 发行版 | Ubuntu 22.04 LTS |
| 交叉编译器 | `riscv64-unknown-elf-gcc` 10.2.0 |
| 链接器 | `riscv64-unknown-elf-ld` 2.35.1 |
| 模拟器 | QEMU 6.2.0 |
| 固件 | QEMU 自带 OpenSBI |
| 调试器 | GNU gdb 12.1 |
| 模拟平台 | `virt`，RISC-V 64 位，单 HART，128 MiB DRAM |
| 源码目录 | `OS/lab3/code` |

### 2.2 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 2411843-王鹏钧 | Claude Code | Claude Opus | |
| [成员2] | [待填写] | [待填写] | [待填写] |
| [成员3] | [待填写] | [待填写] | [待填写] |

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线



### 3.2 功能的逐步实现



### 3.3 核心文件与模块



## 四、实验内容与实现

### 4.1 功能模块一：Trap 上下文恢复与 Trap 入口功能

#### 任务描述

实现 `kern/trap/trapentry.S` 中的两个核心部分：

1. **`RESTORE_ALL` 宏**：负责完成 Trap 从"进入内核"到"返回原执行流"之间的底层上下文恢复。
2. **`__alltraps` 函数**：负责完成 Trap 入口与 C 语言 Trap 处理函数之间的衔接。




#### 提示词

````text
[PROMPT] 实现 kern/trap/trapentry.S 中 RESTORE_ALL 宏和 __alltraps 函数

[CONTEXT]
RISC-V Supervisor 模式下，Trap（中断或异常）发生时硬件会自动：
1. 保存当前 PC 到 sepc
2. 保存 Trap 原因到 scause
3. 保存出错地址（若适用）到 sbadaddr  
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



### 4.2 功能模块二：中断与异常处理功能（[成员2]）

#### 任务描述

[待填写]

#### 提示词

[待填写]

#### 实现与验证

[待填写]

---

### 4.3 功能模块三：[模块名称]（[成员3]）

#### 任务描述

[待填写]

#### 提示词

[待填写]

#### 实现与验证

[待填写]

---

## 五、实验结果

### 功能验证

[待其他模块完成后补充完整的运行截图和测试结果]

---

## 六、实验总结与思考

### 6.1 实验收获

通过本次实验的功能模块一，我深入理解了以下内容：

1. **Trap 处理的底层机制**：硬件只保存最基本的 CSR（sepc、scause 等），软件必须手动保存恢复所有通用寄存器，这种分工体现了 RISC 架构的设计哲学——硬件简单，软件灵活。

2. **上下文切换的完整性**：任何一个寄存器的保存或恢复出错，都会导致程序崩溃。Trap frame 的布局、保存和恢复必须严格一一对应，这要求极高的细致度。

3. **栈的双重用途**：在 Trap 处理过程中，sp 既指向 Trap frame（用于访问保存的寄存器），又是需要恢复的目标。理解这种双重角色对正确实现 RESTORE_ALL 至关重要。

4. **RISC-V 调用约定的重要性**：函数调用必须遵守 ABI 规范，caller-saved 和 callee-saved 寄存器的区分、参数传递规则等，都直接影响 Trap 处理的正确性。

5. **AI 辅助编程的有效方法**：通过清晰的提示词（包含上下文、约束、输入输出规范），AI 能够生成符合要求的底层汇编代码。但生成后必须人工验证逻辑正确性，特别是寄存器操作的顺序和依赖关系。

### 6.2 遇到的问题与解决

1. **sp 恢复的理解**：最初不确定 `LOAD sp, 2*REGBYTES(sp)` 是否会产生问题（用 sp 计算地址的同时修改 sp）。通过查阅 RISC-V 指令手册，确认 LOAD 指令先计算地址再写回，因此这种写法是安全的。

2. **哪些 CSR 需要恢复**：SAVE_ALL 保存了 sstatus、sepc、sbadaddr、scause 四个 CSR，但只需要恢复前两个。理解了 sbadaddr 和 scause 是只读的（由硬件设置），软件无需恢复。

3. **提示词的设计**：如何让 AI 理解 Trap frame 布局和恢复顺序是关键。通过在提示词中明确列出 SAVE_ALL 的保存顺序和布局，以及恢复的约束条件，AI 能够生成正确的代码。

### 6.3 思考题

[待实验指导书中的思考题填写]

