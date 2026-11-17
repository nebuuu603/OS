# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| 实验名称 | Lab 4：进程管理 |
| 小组成员 | 2411843 王鹏钧、2412854 于子朔、2412855 张雅程 |
| 实验日期 | 2026-11-17 |
| 源码目录 | `OS/lab4/code` |
| 评分结果 | `make grade` 30/30 |

### 小组分工

| 成员 | 负责模块 | 主要成果 |
|------|----------|----------|
| 2411843 王鹏钧 | 功能模块一：虚拟内存页操作 | 在 `kern/mm/pmm.c` 实现 `page_insert()` 与 `page_remove()`，处理同页重复映射、引用计数与 TLB 失效 |
| 2412854 于子朔 | 功能模块二：进程创建与初始化 | 在 `kern/process/proc.c` 实现 `alloc_proc()`、`proc_init()`、`kernel_thread()`、`copy_thread()` |
| 2412855 张雅程 | 功能模块二收尾 + 功能模块三 + 集成验证 | 实现 `copy_mm()`、`do_fork()`、`cpu_idle()` 与 `kern/schedule/sched.c` 的 `schedule()`；完成 WSL 环境适配、官方评分验证与报告整理 |

说明：手册把 `do_fork`/`copy_mm` 列在功能模块二，本组按三人工作量把它们与功能模块三一起交给成员 C；`proc_run()` 与 `switch.S` 的 `switch_to` 在骨架中已给出，未再改动。

---

## 一、实验目的

1. 理解虚拟内存页操作的基本接口：把物理页映射到线性地址、撤销映射，并保持页表项、引用计数和 TLB 三者一致。
2. 理解进程控制块（PCB）的字段含义，以及内核线程为什么共享内核地址空间。
3. 理解进程创建流程：`alloc_proc` → 内核栈 → 地址空间处理 → 首次运行现场 → 登记与唤醒。
4. 理解 `trapframe` 与 `context` 的分工，以及 `forkret → forkrets → __trapret → kernel_thread_entry` 的首次运行链路。
5. 理解 FIFO/轮转调度器、idleproc 的保底作用，以及调度与上下文切换中的临界区保护。
6. 练习用提示词让 AI 生成符合既有框架约束的内核代码，并用官方评分用例而不是“能启动”来验证结果。

---

## 二、实验环境

### 2.1 软件与工具

| 项目 | 实际实验环境 |
|------|--------------|
| 宿主系统 | Windows，使用 WSL 2 |
| Linux 发行版 | Ubuntu 22.04.5 LTS（`Linux 6.6.87.2-microsoft-standard-WSL2 x86_64`） |
| 交叉编译器 | `riscv64-unknown-elf-gcc` 10.2.0（`/usr/bin`） |
| 链接器 | `riscv64-unknown-elf-ld` 2.35.1 |
| 调试器 | GNU gdb 12.1（`riscv64-unknown-elf-gdb`，`/usr/local/bin`） |
| 模拟器 | QEMU 6.2.0（Debian `1:6.2+dfsg-2ubuntu6.31`，`/usr/bin`） |
| 固件 | QEMU 默认 OpenSBI v0.9，Runtime SBI 0.2 |
| 模拟平台 | `virt`，RISC-V 64 位，单 HART，128 MiB DRAM |
| 评分结果 | `make grade` 30/30 |

完整版本与路径记录见 [logs/environment.txt](logs/environment.txt)。

**与手册示例的差异。** 手册示例使用 GCC-Metal 10.2.0、QEMU 4.1.1；本机是课程 GCC 10.2.0 + QEMU 6.2.0，与 lab2 的报告环境一致。需要额外说明的是，本机 WSL 的登录 shell 会把 `/home/zyc/riscv-elf-toolchains/bin`（GCC 15.1.0）与 `/opt/qemu/bin`（QEMU 7.0.0）放进 `PATH` 前面；本文的评分和全部日志都在未加载该 PATH 的默认 shell 中产生。若在登录 shell 中构建，需要显式指定 `GCCPREFIX=/usr/bin/riscv64-unknown-elf-`，并注意 GCC 15 默认把 `int` 到指针的隐式转换、函数指针类型不匹配视为错误。

**启动参数适配。** 本机 OpenSBI 为 fw_dynamic 形式：使用 `-device loader,file=bin/ucore.img,addr=0x80200000` 时，固件输出的 `Domain0 Next Address` 仍为 `0x0`，内核根本不执行（表现为串口只剩固件信息，三组用例全部 `missing`，0/30）。改为 `-kernel bin/ucore.img` 后 `Domain0 Next Address = 0x80200000`，内核正常启动。lab2 已记录过同一问题，本组沿用既有做法，把 `lab4/code/Makefile` 的 `qemu`/`debug` 目标与 `tools/grade.sh` 的 `qemuopts` 改为 `-kernel`；这只改变镜像交给固件的方式，不改变内核代码与链接地址。

### 2.2 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 2411843 王鹏钧 | Claude Code | Claude Opus 5.5 | 辅助页表操作实现与边界审查 |
| 2412854 于子朔 | Codex 桌面应用 | GPT-6.1 | 辅助进程创建与初始化实现 |
| 2412855 张雅程 | DeepSeek Harness | deepseek-flash | 辅助 `do_fork`/`schedule` 实现、环境定位、WSL 验证与报告整理 |

---

## 三、实验整体逻辑分析

### 3.1 本章节的逻辑主线

lab4 要在 lab2 的物理内存管理和 lab3 的 Trap 机制之上，把内核从“只有一个执行流”推进到“能创建、登记、调度多个内核线程并完成上下文切换”。整章的推进顺序可以概括为：

```text
kern_init()
    → pmm_init()            物理内存与页表（lab2）
    → pic_init()/idt_init() Trap 入口（lab3）
    → vmm_init()            mm/vma 结构自检
    → proc_init()           建立 idleproc，创建 initproc
        → alloc_proc()      PCB
        → kernel_thread()   构造首次运行 trapframe
        → do_fork()         PCB + 内核栈 + 登记 + 唤醒
        → copy_thread()     trapframe/context 放到新内核栈
    → cpu_idle()            idleproc 发现 need_resched 后让出 CPU
        → schedule()        在 proc_list 中选下一个 PROC_RUNNABLE
        → proc_run()        切 current、切 satp、切上下文
        → switch_to()       保存/恢复 callee-saved 寄存器
    → forkret() → forkrets() → __trapret → kernel_thread_entry → 线程函数
```

两条主线互相配合：`vmm` 提供地址空间模型（本实验只做预映射），`proc` 提供执行实体；内核线程共享同一个内核页表，所以 `mm` 为 `NULL`、`pgdir` 指向 `boot_pgdir_va`。

### 3.2 功能的逐步实现

1. **完成页表操作**：在 `get_pte()` 的基础上实现 `page_insert()`/`page_remove()`，让“物理页 ↔ 线性地址”的映射可以建立、覆盖和撤销，并通过 `check_pgdir()`/`check_boot_pgdir()`。
2. **建立 PCB**：`alloc_proc()` 用 `kmalloc` 得到干净的 `proc_struct`，把状态置为 `PROC_UNINIT`、`pid=-1`、`pgdir=boot_pgdir_va`，链表项自环。
3. **包装 idleproc**：`proc_init()` 把当前执行现场登记为 pid 0 的 idleproc（栈用 `bootstack`、`need_resched=1`），并把它设为 `current`。
4. **创建 initproc**：`kernel_thread(init_main, "Hello world!!", 0)` 构造临时 trapframe，经 `do_fork()` 生成 pid 1 的内核线程。
5. **搭建首次运行现场**：`copy_thread()` 把 trapframe 放到新内核栈顶，`a0=0`、`sp=tf`，`context.ra=forkret`、`context.sp=tf`。
6. **完成调度器**：`schedule()` 在 `proc_list` 中按 FIFO/轮转选下一个 `PROC_RUNNABLE`；`cpu_idle()` 在 `need_resched` 时调用它。
7. **切换执行现场**：`proc_run()` 更新 `current`、`runs`、`satp` 并 `switch_to()`；新线程经 `forkret` 恢复 trapframe，从 `kernel_thread_entry` 开始执行。
8. **验证**：官方 `make grade` 注入三组用例，分别检查页表操作、进程创建字段和调度轮转。

### 3.3 核心文件与模块

| 文件或模块 | 主要作用 |
|------------|----------|
| `kern/mm/pmm.c` | `page_insert()`、`page_remove()`、`get_pte()`，页表映射维护 |
| `kern/mm/vmm.c` | `mm_struct`/`vma_struct` 的创建、插入、查找与销毁 |
| `kern/process/proc.c` | PCB 分配、idle/init 创建、`kernel_thread`、`do_fork`、`copy_thread`、`cpu_idle` |
| `kern/process/proc.h` | `proc_struct`、`context`、进程状态与函数声明 |
| `kern/process/switch.S` | `switch_to`：保存/恢复 ra、sp、s0~s11 |
| `kern/process/entry.S` | `kernel_thread_entry`：把参数放入 `a0`，调用线程函数，返回后 `do_exit` |
| `kern/trap/trapentry.S` | `__alltraps`/`__trapret`/`forkrets`，trapframe 保存恢复与首次运行恢复 |
| `kern/schedule/sched.c` | `wakeup_proc()` 与 `schedule()` |
| `tools/test_lab4.c`、`tools/inject.py`、`tools/grade.sh` | 官方评分：注入三组用例、启动 QEMU、匹配 PASS 行 |

---

## 四、实验内容与实现

### 4.1 功能模块一：虚拟内存页操作（2411843 王鹏钧）

#### 任务描述

在 `kern/mm/pmm.c` 中实现：

```c
int  page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
void page_remove(pde_t *pgdir, uintptr_t la);
```

要求建立或更新 `la` 到 `page` 的映射，并在撤销映射时正确维护引用计数；`get_pte(..., 1)` 可能为中间级页表分配失败，必须返回 `-E_NO_MEM` 且不留半完成映射；对未映射地址调用 `page_remove` 必须安静返回。

#### 提示词

```text
[PROMPT] 实现 kern/mm/pmm.c 中 page_insert() 与 page_remove()

[CONTEXT]
get_pte(pgdir, la, create) 已经能定位页表项，create=1 时按需分配中间级页表页。
可用接口：pte_create(ppn, type)、pte2page()、page2ppn()、page_ref()/page_ref_inc()/page_ref_dec()、
free_page()、tlb_invalidate()、alloc_page()、struct Page、boot_pgdir_va。
页表项 PTE_V 表示有效；pte_create() 会自动置 PTE_V。

[TASK]
page_insert：
1. get_pte(pgdir, la, 1)；失败返回 -E_NO_MEM；
2. 对 page 先 page_ref_inc；
3. 若该地址已有有效映射：
   - 旧页与 page 相同：撤销本次 inc，只更新权限，tlb_invalidate 后返回 0；
   - 旧页不同：page_remove() 掉旧映射；
4. 写 *ptep = pte_create(page2ppn(page), perm)，并 tlb_invalidate。

page_remove：
1. get_pte(pgdir, la, 0)：NULL 或 PTE_V 未置位则直接返回（幂等）；
2. 取 pte2page，page_ref_dec；引用计数为 0 时 free_page；
3. 清 *ptep = 0，并 tlb_invalidate。

[CONSTRAINTS]
不修改 get_pte()、check_pgdir()、check_boot_pgdir() 和结构体定义；
不引入 libc；保留函数签名与已有注释。
```

#### 实现与验证

`page_insert()`（`kern/mm/pmm.c:333`）先 `get_pte(pgdir, la, 1)`，失败返回 `-E_NO_MEM`；成功则 `page_ref_inc(page)`。若目标页表项已有效，就用 `pte2page()` 取出旧页：旧页与新页相同则回退本次引用并只改权限；不同则先 `page_remove()` 旧映射，再写入新页表项。两条路径最后都调用 `tlb_invalidate()`，保证 MMU 不会继续使用旧翻译。

`page_remove()`（`kern/mm/pmm.c:361`）以 `get_pte(pgdir, la, 0)` 查询而不是创建页表：指针为 `NULL` 或页表项无效时直接返回，因此对未映射地址重复调用是安全的；有效时递减引用计数，计数归零才 `free_page()`，最后清空页表项并失效 TLB。

这两条路径正好与 `check_pgdir()` 的断言对应：同一物理页映射两个地址时 `page_ref(p1) == 2`；把 `p1` 覆盖到原本映射 `p2` 的地址后 `page_ref(p1) == 2` 且 `page_ref(p2) == 0`；`page_remove` 后计数逐级归零。`check_boot_pgdir()` 还会通过 `strcpy((void *)0x100, ...)`/`strcmp()` 真正访问映射，验证 TLB 失效和权限位是生效的，而不仅仅是页表项内容正确。

#### 实现时确认的边界

- **同页重复映射**：若直接 `page_ref_inc` 后覆盖页表项，就会丢掉一次对旧页的 `dec`，引用计数永久偏大；实现里专门分支处理。
- **旧页被释放的顺序**：先 `inc` 新页再 `remove` 旧映射，避免新旧页相同时把页释放掉。
- **失败不留痕**：`get_pte` 分配失败时直接返回，未修改任何页表项，调用者可以安全放弃。
- **幂等删除**：`page_remove` 不把“本来就没有映射”当作错误；测试里对同一地址连续两次 `page_remove` 就是这条约束。

### 4.2 功能模块二：进程创建与初始化（2412854 于子朔；`do_fork`/`copy_mm` 由 2412855 完成）

#### 4.2.1 `alloc_proc()`

`kern/process/proc.c:256` 用 `kmalloc(sizeof(struct proc_struct))` 取得 PCB；成功时 `memset` 清零，置 `state=PROC_UNINIT`、`pid=-1`，`pgdir=(uintptr_t)boot_pgdir_va`（内核线程共享内核页表，`proc_run` 会用 `PADDR` 转换），并 `list_init` 两个链表项。清零保证 `context.ra == 0`、`tf == NULL`、`runs == 0` 等字段处于可判断的初值，这正是评分用例对 idleproc 的断言。

#### 4.2.2 `proc_init()` 与 idleproc

`kern/process/proc.c:300` 先初始化 `proc_list` 与 1024 条 `hash_list`，再 `alloc_proc()` 得到 idleproc：`pid=0`、`state=PROC_RUNNABLE`、`kstack=bootstack`、`need_resched=1`、名字 `"idle"`，并设为 `current`、`nr_process++`。idleproc 直接复用从 `kern_init` 到现在的执行现场，因此它不进入 `proc_list`/`hash_list`，只通过全局指针引用。

随后 `kernel_thread(init_main, "Hello world!!", 0)` 创建 initproc；`find_proc(pid)` 通过 hash 链表找回 PCB，断言 pid 为 1 后命名为 `"init"`。这一步保证系统启动后有“继承现场的 idle”和“第一个真正能跑的线程 init”两个实体。

#### 4.2.3 `kernel_thread()` 与 `copy_thread()`

`kernel_thread()`（`kern/process/proc.c:274`）在栈上构造临时 trapframe：`gpr.s0=fn`、`gpr.s1=arg`、`epc=kernel_thread_entry`，`status` 取当前 `sstatus` 后置 `SPP|SPIE`、清 `SIE`。`SPP=1` 使 `sret` 返回 S 模式（内核线程不落到用户态），`SPIE=1` 让返回后允许中断，而清 `SIE` 保证构造和切换期间不被时钟中断打断。随后调用 `do_fork(clone_flags | CLONE_VM, 0, &tf)`。

`copy_thread()`（`kern/process/proc.c:288`）把 trapframe 放到新内核栈顶：

```c
proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1;
*proc->tf = *tf;
proc->tf->gpr.a0 = 0;
proc->tf->gpr.sp = (esp != 0) ? esp : (uintptr_t)proc->tf;
proc->context.ra = (uintptr_t)forkret;
proc->context.sp = (uintptr_t)proc->tf;
```

`- 1` 让 trapframe 正好落在 `kstack + KSTACKSIZE - sizeof(trapframe)`，不会越过内核栈；`a0=0` 表示“子线程返回 0”；`context.sp` 指向 trapframe，`context.ra` 指向 `forkret`，两者共同构成 `switch_to` 恢复后的入口。

#### 4.2.4 `do_fork()` 与 `copy_mm()`

`do_fork()`（`kern/process/proc.c:359`）按“要么全部成功、要么逐级回收”的顺序完成 7 件事：

1. `nr_process >= MAX_PROCESS` 时返回 `-E_NO_FREE_PROC`；
2. `alloc_proc()`，失败返回 `-E_NO_MEM`；
3. 记录 `proc->parent = current`；
4. `setup_kstack()` 分配 `KSTACKPAGE` 页内核栈，失败 `kfree(proc)`；
5. `copy_mm()`，失败 `put_kstack` 后 `kfree(proc)`；
6. `copy_thread()` 布置首次运行现场；
7. 关中断临界区内分配 `pid`、挂 `hash_list`/`proc_list`、`nr_process++`，然后 `wakeup_proc(proc)`，返回 pid。

把登记动作放在所有可能失败的操作之后，是为了不让 `find_proc()` 看到半初始化的 PCB：`pid`、hash 链和 `proc_list` 三者必须在同一临界区内一起可见。失败标签按相反顺序释放资源（`bad_fork_cleanup_kstack` → `bad_fork_cleanup_proc`），避免只释放 PCB 而泄漏两页内核栈。

`copy_mm()`（`kern/process/proc.c:338`）在本实验里是“地址空间处理接口的占位”：内核线程的 `current->mm` 为 `NULL`，子进程继承 `NULL`；`CLONE_VM` 时共享父进程的 `mm`；真正复制用户地址空间留给后续实验。评分用例断言的 `initproc->mm == NULL` 正是这条路径的结果。

#### 提示词

```text
[PROMPT] 实现 kern/process/proc.c 中 alloc_proc、proc_init、kernel_thread、copy_thread、
        do_fork、copy_mm 六个函数（本组拆成两次提交）

[CONTEXT]
proc_struct 含 state/pid/runs/kstack/need_resched/parent/mm/context/tf/pgdir/flags/name/list_link/hash_link。
proc_init() 由 kern_init 调用，此时 current 尚未建立；idleproc 需要复用 bootstack 和当前现场。
kernel_thread() 是“用函数和参数造线程”的入口，do_fork() 负责资源与登记，copy_thread() 负责首次运行现场。
forkret/forkrets/__trapret/kernel_thread_entry 已存在：forkret 调 forkrets(current->tf)，
forkrets 把 sp 指向 tf 后跳 __trapret，__trapret 恢复 trapframe 并 sret 到 epc=kernel_thread_entry，
入口执行 move a0, s1; jalr s0 后调 do_exit。

[TASK]
alloc_proc：kmalloc + memset；state=PROC_UNINIT；pid=-1；pgdir=boot_pgdir_va；list_init 两个链表项。
proc_init：初始化 proc_list/hash_list；建立 idleproc(pid=0, RUNNABLE, kstack=bootstack,
need_resched=1, name="idle") 并设为 current；kernel_thread 创建 initproc；find_proc + 命名。
kernel_thread：构造 tf（s0=fn, s1=arg, epc=kernel_thread_entry,
status=(sstatus|SPP|SPIE)&~SIE），do_fork(clone_flags|CLONE_VM, 0, &tf)。
copy_thread：tf 放在 kstack+KSTACKSIZE 向下取一个 trapframe；a0=0；sp 用 esp 或 tf；
context.ra=forkret；context.sp=tf。
do_fork：7 步（见报告 4.2.4），失败按 kstack→PCB 顺序回收，登记动作放在关中断临界区。
copy_mm：mm==NULL 直接继承；CLONE_VM 共享；不实现用户地址空间复制。

[CONSTRAINTS]
idleproc 不挂 proc_list/hash_list；do_fork 的 pid/hash/list/nr_process 必须在同一临界区；
wakeup_proc 要求目标不是 ZOMBIE/RUNNABLE；保留 `//LAB4 function2 2412854`、`//LAB4 function3 2412855` 注释。
```

#### 实现时确认的边界

- **内核栈与 PCB 的回收顺序**：先分配的 PCB 后释放，先分配的内核栈先释放；标签顺序写反会二次释放或泄漏。
- **trapframe 的位置**：必须用 `kstack + KSTACKSIZE - sizeof(trapframe)`；若直接拿 `kstack` 或忘记减一，第一次 `sret` 就会写到栈外。
- **`status` 位组合**：只置 `SPP|SPIE` 而忘记清 `SIE`，trapframe 恢复后可能立刻进中断，切换过程被打断；反之漏 `SPIE` 会让新线程一直关中断。
- **登记时机**：`kernel_thread` 返回 pid 后 `proc_init` 立刻 `find_proc`，若 `hash_proc` 没做会直接 `NULL`。
- **`runs` 计数只加一次**：`proc_run()` 已经 `proc->runs++`，`schedule()` 不能再加，否则评分用例对 `idleproc->runs == 0` 的断言会失败。

### 4.3 功能模块三：进程调度（2412855 张雅程）

#### 4.3.1 `cpu_idle()`

`kern/process/proc.c:417` 是 idleproc 的主体：

```c
void cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}
```

`proc_init` 把 idleproc 的 `need_resched` 置 1，因此 `kern_init` 最后调用 `cpu_idle()` 时会立刻让出 CPU；之后每次被唤醒回到这里，只要没有新的调度请求就继续空转。本实验的调度点只有这里（时钟中断本身不调用 `schedule`），所以用例通过直接置 `need_resched` 再调用 `schedule()` 来驱动。

#### 4.3.2 `schedule()`

`kern/schedule/sched.c:21` 实现 FIFO/轮转选择：

```c
current->need_resched = 0;
last = (current == idleproc) ? &proc_list : &(current->list_link);
le = last;
do {
    if ((le = list_next(le)) != &proc_list) {
        next = le2proc(le, list_link);
        if (next->state == PROC_RUNNABLE) break;
    }
} while (le != last);
if (next == NULL || next->state != PROC_RUNNABLE) next = idleproc;
if (next != current) proc_run(next);
```

三个分支对应手册的三种情况：

- **当前是 idleproc**（不在 `proc_list` 中）：从表头开始扫描，给所有线程平等机会；
- **当前不是 idleproc**：从 `current->list_link` 的下一个开始扫描，实现轮转，避免固定优先级；
- **找不到可运行线程**：退回 idleproc 保底。

整个选择与切换在 `local_intr_save`/`local_intr_restore` 之间完成，防止“选中 next 之后、`current` 更新之前”被中断打断，造成两个执行流同时认为自己该运行。若 `next == current`，不调用 `proc_run()`（它本身也会提前返回），直接恢复中断。

#### 4.3.3 `proc_run()` 与 `switch_to()`（骨架提供）

`proc_run()`（`kern/process/proc.c:223`）在关中断后更新 `current`、`proc->runs++`，把 `satp` 切到 `proc->pgdir`（`pgdir` 非 0 时用 `PADDR` 转物理地址，否则用 `boot_pgdir_pa`），执行 `flush_tlb()`，最后 `switch_to(&prev->context, &proc->context)`。`switch_to`（`kern/process/switch.S:5`）只保存/恢复 `ra`、`sp`、`s0~s11` 这些 callee-saved 寄存器：调用者保存寄存器由编译器在 `schedule()`/`proc_run()` 的调用点负责，这也是 `context` 只有 14 个字段的原因。

#### 首次运行链路

新线程第一次被调度时，`switch_to` 恢复出的 `ra` 是 `copy_thread` 写入的 `forkret`，`sp` 是它的 trapframe。`forkret()` 调用 `forkrets(current->tf)`；`forkrets`（`kern/trap/trapentry.S:117`）把 `sp` 指向该 trapframe 后跳到 `__trapret`；`RESTORE_ALL` 恢复所有通用寄存器和 `sstatus`/`sepc`，`sret` 回到 `epc=kernel_thread_entry`；入口用 `move a0, s1; jalr s0` 把 arg 放进 a0 并调用线程函数。线程函数返回后执行 `jal do_exit`，即本实验骨架里的 `panic("process exit!!.")`。

#### 提示词

```text
[PROMPT] 实现 kern/process/proc.c 的 cpu_idle() 与 kern/schedule/sched.c 的 schedule()

[CONTEXT]
proc_list 是全部可调度 PCB 的线性表；idleproc 只在全局指针中，不在这条链表里。
proc_run(proc) 已实现：切 current、runs++、切 satp、switch_to；它对 proc==NULL || proc==current 直接返回。
wakeup_proc(proc) 只把状态置 PROC_RUNNABLE，断言目标不是 ZOMBIE/RUNNABLE。
local_intr_save/restore 在 kern/sync/sync.h；PROC_RUNNABLE 是唯一可被选中的状态。
测试用例会创建两个 kernel_thread，反复置 current->need_resched=1 并调用 schedule()，
要求两个测试线程都被执行并各自让出 CPU。

[TASK]
cpu_idle：while (1) { if (current->need_resched) schedule(); }
schedule：
1. 关中断；current->need_resched = 0；
2. last = (current == idleproc) ? &proc_list : &(current->list_link)；
3. 从 last 之后循环扫描 proc_list，跳过表头，遇到第一个 PROC_RUNNABLE 即选中；
4. 找不到或状态又不合法时退回 idleproc；
5. next != current 才 proc_run(next)；恢复中断。

[CONSTRAINTS]
不得修改 proc_run、switch_to、wakeup_proc；不得在 schedule 里重复 runs++；
必须处理 idleproc 不在 proc_list 的情况；扫描必须能在整条链表上回绕并终止。
```

#### 实现时确认的边界

- **扫描起点**：若总是从 `proc_list` 表头开始，选中的永远是同一个线程，测试里的两个线程无法都运行；必须用 `current->list_link` 作为起点。
- **表头跳过**：`list_next()` 会走到 `&proc_list`（哨兵），必须先判断再 `le2proc`，否则会把哨兵当 PCB。
- **回绕终止**：`do-while (le != last)` 保证走完一圈；`next` 在扫描中被赋值为“最后一个检查过的项”，循环后必须重新检查状态，否则可能选中非 RUNNABLE 的线程。
- **临界区**：选择与切换都在关中断区间内；否则中断处理里再次调度会破坏 `current` 的一致性。
- **`runs` 只加一次**：`proc_run` 已经计数，`schedule` 不能重复加。

### 4.4 思考题

#### 4.4.1 `local_intr_save`/`local_intr_restore` 如何实现开关中断

```c
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();          // 清 sstatus.SIE
        return 1;
    }
    return 0;
}
static inline void __intr_restore(bool flag) {
    if (flag) intr_enable();     // 置 sstatus.SIE
}
```

`local_intr_save(x)` 先读 `sstatus.SIE`：原来开着才关中断并返回 1，原来已关就返回 0。恢复时只有返回 1 才重新开中断，因此**嵌套临界区不会提前开中断**：外层保存 1，内层保存 0，内层恢复不动 SIE，外层恢复才打开。这等价于“保存并清标志 + 按原值恢复”，比直接 `intr_disable()/intr_enable()` 安全。注意它是单 HART 的本地屏蔽，不能代替多核自旋锁。

#### 4.4.2 `get_pte()` 两段相似代码与 Sv32/Sv39/Sv48

`get_pte()` 的两段代码分别处理 `PDX1`（根页表）和 `PDX0`（中间页表）：都先判断页表项 `PTE_V`，未存在时 `alloc_page()`、`set_page_ref(page,1)`、`memset(KADDR(pa),0,PGSIZE)`，再写 `pte_create(page2ppn(page), PTE_U|PTE_V)`，最后用 `KADDR(PDE_ADDR(...))` 进到下一级。它们相似，是因为**每一级页表的结构和分配逻辑完全相同**：都是一页 4 KiB、512 项、每项 8 字节，都只依赖 `PTE_V` 判断“这一级是否已经建立”。

Sv32/Sv39/Sv48 的差别只在级数与位宽：Sv32 是两级（`PDX1`、`PTX`），Sv39 是三级（`PDX1`、`PDX0`、`PTX`），Sv48 是四级；每级索引位宽都是 9 位，`PGOFF` 都是 12 位。所以三级页表的遍历就是“同一段逻辑重复两次再取叶子项”，两段代码自然长得一样；若扩展到 Sv48，只是在根之前再加一段同样的代码。

#### 4.4.3 `get_pte()` 是否应该拆分

`get_pte()` 同时承担“查页表项”和“按需分配页表页”两件事。拆分的好处是职责单一：查询路径不分配、失败语义清晰，也便于统计页表页开销；坏处是调用者要自己串起多级分配，接口变复杂。本实验保留框架原有写法，是因为 `page_insert()`/`page_remove()` 都依赖“一次定位到叶子项”的语义，且测试直接比较 `get_pte(...,0)` 的返回值。若在真实内核中做，通常会把“分配页表页”抽成独立函数（如 Linux 的 `pte_alloc`），并让查询路径保证无副作用。

---

## 五、测试与验证

### 5.1 编译与启动

`make qemu` 的完整输出见 [logs/qemu-output.txt](logs/qemu-output.txt)，关键片段：

```text
+ cc kern/process/proc.c
+ cc kern/schedule/sched.c
……
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img

OpenSBI v0.9
……
Firmware Size             : 100 KB
Runtime SBI Version       : 0.2

Domain0 Name              : root
……
Domain0 Next Address      : 0x0000000080200000
Domain0 Next Mode         : S-mode

DTB Init
HartID: 0
DTB Address: 0x87000000
Physical Memory from DTB:
  Base: 0x0000000080000000
  Size: 0x0000000008000000 (128 MB)
  End:  0x0000000087ffffff
DTB init completed
(THU.CST) os is loading ...

Special kernel symbols:
  entry  0xc020004a (virtual)
  etext  0xc0203e7a (virtual)
  end    0xc020d4e4 (virtual)
Kernel executable memory footprint: 54KB
memory management: default_pmm_manager
check_alloc_page() succeeded!
check_pgdir() succeeded!
check_boot_pgdir() succeeded!
use SLOB allocator
kmalloc_init() succeeded!
check_vma_struct() succeeded!
check_vmm() succeeded.
++ setup timer interrupts
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

| 验证项目 | 实测结果 |
|----------|----------|
| 固件交接 | `Domain0 Next Address=0x80200000`，S 模式 |
| 设备树探测 | base `0x80000000`、size `0x8000000`（128 MB）、end `0x87ffffff` |
| 内核符号 | `entry=0xc020004a`、`end=0xc020d4e4`（虚拟地址），footprint 54KB |
| 页表自检 | `check_pgdir()`/`check_boot_pgdir()` 均 succeeded |
| VMM 自检 | `check_vma_struct()`/`check_vmm()` 均 succeeded |
| 首次调度 | initproc（pid 1）打印三行字符串 |

日志最后的 `kernel panic at kern/process/proc.c:209: process exit!!.` 属于预期：lab4 的 `do_exit()` 仍是骨架里的 `panic`，`init_main` 返回后由 `kernel_thread_entry` 的 `jal do_exit` 触发；进程退出与回收是后续实验的内容。

### 5.2 `make grade` 评分

`make grade` 会把代码树复制到临时目录，注入 `tools/test_lab4.c`，重新编译后在 QEMU 中运行并匹配输出。本机结果：

```text
lab4 injection complete
  -check PMM:                                OK
  -check Process Create:                     OK
  -check Schedule:                           OK
Total Score: 30/30
```

完整评分日志见 [logs/grade-output.txt](logs/grade-output.txt)。

注入测试后的串口输出（[logs/qemu-injected-output.txt](logs/qemu-injected-output.txt)）可以看到三组用例的实际 PASS 行：

```text
[Test PMM] PASS
[Test Process Create] PASS
[Test Schedule] PASS
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

评分脚本编译注入测试时会出现两条 `-Wincompatible-pointer-types` 警告（`test_lab4_thread` 是 `void (*)(void *)`，`kernel_thread` 形参是 `int (*)(void *)`）。这是课程测试代码在 GCC 10 下的既有警告，不影响构建和运行；同一份代码用 GCC 15 编译时该警告会变成错误。

### 5.3 评分用例覆盖分析

| 检查项（各 10 分） | 注入位置 | 覆盖内容 |
|-------------------|----------|----------|
| `check PMM` | `pmm_init()` 末尾 | `page_insert` 建立映射、同页重复映射、权限覆盖、`page_remove` 幂等与引用计数归零 |
| `check Process Create` | `proc_init()` 末尾 | idleproc 的 pid/state/kstack/parent/mm/tf/runs/context，initproc 的 pid/parent/mm/context |
| `check Schedule` | `init_main()` 入口 | 两次 `kernel_thread` 创建测试线程，8 轮 `need_resched+schedule()` 后两个线程都运行过，且 initproc 能继续执行 |

三项分别对应本组三个模块：PMM 对应 A 的页表操作，Process Create 对应 B 的创建与初始化（`do_fork` 把 initproc 建出来），Schedule 对应 C 的 `do_fork` + `schedule` + `cpu_idle` 链路。`check Schedule` 还要求三行字符串按顺序出现，说明 initproc 在被调度回来后还能正常继续执行并返回。

### 5.4 日志附件

| 附件 | 内容 |
|------|------|
| [环境记录](logs/environment.txt) | 默认 shell 与登录 shell 的工具链、QEMU、GDB 版本对比 |
| [构建日志](logs/build-output.txt) | 注入测试树的完整编译输出 |
| [启动日志](logs/qemu-output.txt) | `make qemu`：固件交接、DTB、内核自检与 initproc 输出 |
| [评分日志](logs/grade-output.txt) | `make grade` 的完整输出（30/30） |
| [注入测试日志](logs/qemu-injected-output.txt) | 三组用例的 PASS 行与 initproc 输出 |

---

## 六、实验总结与收获

### 6.1 与操作系统原理的对应关系

| 实验知识点 | 对应 OS 原理 | 含义、关系与差异 |
|------------|-------------|-----------------|
| `struct proc_struct` | 进程控制块 / TCB | 实验用一块 `kmalloc` 内存描述执行实体；真实内核还有调度实体、凭证、文件表等字段，并需锁保护 |
| `PROC_UNINIT/RUNNABLE/SLEEPING/ZOMBIE` | 进程状态机 | 实验只用到 UNINIT/RUNNABLE/ZOMBIE 的少量迁移；完整内核还有阻塞、唤醒、挂起等 |
| `context` 仅保存 ra/sp/s0~s11 | 上下文切换 | 利用调用约定只保存 callee-saved 寄存器；真实内核还要处理 FPU、向量、地址空间标识等 |
| `switch_to` + `proc_run` | 调度与分派 | 实验是单核、无锁、FIFO；真实调度器有时间片、优先级、CFS 等策略 |
| `proc_list` 线性扫描 | 就绪队列 | 实验用链表轮转；真实内核用 per-CPU 运行队列与红黑树 |
| `parent/cptr/optr/yptr` 字段 | 进程树 | 实验只建立 parent，未实现 wait/回收；完整内核靠父子关系完成退出状态回收 |
| `do_fork` + `copy_thread` | fork/clone 语义 | 实验只创建内核线程（CLONE_VM、mm=NULL）；用户进程还需复制页表、文件表、信号处理等 |
| idleproc | idle 进程 | 实验的 idle 是“复用启动现场”的特殊 PCB，不挂就绪队列；Linux 的 idle 是每 CPU 的静态任务 |
| `local_intr_save/restore` | 临界区 | 实验用单核关中断保护调度；多核还需要自旋锁与内存屏障 |
| 内核线程共享 `boot_pgdir_va` | 地址空间隔离 | 实验线程共享内核页表，没有用户地址空间；用户进程需要独立 `mm` 与页表 |

### 6.2 本实验尚未涉及的重要内容

1. 用户态与系统调用：本实验的内核线程始终在 S 模式运行，没有 `ecall` 与用户程序。
2. `copy_mm` 的真正实现：用户地址空间的页表复制、VMA 复制与 COW。
3. 进程退出与回收：`do_exit`/`do_wait` 仍是骨架，`init_main` 返回后直接 panic。
4. 睡眠/唤醒与同步原语：没有信号量、等待队列，`PROC_SLEEPING` 未被使用。
5. 时间片与优先级：调度器只按 FIFO 轮转，时钟中断不触发抢占。
6. 多核与并发：单 HART，`local_intr_save` 只能屏蔽本核。
7. 文件描述符、信号、进程组等进程资源管理。

### 6.3 提示词、bug 与解决（AI 协作经验）

1. **同一份代码在不同编译器下结论不同。** 本机登录 shell 的 `PATH` 优先使用 GCC 15.1，`make clean && make` 会在 `kern/mm/kmalloc.c` 报 `passing argument 1 of 'kva2page' makes pointer from integer without a cast`：GCC 15 默认把 `int` 到指针的隐式转换视为错误，GCC 10 只是警告。解决：在 `__slob_free_pages()` 里补 `(void *)` 显式转换，两个编译器都能通过；评分则以课程 GCC 10.2 为准。
2. **环境错会被误判成代码错。** 第一次用官方脚本评分是 0/30，但编译成功，输出里三组用例都是 `missing ... PASS` 而不是断言失败（更早一次构建失败时还出现过 `grep: obj/kernel.sym: No such file`，那是 GCC 15 导致 kernel 没生成）。继续查 QEMU 串口日志才发现固件 `Domain0 Next Address=0x0`：本机 OpenSBI 是 fw_dynamic，`-device loader` 不设置跳转地址，内核从未执行。解决：按 lab2/lab3 的做法改用 `-kernel bin/ucore.img`，并保留注释说明原因。这个教训和 lab2 报告里的第 3 条一致：要区分“代码错”和“环境错”。
3. **测试代码本身也会被新编译器拒绝。** 课程 `tools/test_lab4.c` 把 `void (*)(void *)` 传给 `int (*)(void *)` 形参；GCC 15 视为错误，GCC 10 仅警告。解决：不要改课程测试，改用课程工具链评分；同时说明“同一仓库在两种环境下结论不同”是环境差异，不是实现问题。
4. **`do_fork` 的失败路径容易漏资源。** 最初只想“分配失败就返回”，但 `setup_kstack` 成功后若 `copy_mm` 失败，必须 `put_kstack` 再 `kfree(proc)`；登记动作（pid/hash/list）必须放在所有可能失败的操作之后，否则 `find_proc` 可能看到半初始化 PCB。最终用两个清理标签把顺序固定下来。
5. **调度器最容易写成“固定优先级”。** 若每次都从 `proc_list` 表头扫描，永远选中同一个可运行线程，测试里的两个线程无法都跑到；按手册改成从 `current->list_link` 开始并在整条链表上回绕，才是轮转。另外 `runs` 只在 `proc_run` 里加一次，避免与 `schedule` 重复计数。
6. **提示词要写“可检查的约束”，不要写“实现调度器”。** 把“idleproc 不在链表里”“遇到哨兵要跳过”“next==current 不切换”“在关中断区间内”这些条件写进 spec，生成的代码才和框架的状态约定一致；否则代码可能编译通过但评分用例一条都不过。

### 6.4 小组协作说明

- 王鹏钧负责页表操作，是进程创建的前提：`do_fork` 的内核栈分配与后续用户地址空间都依赖 `page_insert`/`page_remove` 的正确性。
- 于子朔负责 PCB 与首次运行现场，决定了“新线程第一次从哪里开始执行”；`copy_thread` 与 `forkret` 的配合是本实验最关键的链路。
- 张雅程负责 `do_fork`/`copy_mm` 与调度器，并完成环境适配与 WSL 验证；调度器把“静止的 PCB”变成“真正轮流使用 CPU 的线程”。
- 三个模块通过 `proc_list`、`context`、`trapframe` 三个接口衔接：A 提供地址映射，B 提供可运行实体，C 提供选择与切换。

---

## 七、报告要求对照

| 手册要求 | 对应章节 |
|----------|----------|
| 说明整章逻辑线与实现顺序 | 3.1、3.2 |
| 说明每个功能的核心函数/模块理解 | 4.1、4.2、4.3 |
| 说明每个练习的 spec 与 prompt 写法 | 4.1/4.2/4.3 的“提示词”小节与 [prompt.md](prompt.md) |
| 说明 AI 生成过程中出现的 bug 及解决 | 4.1/4.2/4.3 的“实现时确认的边界”、6.3 |
| 列出重要知识点与 OS 原理的对应 | 6.1 |
| 列出 OS 原理中重要但本实验未覆盖的知识点 | 6.2 |

---

## 附录：报告相关文件

| 文件 | 说明 |
|------|------|
| [prompt.md](prompt.md) | 三个模块的提示词原文（四个任务） |
| [logs/](logs/) | 环境记录、构建日志、启动日志、评分日志、注入测试日志 |
| `../code/kern/mm/pmm.c` | `page_insert()`/`page_remove()` |
| `../code/kern/process/proc.c` | `alloc_proc`/`proc_init`/`kernel_thread`/`copy_thread`/`copy_mm`/`do_fork`/`cpu_idle` |
| `../code/kern/schedule/sched.c` | `wakeup_proc()`/`schedule()` |
