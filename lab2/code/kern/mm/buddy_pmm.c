#include <assert.h>
#include <buddy_pmm.h>
#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <pmm.h>
#include <stdio.h>

/*
 * Buddy System 分配器（lab2 成员 C）。
 *
 * 每个空闲块的首个 Page 作为该块的代表：
 *   - property 记录块的 order（块大小 = 1 << order 页）
 *   - PG_property 置位表示这是一个空闲块头
 *   - page_link 挂在对应 order 的空闲链表上
 * 非块头的页不参与链表，flags / property 均为 0。
 */

#define BUDDY_MAX_ORDER 15                  /* 最大块 2^15 = 32768 页 */
#define BUDDY_NR_ORDERS (BUDDY_MAX_ORDER + 1)
#define BUDDY_MAX_REGIONS 8                 /* 可登记的内存区域数 */

struct buddy_free_area {
    list_entry_t free_list;                 /* 该 order 的空闲块头链表 */
    size_t nr_free;                         /* 该 order 的空闲页数 */
};

static struct buddy_free_area buddy_free_areas[BUDDY_NR_ORDERS];
static size_t buddy_nr_free;                /* 全部空闲页数 */

/*
 * init_memmap 可能被多次调用，每次登记一段连续物理页。合并只在同一段
 * 区域内进行，避免把不属于 Buddy 的页误当成伙伴块。
 */
struct buddy_region {
    size_t start;                           /* 起始物理页号 */
    size_t end;                             /* 结束物理页号（不含） */
};

static struct buddy_region buddy_regions[BUDDY_MAX_REGIONS];
static size_t buddy_nr_regions;

/* 向上取整到 2 的幂所需的 order；调用者负责检查是否越界。 */
static size_t
buddy_order_ceil(size_t n) {
    size_t order = 0;
    while ((1UL << order) < n) {
        order++;
    }
    return order;
}

static bool
buddy_region_contains(size_t start, size_t end) {
    for (size_t i = 0; i < buddy_nr_regions; i++) {
        if (start >= buddy_regions[i].start && end <= buddy_regions[i].end) {
            return 1;
        }
    }
    return 0;
}

/* 把一个已经初始化好的块头挂到 order 对应的空闲链表。 */
static void
buddy_insert_block(struct Page *base, size_t order) {
    base->property = (unsigned int)order;
    SetPageProperty(base);
    list_add(&buddy_free_areas[order].free_list, &base->page_link);
    buddy_free_areas[order].nr_free += 1UL << order;
    buddy_nr_free += 1UL << order;
}

static void
buddy_init(void) {
    for (size_t order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&buddy_free_areas[order].free_list);
        buddy_free_areas[order].nr_free = 0;
    }
    buddy_nr_free = 0;
    buddy_nr_regions = 0;
}

/*
 * 把 [base, base + n) 这段连续物理页交给 Buddy 管理。
 * 区域按“尽量大的对齐块”切分后逐块插入，因此即使起始页号不是 2 的幂
 * 对齐，也不会产生越界的伙伴块。
 */
static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);

    size_t start = page2ppn(base);
    size_t end = start + n;

    assert(end > start);
    if (buddy_nr_regions < BUDDY_MAX_REGIONS) {
        buddy_regions[buddy_nr_regions].start = start;
        buddy_regions[buddy_nr_regions].end = end;
        buddy_nr_regions++;
    }

    struct Page *p = base;
    for (size_t i = 0; i < n; i++, p++) {
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }

    size_t done = 0;
    p = base;
    while (done < n) {
        size_t order = 0;
        /* 只在“绝对页号对齐”且“剩余空间足够”时才继续升级。 */
        while (order < BUDDY_MAX_ORDER &&
               (((start + done) & ((1UL << (order + 1)) - 1)) == 0) &&
               (done + (1UL << (order + 1)) <= n)) {
            order++;
        }
        buddy_insert_block(p, order);
        done += 1UL << order;
        p += 1UL << order;
    }
}

static struct Page *
buddy_alloc_pages(size_t n) {
    if (n == 0) {
        return NULL;
    }

    size_t order = buddy_order_ceil(n);
    if (order > BUDDY_MAX_ORDER) {
        return NULL;
    }

    size_t cur = order;
    while (cur <= BUDDY_MAX_ORDER &&
           list_empty(&buddy_free_areas[cur].free_list)) {
        cur++;
    }
    if (cur > BUDDY_MAX_ORDER) {
        return NULL;
    }

    struct Page *page =
        le2page(list_next(&buddy_free_areas[cur].free_list), page_link);
    list_del(&page->page_link);
    buddy_free_areas[cur].nr_free -= 1UL << cur;
    buddy_nr_free -= 1UL << cur;
    ClearPageProperty(page);

    /* 从 cur 逐级分裂到 order，多出来的后半块挂回对应链表。 */
    while (cur > order) {
        cur--;
        struct Page *buddy = page + (1UL << cur);
        buddy->flags = 0;
        buddy_insert_block(buddy, cur);
    }

    page->flags = 0;
    page->property = 0;
    set_page_ref(page, 0);
    return page;
}

/*
 * 归还 [base, base + n) 并尽力向上合并。
 * n 会向上取整到 2 的幂，与 alloc_pages 的分块方式保持一致。
 */
static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);

    size_t order = buddy_order_ceil(n);
    assert(order <= BUDDY_MAX_ORDER);

    size_t start = page2ppn(base);
    for (size_t i = 0; i < n; i++) {
        struct Page *p = base + i;
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        p->property = 0;
        set_page_ref(p, 0);
    }

    base->property = (unsigned int)order;
    SetPageProperty(base);

    while (order < BUDDY_MAX_ORDER) {
        size_t block = 1UL << order;
        size_t buddy_ppn = start ^ block;
        size_t merged_start = start < buddy_ppn ? start : buddy_ppn;
        size_t merged_end = merged_start + (block << 1);

        /* 伙伴块必须仍在这段被管理的内存里。 */
        if (!buddy_region_contains(merged_start, merged_end)) {
            break;
        }

        struct Page *buddy = pages + (buddy_ppn - nbase);
        if (!PageProperty(buddy) || buddy->property != order) {
            break;                          /* 伙伴块不空闲或已被分配 */
        }

        list_del(&buddy->page_link);
        buddy_free_areas[order].nr_free -= block;
        buddy_nr_free -= block;             /* 伙伴块并入，先按原块记账 */
        ClearPageProperty(buddy);

        struct Page *lo = (buddy < base) ? buddy : base;
        struct Page *hi = (buddy < base) ? base : buddy;
        ClearPageProperty(hi);              /* hi 变成块内页 */

        order++;
        lo->property = (unsigned int)order;
        SetPageProperty(lo);

        base = lo;
        start = merged_start;
    }

    buddy_insert_block(base, order);
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_nr_free;
}

/* 检查链表结构、order 记录、对齐关系与页数统计是否自洽。 */
static void
buddy_check(void) {
    size_t total = 0;

    for (size_t order = 0; order <= BUDDY_MAX_ORDER; order++) {
        size_t sum = 0;
        list_entry_t *le = &buddy_free_areas[order].free_list;
        while ((le = list_next(le)) != &buddy_free_areas[order].free_list) {
            struct Page *p = le2page(le, page_link);
            assert(PageProperty(p));
            assert(p->property == order);
            assert((page2ppn(p) & ((1UL << order) - 1)) == 0);
            assert(page_ref(p) == 0);
            sum += 1UL << order;
        }
        assert(sum == buddy_free_areas[order].nr_free);
        total += sum;
    }
    assert(total == buddy_nr_free);

    cprintf("buddy_check() succeeded!\n");
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};

/*
 * 自测。为了不打扰当前正在使用的管理器，先向它借一段物理页，再让
 * Buddy 独立管理这段页；测试结束后把页清零并整段还回去。
 * 测试区域强制 1024 页对齐，这样才能验证“全部合并成一个块”。
 */
#define BUDDY_TEST_PAGES 1024

void
buddy_self_test(void) {
    cprintf("[Test C1] Buddy System: begin\n");

    struct Page *run = alloc_pages(2 * BUDDY_TEST_PAGES);
    assert(run != NULL);

    size_t run_ppn = page2ppn(run);
    size_t aligned_ppn =
        (run_ppn + BUDDY_TEST_PAGES - 1) & ~(size_t)(BUDDY_TEST_PAGES - 1);
    assert(aligned_ppn + BUDDY_TEST_PAGES <= run_ppn + 2 * BUDDY_TEST_PAGES);

    struct Page *base = pages + (aligned_ppn - nbase);
    cprintf("[Test C1]   region @0x%lx, %lu pages\n",
            (unsigned long)page2pa(base), (unsigned long)BUDDY_TEST_PAGES);

    buddy_init();
    buddy_init_memmap(base, BUDDY_TEST_PAGES);
    buddy_check();
    assert(buddy_nr_free_pages() == BUDDY_TEST_PAGES);

    /* 1 页：最小分配 */
    struct Page *p = buddy_alloc_pages(1);
    assert(p != NULL && p >= base && p < base + BUDDY_TEST_PAGES);
    assert(!PageProperty(p));
    assert(page_ref(p) == 0);
    assert(buddy_nr_free_pages() == BUDDY_TEST_PAGES - 1);
    buddy_check();

    /* 3 页：向上取整，实际占用 4 页 */
    struct Page *q = buddy_alloc_pages(3);
    assert(q != NULL && q >= base && q < base + BUDDY_TEST_PAGES);
    assert(q + 3 <= base + BUDDY_TEST_PAGES);
    assert(buddy_nr_free_pages() == BUDDY_TEST_PAGES - 5);
    assert(q != p);
    buddy_check();

    /* 此时剩余空间不足，整段分配必须失败 */
    assert(buddy_alloc_pages(BUDDY_TEST_PAGES) == NULL);

    /* 归还并验证逐级合并 */
    buddy_free_pages(q, 3);
    buddy_free_pages(p, 1);
    buddy_check();
    assert(buddy_nr_free_pages() == BUDDY_TEST_PAGES);

    /* 只有完全合并成 2^10 的块，才能满足整段分配 */
    struct Page *whole = buddy_alloc_pages(BUDDY_TEST_PAGES);
    assert(whole == base);
    assert(buddy_nr_free_pages() == 0);
    assert(buddy_alloc_pages(1) == NULL);
    buddy_check();

    buddy_free_pages(whole, BUDDY_TEST_PAGES);
    buddy_check();
    assert(buddy_nr_free_pages() == BUDDY_TEST_PAGES);

    /* 归还给当前管理器前，清掉 Buddy 留下的状态。 */
    for (struct Page *it = run; it < run + 2 * BUDDY_TEST_PAGES; it++) {
        it->flags = 0;
        it->property = 0;
        set_page_ref(it, 0);
    }
    pmm_manager->free_pages(run, 2 * BUDDY_TEST_PAGES);

    cprintf("[Test C1] Buddy System: PASS\n");
}
