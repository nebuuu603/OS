#include <defs.h>
#include <error.h>
#include <list.h>
#include <memlayout.h>
#include <pmm.h>
#include <slub_page.h>

static list_entry_t slub_page_list;
static size_t slub_slab_count;
static size_t slub_page_count;
static bool slub_page_ready;

static void
slub_page_ensure_init(void) {
    if (!slub_page_ready) {
        list_init(&slub_page_list);
        slub_slab_count = 0;
        slub_page_count = 0;
        slub_page_ready = 1;
    }
}

static bool
slub_page_is_linked(struct slub_page *page) {
    list_entry_t *le;

    for (le = list_next(&slub_page_list); le != &slub_page_list;
         le = list_next(le)) {
        if (to_struct(le, struct slub_page, link) == page) {
            return 1;
        }
    }
    return 0;
}

void
slub_page_init(void) {
    slub_page_ensure_init();
}

struct slub_page *
slub_page_alloc(struct slub_page *page, size_t nr_pages) {
    struct Page *base;
    uintptr_t pa;

    if (page == NULL || nr_pages == 0) {
        return NULL;
    }

    slub_page_ensure_init();

    /* A live descriptor must be released before it is reused. */
    if (slub_page_is_linked(page)) {
        return NULL;
    }

    /* page2pa() is valid only after pmm_init() has built the Page array. */
    if (pages == NULL || npage <= nbase || va_pa_offset == 0 ||
        nr_pages > npage - nbase) {
        return NULL;
    }

    base = alloc_pages(nr_pages);
    if (base == NULL) {
        return NULL;
    }

    pa = page2pa(base);

    page->base = base;
    page->addr = (void *)(pa + va_pa_offset);
    page->nr_pages = nr_pages;
    list_init(&page->link);
    list_add(&slub_page_list, &page->link);

    slub_slab_count++;
    slub_page_count += nr_pages;
    return page;
}

int
slub_page_free(struct slub_page *page) {
    size_t nr_pages;

    if (page == NULL) {
        return E_INVAL;
    }

    slub_page_ensure_init();

    if (!slub_page_is_linked(page)) {
        return E_INVAL;
    }

    nr_pages = page->nr_pages;
    list_del_init(&page->link);
    free_pages(page->base, nr_pages);

    if (slub_slab_count != 0) {
        slub_slab_count--;
    }
    if (slub_page_count >= nr_pages) {
        slub_page_count -= nr_pages;
    } else {
        slub_page_count = 0;
    }

    page->base = NULL;
    page->addr = NULL;
    page->nr_pages = 0;
    list_init(&page->link);
    return 0;
}

size_t
slub_page_nr_slabs(void) {
    slub_page_ensure_init();
    return slub_slab_count;
}

size_t
slub_page_nr_pages(void) {
    slub_page_ensure_init();
    return slub_page_count;
}
