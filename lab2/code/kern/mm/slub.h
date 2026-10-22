#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>

/*
 * SLUB 任意大小对象分配器（lab2 成员 C）。
 *
 * 两层结构：
 *   第一层  slub_page.c  以页为单位向 pmm_manager 申请/归还连续物理页；
 *   第二层  本模块         把每个页切分成固定大小的小对象，按大小分级缓存。
 *
 * 大小分级为 8, 16, 32, ..., 2048 字节；请求会向上取整到最近的一级。
 * 每个 slab 恰好占一页：页首是 slab 元数据，其后是对象区和内嵌的空闲链表。
 */

#define SLUB_MIN_OBJECT 8
#define SLUB_MAX_OBJECT 2048

/* 分配 size 字节，返回 16 字节对齐的对象；失败返回 NULL。 */
void *slub_alloc(size_t size);

/* 释放 slub_alloc() 返回的指针；成功返回 0，指针非法返回 E_INVAL。 */
int slub_free(void *ptr);

/* 统计信息，供测试与诊断使用。 */
size_t slub_nr_free_objects(void);
size_t slub_nr_slabs(void);

/* 自测：覆盖分级、分配、释放、复用、边界与跨 slab 行为。 */
void slub_self_test(void);

#endif /* !__KERN_MM_SLUB_H__ */
