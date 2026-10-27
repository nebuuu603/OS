#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <../sync/sync.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &default_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 请补充你在lab2中的代码

static void page_init(void) {
    extern char end[];

    uint64_t mem_base;
    uint64_t mem_size;
    uint64_t mem_end;
    uintptr_t pages_pa;
    uintptr_t free_base;
    size_t page_desc_count;
    size_t free_nr;

    /*
     * 高地址虚拟内核地址与物理地址之间的偏移：
     *
     * VA = PA + PHYSICAL_MEMORY_OFFSET
     */
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    /*
     * 从设备树获取物理内存范围。
     * 当前 pmm 框架中的 nbase 固定以 DRAM_BASE 为起点，
     * 因此要求设备树报告的内存起点为 DRAM_BASE。
     */
    mem_base = get_memory_base();
    mem_size = get_memory_size();

    if (mem_base == 0 || mem_size == 0) {
        panic("invalid physical memory information");
    }

    if (mem_base != DRAM_BASE) {
        panic("unsupported physical memory base");
    }

    mem_end = mem_base + mem_size;

    /* 检查无符号整数加法是否溢出 */
    if (mem_end <= mem_base) {
        panic("physical memory range overflow");
    }

    /*
     * npage 表示物理内存末尾对应的物理页号。
     * 例如：
     *   mem_base = 0x80000000
     *   mem_end  = 0x88000000
     *   npage    = 0x88000
     */
    npage = PPN(mem_end);

    if (npage <= nbase) {
        panic("physical memory is smaller than DRAM base");
    }

    /*
     * end 是高地址虚拟地址。
     * Page 描述符数组放在内核镜像结束位置之后，并按页对齐。
     */
    pages = (struct Page *)ROUNDUP((uintptr_t)end, PGSIZE);

    if ((uintptr_t)pages < KERNBASE) {
        panic("invalid pages virtual address");
    }

    page_desc_count = npage - nbase;

    /*
     * 初始化所有 Page 描述符为 reserved。
     * 后面只有真正可用的物理页会交给 init_memmap()。
     */
    for (size_t i = 0; i < page_desc_count; i++) {
        pages[i].flags = 0;
        pages[i].property = 0;
        pages[i].ref = 0;
        SetPageReserved(&pages[i]);
    }

    /*
     * 将高地址虚拟地址转换为 Page 描述符数组对应的物理地址。
     */
    pages_pa = PADDR((uintptr_t)pages);

    /*
     * Page 描述符数组本身占用的物理空间也必须保留。
     * free_base 是真正可分配物理内存的起始地址。
     */
    free_base = pages_pa +
        ROUNDUP(page_desc_count * sizeof(struct Page), PGSIZE);

    if (free_base < pages_pa || free_base >= mem_end) {
        panic("no usable physical memory after page descriptors");
    }

    /*
     * 只登记完整的物理页。
     */
    free_nr = (mem_end - free_base) / PGSIZE;

    if (free_nr == 0) {
        panic("no free physical pages");
    }

    /*
     * 建立空闲物理页管理区。
     * free_base 已经是物理地址，需要转换成对应的 Page 描述符。
     */
    init_memmap(pa2page(free_base), free_nr);
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
