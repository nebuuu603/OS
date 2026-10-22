#include <assert.h>
#include <defs.h>
#include <error.h>
#include <list.h>
#include <memlayout.h>
#include <pmm.h>
#include <slub.h>
#include <slub_page.h>
#include <stdio.h>
#include <string.h>

/*
 * 简化版 SLUB：
 *   - 每个 slab 占一页，页首放 struct slub_slab，后面切成等长对象；
 *   - 空闲对象用对象自身的头 8 字节串成单链表（嵌入式 freelist）；
 *   - 每个大小级别维护 slabs（全部）与 partial（还有空对象的）两条链表。
 */

#define SLUB_ALIGN 8
#define SLUB_MAGIC 0x534c5542u              /* "SLUB" */
#define SLUB_MAX_SLABS 32                   /* 描述符池大小 */

#define SLUB_ALIGN_UP(x) (((x) + (SLUB_ALIGN - 1)) & ~((size_t)SLUB_ALIGN - 1))

struct slub_cache;

struct slub_slab {
    uint32_t magic;
    struct slub_cache *cache;
    struct Page *base;                      /* 页描述符 */
    void *base_addr;                        /* 页起始虚拟地址 */
    list_entry_t link;                      /* cache->slabs */
    list_entry_t partial_link;              /* cache->partial */
    void *freelist;                         /* 空闲对象链 */
    size_t nr_free;
    size_t nr_total;
};

#define le2slab(le, member) to_struct((le), struct slub_slab, member)

struct slub_cache {
    size_t obj_size;
    list_entry_t slabs;
    list_entry_t partial;
    size_t nr_free_objects;
    size_t nr_slabs;
};

#define SLUB_NR_CLASSES 9                   /* 8,16,...,2048 共 9 级 */

static struct slub_cache slub_caches[SLUB_NR_CLASSES];
static struct slub_page slub_page_pool[SLUB_MAX_SLABS];
static bool slub_ready;

static size_t
slub_slab_header_size(void) {
    return SLUB_ALIGN_UP(sizeof(struct slub_slab));
}

static void
slub_init_once(void) {
    if (slub_ready) {
        return;
    }
    for (size_t i = 0; i < SLUB_NR_CLASSES; i++) {
        slub_caches[i].obj_size = SLUB_MIN_OBJECT << i;
        list_init(&slub_caches[i].slabs);
        list_init(&slub_caches[i].partial);
        slub_caches[i].nr_free_objects = 0;
        slub_caches[i].nr_slabs = 0;
    }
    slub_page_init();
    slub_ready = 1;
}

/* 把 size 向上取整到大小级别；超出上限返回 NULL。 */
static struct slub_cache *
slub_cache_for(size_t size) {
    if (size == 0 || size > SLUB_MAX_OBJECT) {
        return NULL;
    }
    size_t index = 0;
    while ((SLUB_MIN_OBJECT << index) < size) {
        index++;
    }
    return &slub_caches[index];
}

/* 从描述符池里取一个空闲的 slub_page，并向页层申请一页。 */
static struct slub_page *
slub_page_pool_get(void) {
    for (size_t i = 0; i < SLUB_MAX_SLABS; i++) {
        if (slub_page_pool[i].addr == NULL) {
            if (slub_page_alloc(&slub_page_pool[i], 1) != NULL) {
                return &slub_page_pool[i];
            }
            return NULL;
        }
    }
    return NULL;
}

/*
 * 新建一个 slab：申请一页，把对象区切成等长对象并串成空闲链。
 * 返回 NULL 表示页层已经无法再提供内存。
 */
static struct slub_slab *
slub_grow(struct slub_cache *cache) {
    struct slub_page *page = slub_page_pool_get();
    if (page == NULL) {
        return NULL;
    }

    size_t header = slub_slab_header_size();
    size_t total = (PGSIZE - header) / cache->obj_size;
    if (total == 0) {
        slub_page_free(page);
        return NULL;
    }

    struct slub_slab *slab = (struct slub_slab *)page->addr;
    slab->magic = SLUB_MAGIC;
    slab->cache = cache;
    slab->base = page->base;
    slab->base_addr = page->addr;
    slab->freelist = NULL;
    slab->nr_free = total;
    slab->nr_total = total;
    list_init(&slab->link);
    list_init(&slab->partial_link);

    /* 逆序串链，保证分配时从低地址开始。 */
    char *area = (char *)page->addr + header;
    for (size_t i = total; i > 0; i--) {
        void *obj = area + (i - 1) * cache->obj_size;
        *(void **)obj = slab->freelist;
        slab->freelist = obj;
    }

    list_add(&cache->slabs, &slab->link);
    list_add(&cache->partial, &slab->partial_link);
    cache->nr_slabs++;
    cache->nr_free_objects += total;
    return slab;
}

/* slab 全部空闲时把它还给页层，但每个级别至少留一个缓存。 */
static void
slub_release_slab(struct slub_slab *slab) {
    struct slub_cache *cache = slab->cache;

    list_del_init(&slab->link);
    if (!list_empty(&slab->partial_link)) {
        list_del_init(&slab->partial_link);
    }
    cache->nr_slabs--;
    cache->nr_free_objects -= slab->nr_free;

    struct Page *base = slab->base;
    uintptr_t addr = (uintptr_t)slab->base_addr;

    /* 先清掉元数据，避免页被别的分配器立刻复用后又被写坏。 */
    slab->magic = 0;
    slab->freelist = NULL;
    slab->nr_free = 0;
    slab->nr_total = 0;
    slab->base = NULL;
    slab->base_addr = NULL;
    slab->cache = NULL;

    /* 找到它在描述符池里的位置，再交还给页层。 */
    for (size_t i = 0; i < SLUB_MAX_SLABS; i++) {
        if (slub_page_pool[i].addr == (void *)addr &&
            slub_page_pool[i].base == base) {
            slub_page_free(&slub_page_pool[i]);
            break;
        }
    }
}

static bool
slub_slab_in_cache(struct slub_cache *cache, struct slub_slab *slab) {
    list_entry_t *le = &cache->slabs;
    while ((le = list_next(le)) != &cache->slabs) {
        if (le2slab(le, link) == slab) {
            return 1;
        }
    }
    return 0;
}

void *
slub_alloc(size_t size) {
    slub_init_once();

    struct slub_cache *cache = slub_cache_for(size);
    if (cache == NULL) {
        return NULL;
    }

    if (list_empty(&cache->partial)) {
        if (slub_grow(cache) == NULL) {
            return NULL;
        }
    }

    struct slub_slab *slab = le2slab(list_next(&cache->partial), partial_link);
    assert(slab->magic == SLUB_MAGIC);
    assert(slab->freelist != NULL);

    void *obj = slab->freelist;
    slab->freelist = *(void **)obj;
    slab->nr_free--;
    cache->nr_free_objects--;

    if (slab->nr_free == 0) {
        list_del_init(&slab->partial_link);
    }

    memset(obj, 0, size);
    return obj;
}

int
slub_free(void *ptr) {
    if (ptr == NULL) {
        return E_INVAL;
    }

    slub_init_once();

    /* slab 恰好占一页，页首就是元数据。 */
    uintptr_t addr = (uintptr_t)ptr;
    struct slub_slab *slab =
        (struct slub_slab *)(addr & ~(uintptr_t)(PGSIZE - 1));

    if (slab->magic != SLUB_MAGIC) {
        return E_INVAL;
    }

    struct slub_cache *cache = slab->cache;
    if (cache == NULL || !slub_slab_in_cache(cache, slab)) {
        return E_INVAL;
    }

    size_t header = slub_slab_header_size();
    uintptr_t area = (uintptr_t)slab->base_addr + header;
    if (addr < area ||
        addr >= area + slab->nr_total * cache->obj_size ||
        ((addr - area) % cache->obj_size) != 0) {
        return E_INVAL;
    }

    *(void **)ptr = slab->freelist;
    slab->freelist = ptr;
    slab->nr_free++;
    cache->nr_free_objects++;

    if (slab->nr_free == slab->nr_total && cache->nr_slabs > 1) {
        slub_release_slab(slab);
    } else if (slab->nr_free > 0 && list_empty(&slab->partial_link)) {
        list_add(&cache->partial, &slab->partial_link);
    }

    return 0;
}

size_t
slub_nr_free_objects(void) {
    slub_init_once();
    size_t total = 0;
    for (size_t i = 0; i < SLUB_NR_CLASSES; i++) {
        total += slub_caches[i].nr_free_objects;
    }
    return total;
}

size_t
slub_nr_slabs(void) {
    slub_init_once();
    size_t total = 0;
    for (size_t i = 0; i < SLUB_NR_CLASSES; i++) {
        total += slub_caches[i].nr_slabs;
    }
    return total;
}

/* ------------------------------------------------------------------ */
/* 自测                                                               */
/* ------------------------------------------------------------------ */

#define SLUB_TEST_OBJECTS 128

void
slub_self_test(void) {
    cprintf("[Test C2] SLUB: begin\n");

    /* 边界：0 与超过上限的请求必须失败 */
    assert(slub_alloc(0) == NULL);
    assert(slub_alloc(SLUB_MAX_OBJECT + 1) == NULL);

    /* 分级：1/24/200 字节分别落到 8/32/256 字节的级别 */
    void *a = slub_alloc(1);
    void *b = slub_alloc(24);
    void *c = slub_alloc(200);
    assert(a != NULL && b != NULL && c != NULL);
    assert(a != b && b != c && a != c);
    assert(((uintptr_t)a % SLUB_ALIGN) == 0);
    assert(((uintptr_t)b % SLUB_ALIGN) == 0);

    /* 对象互不重叠且可写回读 */
    memset(a, 0xa5, 8);
    memset(b, 0x5a, 24);
    memset(c, 0xc3, 200);
    for (int i = 0; i < 8; i++) {
        assert(((unsigned char *)a)[i] == 0xa5);
    }
    for (int i = 0; i < 24; i++) {
        assert(((unsigned char *)b)[i] == 0x5a);
    }
    for (int i = 0; i < 200; i++) {
        assert(((unsigned char *)c)[i] == 0xc3);
    }

    assert(slub_free(a) == 0);
    assert(slub_free(b) == 0);
    assert(slub_free(c) == 0);
    assert(slub_free(NULL) == E_INVAL);

    /* 一个页装不下 128 个 128 字节对象，必须跨多个 slab */
    void *objects[SLUB_TEST_OBJECTS];
    for (int i = 0; i < SLUB_TEST_OBJECTS; i++) {
        objects[i] = slub_alloc(128);
        assert(objects[i] != NULL);
        assert(((uintptr_t)objects[i] % SLUB_ALIGN) == 0);
        *(int *)objects[i] = i;
    }
    assert(slub_nr_slabs() >= 2);

    for (int i = 0; i < SLUB_TEST_OBJECTS; i++) {
        for (int j = i + 1; j < SLUB_TEST_OBJECTS; j++) {
            assert(objects[i] != objects[j]);
        }
    }
    for (int i = 0; i < SLUB_TEST_OBJECTS; i++) {
        assert(*(int *)objects[i] == i);
        assert(slub_free(objects[i]) == 0);
    }

    /* 全部归还后对象应当被回收复用 */
    void *again = slub_alloc(128);
    assert(again != NULL);
    assert(slub_free(again) == 0);

    /* 最大级别也能正常分配 */
    void *big = slub_alloc(SLUB_MAX_OBJECT);
    assert(big != NULL);
    memset(big, 0x77, SLUB_MAX_OBJECT);
    assert(slub_free(big) == 0);

    cprintf("[Test C2] SLUB: PASS\n");
}
