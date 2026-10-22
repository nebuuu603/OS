
### 功能模块：[物理内存初始化功能]


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

### 实现Best-fit

````text
【背景】
riscv64-ucore lab2，参考实现是 kern/mm/default_pmm.c（First-Fit）。框架在
kern/mm/pmm.h 给出 struct pmm_manager 的六个回调，kern/mm/memlayout.h 给出
struct Page（ref/flags/property/page_link）与 free_area_t。

【任务】
只修改 kern/mm/best_fit_pmm.c 中标有 lab2 / YOUR CODE 的位置，实现完整的
Best-Fit 物理内存管理模块，并定义 best_fit_pmm_manager 完成注册；随后在
kern/mm/pmm.c 的 init_pmm_manager() 中切换到它。

【约束】
1. 文件里已有的 basic_check() 与 best_fit_check() 不得修改，实现必须符合它们
   使用的状态约定（块头 PG_property、property 记录块长度、空闲链表按地址升序）；
2. 分配：在空闲链表中选 property >= n 且最小的块，找不到返回 NULL；
3. 剩余空间要切分成新块头插回原位置，nr_free 正确更新；
4. 释放：断言页未被保留且不是块头，清标志与引用计数，按地址插链，并与前、后
   相邻空闲块合并；
5. 不得改动 default_pmm.c，不得使用 libc，必须通过 -Wall -Werror -std=gnu99。

【输出】
best_fit_pmm.c 的完整实现，以及一句说明：为什么 Best-Fit 需要遍历整条链表。
````

### 生成 Buddy System 模块并接入

````text
【背景】
riscv64-ucore lab2，物理内存管理接口是 kern/mm/pmm.h 的 struct pmm_manager
（init / init_memmap / alloc_pages / free_pages / nr_free_pages / check）。
可用资源：struct Page（ref/flags/property/page_link）、PG_property 相关宏、
libs/list.h、page2pa/pa2page/page2ppn、npage/nbase/pages/va_pa_offset、
assert/panic/cprintf、set_page_ref。参考实现是 default_pmm.c。

【任务】
新建 kern/mm/buddy_pmm.c 与 buddy_pmm.h，从零实现伙伴系统，并定义
const struct pmm_manager buddy_pmm_manager 完成注册。不要修改 struct Page、
不要改动 default_pmm.c / best_fit_pmm.c，也不要把 init_pmm_manager() 的默认值改掉。

【要求】
1. 最大块 2^15 页，每个 order 一条按物理地址升序的空闲链表；
2. init_memmap 要能处理起点不是 2 的幂对齐的区域，只切出对齐且不越界的块；
3. alloc_pages(n) 把 n 向上取整到 2 的幂，从最小够用块分裂，支持任意 n；
4. free_pages(base, n) 按同样规则还原 order，与 start^(1<<order) 的伙伴块逐级合并；
5. 维护每级页数与全局页数，保证任何时刻 sum(各级) == 全局；
6. 记录 init_memmap 登记过的区域，合并只允许发生在同一区域内部；
7. check() 校验块头标志、order、2^order 对齐、各级页数与总和；
8. 只用内核头文件，通过 -Wall -Werror -std=gnu99。

【输出】
两个文件的完整内容，并说明：为什么 init_memmap 要按“对齐且不越界”切块，
以及合并时全局计数器应当如何更新。
````

### 为 Buddy 生成测试

````text
【背景】
buddy_pmm.c 已实现，但它不能设为默认管理器（官方评分要求 Best-Fit 处于激活
状态），测试也不能破坏当前 pmm_manager 的状态。

【任务】
写 void buddy_self_test(void)（声明放 buddy_pmm.h），用断言完成自测。

【约束】
1. 先向当前 pmm_manager 借 2048 页，取其中 1024 页对齐的一段作为测试区域，
   保证“全部空闲时可以合并成一个 order-10 块”；
2. 直接调用 buddy_pmm_manager 的回调，不切换全局 pmm_manager；
3. 结束时把整段 2048 页的 flags/property/ref 清零后还给当前 pmm_manager；
4. 每个阶段调用 buddy_check()，成功打印一行 PASS。

【必须覆盖】
初始化后页数正确；分配 1 页、分配 3 页（按 4 页记账）；两者不重叠；
剩余不足时整段分配失败；释放后能否合并回单个 order-10 块（用“再分配 1024 页
必须成功且返回区域首地址”反向验证）；耗尽时分配失败；每阶段 check() 通过。

【输出】
完整函数，并说明每个用例能发现哪类实现错误。
````

### 生成 SLUB 对象分配模块

````text
【背景】
riscv64-ucore lab2。页层已经写好：kern/mm/slub_page.h 提供

struct slub_page { struct Page *base; void *addr; size_t nr_pages; list_entry_t link; };
struct slub_page *slub_page_alloc(struct slub_page *page, size_t nr_pages);
int slub_page_free(struct slub_page *page);

它内部调用 alloc_pages/free_pages，addr 是页的虚拟地址。现在要在它之上实现
任意大小对象的分配层，不能修改 slub_page.c。

【任务】
新建 kern/mm/slub.c 与 slub.h，提供：
void *slub_alloc(size_t size);
int   slub_free(void *ptr);       /* 成功 0，非法指针 E_INVAL */
size_t slub_nr_free_objects(void);
size_t slub_nr_slabs(void);

【要求】
1. 大小分级 8..2048（9 级），请求向上取整；0 与超限返回 NULL；
2. 每个 slab 一页：页首元数据，其后等长对象；空闲对象用自身前 8 字节串链；
3. 每级维护 slabs（全部）与 partial（有空对象）两条链表；
4. 分配优先用 partial，没有空闲对象再向页层申请新 slab；
5. 释放时用 magic + cache 归属 + 地址区间 + 对象对齐校验指针；
6. slab 全空且该级 slab 数 > 1 时归还页层，否则挂回 partial 复用；
7. 只用内核头文件，通过 -Wall -Werror -std=gnu99。

【输出】
两个文件的完整内容，并说明：为什么对象指针可以按页对齐反推 slab 元数据，
以及 freelist 嵌在对象内部有什么代价。
````

### 为 SLUB 生成测试

````text
【背景】
kern/mm/slub.c 已实现，pmm_init() 已完成、页层可用；测试在内核里跑，失败即 panic。

【任务】
写 void slub_self_test(void)，用断言验证对象层，最后打印一行 PASS。

【必须覆盖】
1. 边界：slub_alloc(0) 与 slub_alloc(2049) 返回 NULL；
2. 分级：1/24/200 字节分别落在 8/32/256 字节级，地址 8 字节对齐；
3. 独立性：三个对象互不重叠，写入不同图案后逐字节读回；
4. 跨 slab：连续分配 128 个 128 字节对象，全部非空、两两不同、可写回读；
5. 复用：全部释放后再分配一次必须成功；
6. 最大级：slub_alloc(2048) 成功并可写满；
7. 非法指针：slub_free(NULL) 返回 E_INVAL。

【输出】
完整函数，并说明每个用例能发现哪类错误（分级错、freelist 断链、slab 复用未清
元数据、越界写等）。
````