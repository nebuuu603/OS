# Lab4 提示词记录

本文件按功能模块记录生成代码时实际使用的提示词。写法沿用前几次实验的经验：先给框架上下文（有哪些结构体、接口、约定），再给可检查的规格（状态、顺序、边界、失败路径），最后给约束（不能改哪些文件、保留哪些注释）。生成结果都经过人工对照手册逐条核对，并用官方 `make grade` 验证。

---

## 一、功能模块一：虚拟内存页操作（2411843 王鹏钧）

````text
[PROMPT] 补全 kern/mm/pmm.c 中 page_insert() 与 page_remove()，实现 Sv39 页表上的映射建立与撤销。

[RELY]
接口：get_pte(pgdir, la, create)、pte_create(ppn, type)、pte2page()、page2ppn()、
page_ref()/page_ref_inc()/page_ref_dec()、free_page()、tlb_invalidate()、flush_tlb()、alloc_page()。
数据：struct Page、pages、boot_pgdir_va、PTE_V/PTE_U/PTE_W/PTE_R。
语义：pte_create() = (ppn << PTE_PPN_SHIFT) | PTE_V | type，即 PTE_V 自动置位；
      get_pte(..., 1) 可能为中间级页表分配页，失败返回 NULL。

[TASK]
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
1. 取叶子页表项：ptep = get_pte(pgdir, la, 1)；为 NULL 返回 -E_NO_MEM，且不修改任何页表项。
2. page_ref_inc(page)。
3. 若 *ptep & PTE_V：
   a. old = pte2page(*ptep)；
   b. old == page：page_ref_dec(page) 撤销本次 inc，只把 *ptep 更新为 pte_create(page2ppn(page), perm)，
      tlb_invalidate(pgdir, la)，返回 0；
   c. old != page：先 page_remove(pgdir, la) 撤销旧映射（它负责旧页计数与 TLB）。
4. 写 *ptep = pte_create(page2ppn(page), perm)；tlb_invalidate(pgdir, la)；返回 0。

void page_remove(pde_t *pgdir, uintptr_t la)
1. ptep = get_pte(pgdir, la, 0)；NULL 或 !(*ptep & PTE_V) 直接 return（幂等，不 panic、不分配）。
2. page = pte2page(*ptep)；page_ref_dec(page)；若 page_ref(page) == 0 则 free_page(page)。
3. *ptep = 0；tlb_invalidate(pgdir, la)。

[SPECIFICATION]
- 同一物理页重复映射到同一地址时不得多计引用；
- 同一地址换页时旧页引用必须减到 0 并可释放；
- page_insert 失败返回 -E_NO_MEM，且地址原有映射保持不变；
- 所有页表修改后都必须失效对应 TLB，不能只改内存不刷 TLB。

[CONSTRAINTS]
- 只改 page_insert/page_remove 的函数体与必要的局部变量；
- 不改 get_pte()、check_pgdir()、check_boot_pgdir()、check_alloc_page() 和结构体定义；
- 不使用 libc；保持 `//lab2 function1 2411843` 注释与函数签名。

[OUTPUT]
给出两个函数的完整实现，并说明“旧页 == 新页”分支为什么必须单独处理。
````

---

## 二、功能模块二（上）：PCB 与首次运行现场（2412854 于子朔）

````text
[PROMPT] 实现 kern/process/proc.c 中 alloc_proc()、proc_init()、kernel_thread()、copy_thread()，
        为 uCore 建立 idleproc 与 initproc。

[CONTEXT]
struct proc_struct 字段：state、pid、runs、kstack、need_resched、parent、mm、context、tf、pgdir、
flags、name、list_link、hash_link；context 只有 ra、sp、s0~s11。
kernel_thread() 最终调用 do_fork()（由另一位成员实现），copy_thread() 负责首次运行现场。
已有链路：switch_to 恢复 context 后 ret 到 forkret；forkret 调 forkrets(current->tf)；
forkrets 令 sp = tf 后跳 __trapret；__trapret 恢复 trapframe 并 sret 到 epc；
kernel_thread_entry 执行 move a0, s1; jalr s0，函数返回后 jal do_exit。
kernel_thread() 的调用者约定函数签名为 int (*fn)(void *)。

[TASK]
static struct proc_struct *alloc_proc(void)
- kmalloc(sizeof(struct proc_struct))；NULL 直接返回；
- memset 清零；state = PROC_UNINIT；pid = -1；
- pgdir = (uintptr_t)boot_pgdir_va（proc_run 会用 PADDR 转物理地址）；
- list_init(&proc->list_link)、list_init(&proc->hash_link)。

void proc_init(void)
- list_init(proc_list)，初始化 HASH_LIST_SIZE 条 hash_list；
- idleproc = alloc_proc()；失败 panic；
- idleproc：pid = 0，state = PROC_RUNNABLE，kstack = (uintptr_t)bootstack，
  need_resched = 1，set_proc_name("idle")；current = idleproc；nr_process++；
- pid = kernel_thread(init_main, "Hello world!!", 0)；pid <= 0 时 panic；
- initproc = find_proc(pid)；assert(initproc != NULL && initproc->pid == 1)；set_proc_name("init")。

int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
- struct trapframe tf 清零；
- tf.gpr.s0 = (uintptr_t)fn；tf.gpr.s1 = (uintptr_t)arg；
- tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) & ~SSTATUS_SIE；
- tf.epc = (uintptr_t)kernel_thread_entry；
- return do_fork(clone_flags | CLONE_VM, 0, &tf)。

static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trapframe *tf)
- proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE) - 1；*proc->tf = *tf；
- proc->tf->gpr.a0 = 0；proc->tf->gpr.sp = (esp != 0) ? esp : (uintptr_t)proc->tf；
- proc->context.ra = (uintptr_t)forkret；proc->context.sp = (uintptr_t)proc->tf。

[CONSTRAINTS]
- idleproc 直接复用启动现场，不挂 proc_list/hash_list，不分配内核栈；
- tf.status 必须置 SPP/SPIE 并清 SIE；trapframe 必须正好落在内核栈顶；
- do_fork 的调用只传 clone_flags|CLONE_VM、stack=0、&tf；
- 保留 `//LAB4 function2 2412854` 注释，把函数放在该标记之后；
- 不修改 do_fork、copy_mm、schedule 等其他人负责的函数。

[OUTPUT]
四个函数的完整实现，并说明 SPP/SPIE/SIE 三个位各自的作用。
````

---

## 三、功能模块二（下）：do_fork 与 copy_mm（2412855 张雅程）

````text
[PROMPT] 实现 kern/process/proc.c 中 do_fork() 与 copy_mm()：把 PCB、内核栈、地址空间和首次运行现场
        组合成一个可调度实体，并保证失败路径不泄漏资源。

[CONTEXT]
可复用：alloc_proc()、setup_kstack()、put_kstack()、copy_thread()、get_pid()、hash_proc()、
list_add()、wakeup_proc()、kfree()、nr_process、proc_list、hash_list、MAX_PROCESS；
CLONE_VM 定义在 kern/mm/pmm.h；错误码 E_NO_MEM/E_NO_FREE_PROC 在 libs/error.h；
local_intr_save/restore 在 kern/sync/sync.h。
本实验只创建内核线程：current->mm == NULL，pgdir 由 alloc_proc 指向 boot_pgdir_va。

[TASK]
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
1. ret = -E_NO_FREE_PROC；nr_process >= MAX_PROCESS 时直接返回；
2. ret = -E_NO_MEM；proc = alloc_proc()，NULL 返回；
3. proc->parent = current；
4. setup_kstack(proc) != 0 → 释放 PCB 后返回；
5. copy_mm(clone_flags, proc) != 0 → put_kstack 后释放 PCB；
6. copy_thread(proc, stack, tf)；
7. 关中断临界区：proc->pid = get_pid()；hash_proc(proc)；list_add(&proc_list, &proc->list_link)；
   nr_process++；恢复中断；
8. wakeup_proc(proc)；ret = proc->pid；
9. fork_out 返回 ret；清理标签按“先 kstack 后 PCB”的顺序释放。

static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
- 取 mm = current->mm；
- mm == NULL（内核线程）：proc->mm = NULL，返回 0；
- 带 CLONE_VM：共享 mm，返回 0；
- 其余情况属于本实验未实现的用户地址空间复制，返回错误，不做半成品复制。

[SPECIFICATION]
- 成功返回新 pid（非 0）；失败返回负错误码；
- pid、hash 链表、proc_list、nr_process 必须在同一关中断区间内一次性登记；
- 任何失败路径都不得留下已分配的内核栈或已挂链表的 PCB；
- 不重复增加 runs（proc_run 已负责）；
- copy_thread 的 stack 参数来自 kernel_thread 的 0，此时 sp 使用 trapframe 地址。

[CONSTRAINTS]
- 只改 do_fork/copy_mm，保留 `//LAB4 function3 2412855` 注释（YOUR CODE 处换成学号）；
- 不修改 alloc_proc/proc_init/kernel_thread/copy_thread（其他成员负责）；
- 不改 proc.h、pmm.h、sync.h；不加锁、不使用 libc。

[OUTPUT]
两个函数的完整实现，并逐条说明失败路径释放了哪些资源、为什么登记动作要放在最后。
````

---

## 四、功能模块三：进程调度（2412855 张雅程）

````text
[PROMPT] 实现 kern/process/proc.c 的 cpu_idle() 与 kern/schedule/sched.c 的 schedule()，
        完成 FIFO/轮转调度与 idleproc 保底。

[CONTEXT]
proc_list：全部可调度 PCB 的线性表（含 initproc 与后续 kernel_thread 创建的线程）；
idleproc：pid 0，复用启动现场，不在 proc_list 与 hash_list 中；
proc_run(proc)：已实现，切 current、proc->runs++、写 satp、flush_tlb、switch_to；
对 proc == NULL || proc == current 提前 return；
wakeup_proc(proc)：断言目标不是 ZOMBIE/RUNNABLE，然后把状态置 PROC_RUNNABLE；
local_intr_save/restore：保存并清 SIE、按原值恢复；
测试用例：init_main 入口创建两个 kernel_thread，反复置 current->need_resched=1 并调用 schedule()，
要求两个测试线程都运行并各自让出 CPU，最后 initproc 还能继续执行。

[TASK]
void cpu_idle(void)
- while (1) { if (current->need_resched) schedule(); }

void schedule(void)
1. local_intr_save(intr_flag)；current->need_resched = 0；
2. last = (current == idleproc) ? &proc_list : &(current->list_link)；
3. le = last；do { le = list_next(le)；若 le != &proc_list：
        next = le2proc(le, list_link)；若 next->state == PROC_RUNNABLE 则 break； }
   while (le != last)；
4. next == NULL 或 next->state != PROC_RUNNABLE 时 next = idleproc；
5. next != current 时 proc_run(next)；
6. local_intr_restore(intr_flag)。

[SPECIFICATION]
- 当前是 idleproc 时从表头扫描；否则从 current 的下一个开始，保证轮转；
- 扫描必须跳过 &proc_list 哨兵，并在走完一圈后终止；
- 找不到 RUNNABLE 时退回 idleproc，不能选到 SLEEPING/ZOMBIE/UNINIT；
- 选择与切换过程必须关中断；next == current 时不做切换；
- 不得在 schedule 中增加 runs。

[CONSTRAINTS]
- 只改 cpu_idle() 与 schedule()，保留 `//LAB4 function3 2412855` 注释；
- 不修改 proc_run、switch_to、wakeup_proc、proc.h、sched.h；
- 不使用动态内存和 libc；保持函数签名。

[OUTPUT]
两个函数的完整实现，并说明“从 current 之后开始扫描”和“退回 idleproc”分别解决什么问题。
````
