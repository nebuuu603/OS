#ifndef __KERN_MM_SLUB_PAGE_H__
#define __KERN_MM_SLUB_PAGE_H__

#include <defs.h>
#include <list.h>
#include <pmm.h>

/* A contiguous page run used as the backing storage of one SLUB slab. */
struct slub_page {
    struct Page *base;
    void *addr;
    size_t nr_pages;
    list_entry_t link;
};

/* Initialize the page-layer bookkeeping. Safe to call more than once. */
void slub_page_init(void);

/*
 * Allocate a contiguous run of physical pages for a slab.
 * The caller owns the descriptor and must keep it until slub_page_free().
 * Returns page on success, NULL on invalid input or allocation failure.
 */
struct slub_page *slub_page_alloc(struct slub_page *page, size_t nr_pages);

/* Release a page run previously returned by slub_page_alloc(). */
int slub_page_free(struct slub_page *page);

/* Statistics for diagnostics and tests. */
size_t slub_page_nr_slabs(void);
size_t slub_page_nr_pages(void);

#endif /* !__KERN_MM_SLUB_PAGE_H__ */
