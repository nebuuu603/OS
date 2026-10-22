# 操作系统实验报告

## 实验基本信息

| 项目 | 内容 |
|------|------|
| **实验名称** | Lab 2: 物理内存和页表 |
| **小组成员** | 2411843-王鹏钧、2412854-于子朔、2412855-张雅程 |
| **完成日期** | YYYY-MM-DD |

### 小组分工

练习如何分工？

| 成员 | 负责的练习/模块 |
|------|----------------|
| 2411843-王鹏钧 | 功能模块一：物理内存初始化功能|
| 学号2-姓名2 | [练习或模块名称] |
| 学号3-姓名3 | [练习或模块名称] |

实验报告如何分工？

---

## 一、实验目的

<!-- 说明本次实验的主要目标和预期成果 -->

本实验的主要目的是：

1. [目的1：建立物理页描述符并将可用页面交给物理内存管理器]
2. [目的2：实现并接入 Best-Fit 分配器]
3. [目的3：通过 Buddy System 和 SLUB 扩展任务进一步理解页级分配与任意大小内存对象分配]

---

## 二、实验环境

你们使用的 AI 工具

| 成员 | AI 编程工具 | 底层模型 | 备注 |
|------|------------|---------|------|
| 2411843-王鹏钧 | Codex 桌面应用 | GPT-6.1 |  |
| 2412854-于子朔 | Codex 桌面应用 | GPT-6.1 |  |
| 2412855-张雅程 | Codex 桌面应用 | GPT-6 Astra |  |

**说明：**
- **AI 编程工具**：指具体使用的终端工具、编辑器插件、桌面应用或浏览器界面
- **底层模型**：指该工具使用的大语言模型及版本

---

## 三、实验整体逻辑分析

### 2.1 本章节的逻辑主线

[请用自己的话描述本章实验的核心主线，例如：
- 本章围绕哪个核心主题展开？（例如：用户进程管理、虚拟内存、文件系统等）
- 整个实验解决什么问题？（例如：如何让用户程序运行在操作系统之上）]

### 2.2 功能的逐步实现

[说明本章实验是按照什么顺序逐步实现各个功能的，以及为什么要按这个顺序，例如：

1. 首先实现**功能A** → 为什么首先实现这个？（例如：建立系统调用接口是后续所有用户态功能的基础）
2. 接着再完善**功能B** → 为什么接着实现这个？（例如：有了系统调用后，需要能够加载和启动第一个用户进程）
3. 然后完成**功能C** → 为什么然后实现这个？（例如：有了第一个进程后，需要实现进程复制功能）
4. 最后补充了**功能D** → 为什么最后实现这个？（例如：最后完善进程退出和清理机制，形成完整闭环）]

---

## 四、实验内容与实现

<!-- 
说明：本部分按照 exercises 文档中的练习顺序组织
每个功能模块或者练习中可能包含多项任务，请根据实际情况调整
-->

### 功能模块：[物理内存初始化功能]

**负责人：** [2411843-王鹏钧]

#### 模块功能描述

**需要实现/修改的函数：**

```c
static void page_init(void); (kern/mm/pmm.c)
```

**功能说明：**


- 作为内核物理内存管理的总入口和抽象层：在 kern_init 启动早期调用 pmm_init()，探测物理内存、建立 struct Page 描述符数组和空闲页链表，是整个内存管理子系统（后续分页、缺页、用户进程）的基础
- 主要处理内存探测：从设备树（DTB）获取物理内存基址/大小，计算 npage（内存末端页号）。统一分配/释放入口：为上层屏蔽首次适配、最佳适配、伙伴系统等不同算法的差异；check_alloc_page 做自检
- init_pmm_manager() 选定某个算法实例（如 best_fit_pmm_manager），并调用它的 init() 初始化内部链表/位图

#### 最终提示词

以下是经过迭代优化后，最终成功实现该功能的提示词：

````markdown
[PROMPT] 补全 pmm.c中//lab2 function1 YOUR CODE 标记处的 page_init 函数

[RELY]

宏/常量：PHYSICAL_MEMORY_OFFSET、PGSIZE、PPN()、ROUNDUP()、PADDR()（依赖 va_pa_offset 先赋值）
全局变量：pages、npage、nbase（= DRAM_BASE/PGSIZE）、va_pa_offset
数据结构：struct Page、PG_reserved、SetPageReserved()、pa2page()/page2pa()
已有函数：init_memmap(base, n)、get_memory_base()/get_memory_size()
[GUARANTEE] 

完整的 static void page_init(void) 函数
必须区分虚拟地址、物理地址、物理页号和Page描述符地址，正确处理可管理上界和页面对齐，确保内核及描述符数组不会进入空闲区；
设备树信息无效时应终止初始化，没有可用完整页面时不得调用init_memmap()。
实现过程中可以使用get_memory_base()、get_memory_size()、panic()、cprintf()、init_memmap()、pa2page()、链接符号end、struct Page及相关全局变量，并使用框架提供的内存上界、地址转换、页面对齐和页面状态宏完成初始化
[SPECIFICATION]

va_pa_offset = PHYSICAL_MEMORY_OFFSET 必须最先设置
npage = PPN(mem_base + mem_size)，且 npage > nbase
pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE)，必须页对齐且 ≥ end
先将 pages[0..npage-nbase) 全部 SetPageReserved 并清 flags/property/ref
空闲区 = PADDR(pages) + ROUNDUP((npage-nbase)*sizeof(struct Page), PGSIZE) 到 mem_base+mem_size，调用 init_memmap(pa2page(free_base), free_nr)，须满足 0 < free_nr <= npage - nbase
结果需通过 Test 1 的 8 项检查：含 end 的页、含 pages 数组的页保持 reserved，page2pa/pa2page 往返一致]
````

**硬件的可用物理内存范围获取方法** 

OS 可以在启动时通过固件或引导程序提供的硬件信息获取可用物理内存范围。读取设备树的 memory 节点，获得内存基址和大小；也可以由 BIOS/UEFI 提供内存映射，或通过引导协议把内存区域列表传给内核。另外如果固件没有提供内存映射，但OS知道当前运行的硬件架构，也可以直接查询内存控制器


### 练习：[练习标题]

**负责人：** [学号-姓名]

[按照练习的具体要求进行解答]

---

### Challenge：[Challenge标题]

**负责人：** [学号-姓名]

[按照Challenge的具体要求进行解答]

---

### 功能模块：[Buddy System 连续物理内存分配]

**负责人：** [2412855-张雅程]

#### 模块功能描述

**需要实现/修改的文件：**

```c
kern/mm/buddy_pmm.c    // Buddy System 分配器实现
kern/mm/buddy_pmm.h    // 对外接口与自测声明
kern/mm/lab2_c_test.c  // 自测入口（仅在 -DLAB2_C_TEST 时被 pmm_init 调用）
```

**功能说明：**

- 按 2 的幂把空闲物理内存划分成“块”，每个 order（1, 2, 4, 8, … 32768 页）维护一条按物理地址升序的空闲链表，作为 `pmm_manager` 的又一个实例 `buddy_pmm_manager` 接入内核。
- 分配时把请求页数向上取整到 2 的幂（`alloc_pages(3)` 实际占用 4 页），从最小的够用块中逐级分裂；释放时按同样规则还原 order，并与地址异或得到伙伴块，逐级合并。
- 实现 `init / init_memmap / alloc_pages / free_pages / nr_free_pages / check` 六个回调，可像 First-Fit、Best-Fit 一样被 `init_pmm_manager()` 选中。

#### 设计思路

**数据结构。** 不修改框架的 `struct Page`，复用它的三个字段：块首页用 `property` 记录块的 order、用 `PG_property` 标记“这是空闲块头”、用 `page_link` 挂到对应 order 的链表上；非块头的页 `flags` 与 `property` 都为 0。

```c
struct buddy_free_area {
    list_entry_t free_list;   /* 该 order 的空闲块头 */
    size_t nr_free;           /* 该 order 的空闲页数 */
};
static struct buddy_free_area buddy_free_areas[BUDDY_MAX_ORDER + 1];
static size_t buddy_nr_free;      /* 全局空闲页数 */
```

**初始化。** 把 `[base, base + n)` 贪心地切成“绝对页号对齐、且剩余空间足够”的最大 2 幂块，逐块插入。这样即使区域起点不是 2 的幂对齐（例如内核之后的空闲区从 `0x80346` 开始），也不会切出跨越区域边界的伙伴块。

**分配。** 计算 `order = ceil(log2(n))`，从该 order 往上找到第一条非空链表，摘下块头，再从高到低逐级分裂，把多出来的后半块插回低一级链表；返回的块头清掉 `PG_property`。

**释放与合并。** 把 `n` 还原成 order 后，令 `buddy_ppn = start ^ (1 << order)`：若伙伴块也在空闲链表中、order 相同，就把它摘下、合并、order 加一，继续向上尝试。合并后的块头永远取地址较小者，保证链表仍按物理地址升序。

**两个容易出错的地方。**
1. **记账**：合并时既要把伙伴块从对应 order 的链表和该级计数中删掉，也要从全局 `buddy_nr_free` 中减去；最后插入合并块时再加回。只在最后加、不在中途减，`nr_free` 会在一次“分配-释放”后翻倍——自测里的 `assert(total == buddy_nr_free)` 正是抓住了这个 bug。
2. **区域保护**：合并前检查“合并后的整块”是否仍落在 `init_memmap` 登记过的某段区域内。否则，当 Buddy 只管理内存的一部分时（本实验的自测正是这种情况），可能把相邻的、属于 Best-Fit 的空闲页误当成伙伴块合并进来。

#### 最终提示词

**Prompt 1：生成 Buddy System 分配模块并接入内核**

````markdown
【背景】
riscv64-ucore lab2，内核在 QEMU virt（128MiB DRAM，4KiB 页）上运行。
物理内存管理框架在 kern/mm/pmm.h，接口是：

struct pmm_manager {
    const char *name;
    void (*init)(void);
    void (*init_memmap)(struct Page *base, size_t n);
    struct Page *(*alloc_pages)(size_t n);
    void (*free_pages)(struct Page *base, size_t n);
    size_t (*nr_free_pages)(void);
    void (*check)(void);
};

可用资源：struct Page（memlayout.h，字段只有 ref / flags / property / page_link，
另有 PG_property、SetPageProperty/ClearPageProperty/PageProperty 宏）、
libs/list.h 的双向链表、page2pa/pa2page/page2ppn、npage/nbase/pages/va_pa_offset、
assert/panic/cprintf、set_page_ref。参考实现是 kern/mm/default_pmm.c（First-Fit）。

【任务】
新建 kern/mm/buddy_pmm.c 与 kern/mm/buddy_pmm.h，从零实现伙伴系统，
并定义 const struct pmm_manager buddy_pmm_manager 完成注册。不要修改 struct Page，
不要修改 default_pmm.c / best_fit_pmm.c，也不要把 init_pmm_manager() 的默认值改掉。

【要求】
1. 最大块 2^15 = 32768 页；每个 order 一条空闲链表，按物理地址升序；
2. init_memmap(base, n) 要能处理起点不是 2 的幂对齐的区域，只切出对齐且不越界的块；
3. alloc_pages(n) 把 n 向上取整到 2 的幂，从最小够用块分裂，支持任意 n；
4. free_pages(base, n) 按同样规则还原 order，与 ppn^(1<<order) 的伙伴块逐级合并；
5. 维护每个 order 的页数与全局页数，保证任何时候 sum(各级) == 全局；
6. 记录 init_memmap 登记过的区域，合并只允许发生在同一区域内部；
7. check() 校验：链表节点都是块头、property 等于所在 order、块首按 2^order 对齐、
   各级页数等于该级链表页数之和、总和等于全局页数；
8. 只能用内核已有的头文件（无 libc），必须能通过 -Wall -Werror -std=gnu99 编译。

【输出】
两个文件的完整内容，附一段说明：为什么 init_memmap 要按“对齐且不越界”切块，
以及合并时全局计数器应当如何更新。
````

**Prompt 2：为 Buddy 实现生成测试**

````markdown
【背景】
kern/mm/buddy_pmm.c 已实现，但我不能把它设为默认管理器（官方评分要求 Best-Fit
处于激活状态），也不希望测试破坏当前 pmm_manager 的状态。

【任务】
写 void buddy_self_test(void)（声明放在 buddy_pmm.h），用断言完成自测。

【约束】
1. 先向当前 pmm_manager 借一段连续物理页：分配 2048 页，取其中 1024 页对齐的那一段
   作为测试区域，这样既能保证“全部空闲时可以合并成一个 order-10 块”，又不会碰到
   别的管理器的页；
2. 直接调用 buddy_pmm_manager 的 init/init_memmap/alloc_pages/free_pages，不切换
   全局 pmm_manager；
3. 测试结束把整段 2048 页的 flags/property/ref 清零后还给当前 pmm_manager；
4. 每个阶段都调用 buddy_check()；失败用 assert，成功打印一行 PASS。

【必须覆盖】
- 初始化后空闲页数等于区域页数；
- 分配 1 页；分配 3 页（验证按 4 页记账）；两者互不重叠；
- 剩余页不足时整段分配返回 NULL；
- 释放后能否逐级合并回单个 order-10 块（用“再分配 1024 页必须成功且返回区域首地址”
  来反向验证，这比逐个检查链表更有说服力）；
- 耗尽时分配 1 页返回 NULL；
- 每次分配/释放前后 check() 都通过。

【输出】
完整函数，并用一段话解释每个用例能发现哪类实现错误。
````

#### 测试与验证

自测通过新增的 `make ctest` 目标运行（编译时加 `-DLAB2_C_TEST`，`pmm_init()` 末尾调用
`lab2_c_self_test()`；默认构建不定义该宏，因此完全不影响官方评分）：

```
memory management: best_fit_pmm_manager
check_alloc_page() succeeded!
[Test C1] Buddy System: begin
[Test C1]   region @0x80400000, 1024 pages
buddy_check() succeeded!
...
[Test C1] Buddy System: PASS
[Test C2] SLUB: begin
[Test C2] SLUB: PASS
satp virtual address: 0xffffffffc0206000
satp physical address: 0x0000000080206000
```

| 用例 | 覆盖的行为 | 为什么能发现错误 |
|------|-----------|------------------|
| 初始化后 `nr_free == 1024` | 区域切分与记账 | 切块或计数错会立刻不符 |
| 分配 1 页、3 页 | 最小分配、向上取整 | 3 页必须按 4 页记账 |
| 分配后整段分配失败 | 耗尽检测 | 剩 1019 页不可能满足 1024 页 |
| 释放后整段分配成功且返回 `base` | 逐级合并 | 只有完全合并成 order 10 才可能成功 |
| 每阶段 `buddy_check()` | 链表、order、对齐、计数一致性 | 漏合并/漏记账都会触发断言 |

#### 改进空间

1. **时间**：当前分配是“从所需 order 向上逐条链表查找”，最坏 O(MAX_ORDER)；
   页数很多时可以用位图（每个 order 一张 bitmap）把查找降为 O(1) 级别，代价是额外内存。
2. **空间**：每个空闲块靠 `struct Page` 的 `property` 记录 order，需要为全部物理页
   准备描述符数组（128MiB 就要 1.25MiB 描述符）；位图式伙伴系统只需要
   (总页数 / 8) 字节量级的元数据。
3. **碎片**：伙伴系统只按 2 的幂切分，请求 3 页会占 4 页，内部碎片最大接近 50%；
   可在上层再套一层 slab/slub（本实验的 Challenge）来消化小对象。
4. **并发**：当前实现没有加锁，多核（`-smp 2`）下需要每 order 一把自旋锁或 per-CPU 页池。

---

### Challenge：[SLUB 任意大小的内存单元分配算法]

**负责人：** [2412855-张雅程]

#### 功能说明

SLUB 是“两层”分配器：第一层以页为单位向物理内存管理器申请/归还内存（框架已提供
`kern/mm/slub_page.c`，接口是 `slub_page_alloc(&desc, nr_pages)` / `slub_page_free(&desc)`），
第二层把页切成固定大小的小对象，按大小分级缓存，从而支持任意大小（本实现 8–2048 字节）的分配与释放。

#### 设计思路

- **大小分级**：8、16、32、…、2048 共 9 级，请求向上取整（`slub_alloc(1)` 落到 8 字节级，
  `slub_alloc(24)` 落到 32 字节级）；`size == 0` 或 `> 2048` 直接返回 `NULL`。
- **slab 布局**：一个 slab 恰好占一页。页首是 `struct slub_slab`（magic、所属 cache、
  页描述符、两条链表节点、freelist、空闲/总对象数），其后是等长对象区；
  空闲对象用自身的前 8 字节串成单向链表，不额外占内存。
- **分配**：先在 `partial`（还有空对象的 slab）链表取；为空则新建 slab（页层申请一页、
  逆序串链、挂到 `slabs` 与 `partial`）。取走最后一个对象时把该 slab 从 `partial` 摘掉。
  返回前 `memset` 清零请求的字节数。
- **释放**：slab 一定页对齐，所以把指针按页下取整即得 slab 头；用 magic、所属 cache、
  地址区间与对象对齐四重校验拒绝非法指针（返回 `E_INVAL`）。归还对象后，若 slab 从“满”
  变为“有空”则挂回 `partial`；若整个 slab 都空了且该级还有别的 slab，就把页还给页层，
  避免长期占用。

#### 最终提示词

**Prompt 1：生成 SLUB 对象分配模块**

````markdown
【背景】
riscv64-ucore lab2。页层已经写好：kern/mm/slub_page.h 提供

struct slub_page { struct Page *base; void *addr; size_t nr_pages; list_entry_t link; };
struct slub_page *slub_page_alloc(struct slub_page *page, size_t nr_pages);
int slub_page_free(struct slub_page *page);

它内部调用 alloc_pages/free_pages（当前是 Best-Fit），addr 是页的虚拟地址。
现在要在它之上实现任意大小对象的分配层，不能修改 slub_page.c。

【任务】
新建 kern/mm/slub.c 与 kern/mm/slub.h，提供：
void *slub_alloc(size_t size);
int   slub_free(void *ptr);          /* 成功 0，非法指针 E_INVAL */
size_t slub_nr_free_objects(void);
size_t slub_nr_slabs(void);

【要求】
1. 大小分级 8,16,…,2048（9 级），请求向上取整；0 和超限返回 NULL；
2. 每个 slab 一页：页首放元数据，其后是等长对象区；空闲对象用对象自身前 8 字节串链；
3. 每级维护 slabs（全部）与 partial（有空对象）两条链表，记录空闲对象数与 slab 数；
4. 分配优先用 partial，没有再向页层申请新 slab；
5. 释放时用 magic + cache 归属 + 地址区间 + 对象对齐校验指针，拒绝空指针与野指针；
6. slab 全空且该级 slab 数 > 1 时归还给页层，否则挂回 partial 复用；
7. 只能使用内核头文件（defs/list/memlayout/pmm/stdio/string/error），
   必须通过 -Wall -Werror -std=gnu99；不要引入浮点、递归或全局构造。

【输出】
两个文件的完整内容，并说明：为什么对象指针可以按页对齐反推出 slab 头，
以及 freelist 嵌在对象内部的做法有什么代价。
````

**Prompt 2：为 SLUB 生成测试**

````markdown
【背景】
kern/mm/slub.c 已实现。pmm_init() 已完成，页层可用；测试要在内核里跑，失败即 panic。

【任务】
写 void slub_self_test(void)，用断言验证对象层，最后打印一行 PASS。

【必须覆盖】
1. 边界：slub_alloc(0) 与 slub_alloc(2049) 必须返回 NULL；
2. 分级：1 / 24 / 200 字节分别应落在 8 / 32 / 256 字节级，返回地址 8 字节对齐；
3. 独立性：三个对象互不重叠，写入不同图案后逐字节读回一致；
4. 跨 slab：连续分配 128 个 128 字节对象（一页只放得下 31 个），要求全部非空、
   两两不同、地址对齐，并在每个对象里写入编号后读回；
5. 复用：全部释放后再分配一次，必须成功；
6. 最大级：slub_alloc(2048) 成功并可写满 2048 字节；
7. 非法指针：slub_free(NULL) 必须返回 E_INVAL。

【输出】
完整函数，并说明这些用例分别能发现哪类错误（分级算错、freelist 断链、slab 复用
时没清元数据、越界写等）。
````

#### 测试与验证

`make ctest` 输出 `[Test C2] SLUB: PASS`，覆盖情况见下表：

| 用例 | 覆盖的行为 | 为什么能发现错误 |
|------|-----------|------------------|
| `slub_alloc(0)` / `(2049)` | 边界与越界 | 分级函数写错会返回非 NULL |
| 1 / 24 / 200 字节 | 向上取整 | 落错级别会导致对象区大小与对象数不符 |
| 三对象互写互读 | 不重叠 | freelist 串错会让两个对象指向同一地址 |
| 128×128B | 跨 slab 与增长 | 一页 31 个，必然触发多次 `slub_grow` |
| 全部释放后再分配 | 复用与回收 | slab 回收时元数据未清会立刻出错 |
| `slub_free(NULL)` | 入参校验 | 未判空会直接解引用 |

#### 与 Linux SLUB 的差异、改进空间

1. **没有 per-CPU 缓存**：Linux 每 CPU 有自己的 current slab，分配多数情况无锁；
   本实现全部走同一条 `partial` 链表，多核下必须加锁。
2. **没有 slab 着色（coloring）**：Linux 通过偏移对象区起点让不同 slab 的对象落在
   不同 cache line，减少伪共享；本实现所有 slab 布局完全相同。
3. **size class 粒度粗**：只有 9 级，128 字节的请求会占 128 字节（合适），
   但 130 字节的请求要占 256 字节；可以增加级数或按 2 的幂 + 中间档细分。
4. **没有构造函数/析构函数**，也不支持 `krealloc`、`kzalloc` 之外的语义；
   可以在 `struct slub_cache` 里挂 `ctor` 指针来扩展。
5. **回收策略简单**：只在“整个 slab 都空且该级还有别的 slab”时归还页，
   可以改为维护 `cpu_partial` 阈值，批量归还以降锁竞争。

---

#### 附：实现过程中由测试发现的 bug

1. **Buddy 合并后计数翻倍**：最初只在合并结束时把合并块加进全局 `buddy_nr_free`，
   忘记在合并时把伙伴块减掉，导致一次“分配 1 页 + 释放 1 页”后空闲页数从 1024 变成 2047。
   `buddy_check()` 里 `assert(total == buddy_nr_free)` 直接定位到问题，修正为
   “合并时先减、最后插入时再加”。
2. **QEMU 启动参数**：`-device loader,file=...,addr=0x80200000` 在本机 QEMU 7.0 /
   OpenSBI 0.9 下只把镜像放进内存，固件的 `Domain0 Next Address` 仍是 0，内核从未执行，
   `make grade` 恒为 0 分；改为 `-kernel bin/ucore.img` 后正常启动（与 lab1 的修法一致）。
3. **评分脚本的时序**：`grade.sh` 依赖 `ulimit -t` 事后杀掉 QEMU，冷启动时串口日志还没
   flush，检查必然读不到 PASS；改为轮询等待 `[Test` 出现（最多 60s）后再结束 QEMU。

---

## 五、测试与验证

<!-- 根据该实验的具体情况，提供完整的测试运行截图，应包含：
- 编译及运行成功的输出（make qemu），可能有多个
- 测试通过的结果（make grade），只有一个
-->

**测试截图：**

> 下面同时给出实际运行日志，截图见 `./images/test_result.png`（按真实运行输出整理）。

`make grade`（官方评分，注入 `tools/test_lab2.c` 后运行 QEMU）：

```
  -check page init:                          OK
  -check best fit:                           OK

Total Score: 20/20
```

`make ctest`（成员 C 自测，`-DLAB2_C_TEST`）：

```
memory management: best_fit_pmm_manager
check_alloc_page() succeeded!
[Test C1] Buddy System: begin
[Test C1]   region @0x80400000, 1024 pages
buddy_check() succeeded!
buddy_check() succeeded!
buddy_check() succeeded!
buddy_check() succeeded!
buddy_check() succeeded!
buddy_check() succeeded!
[Test C1] Buddy System: PASS
[Test C2] SLUB: begin
[Test C2] SLUB: PASS
satp virtual address: 0xffffffffc0206000
satp physical address: 0x0000000080206000
```

![测试结果截图](./images/test_result.png)

---

## 六、实验总结与收获

### 对操作系统的理解

1. 列出你们认为本实验中重要的知识点，以及与对应的 OS 原理中的知识点，并简要说明二者的含义、关系和差异
2. 列出你们认为 OS 原理中很重要、但在本实验中没有对应上的知识点

### AI 协作开发的经验

[写出你们在与 AI 协作过程中实际收获的经验或者感悟]

---

