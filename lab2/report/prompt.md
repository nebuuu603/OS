
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