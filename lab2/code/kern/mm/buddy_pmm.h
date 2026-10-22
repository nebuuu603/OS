#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

/*
 * Buddy System 物理内存管理器。
 *
 * 空闲内存被组织成 2^order 个连续页的块（order 0..BUDDY_MAX_ORDER），
 * 每个 order 一条按物理地址升序的空闲链表，块首页的 property 字段记录
 * 该块的 order。分配时向上取整到 2 的幂，并从最小的足够大的块中分裂；
 * 释放时按同样规则还原 order，并与相邻的伙伴块逐级合并。
 */
extern const struct pmm_manager buddy_pmm_manager;

/*
 * 自测：从当前 pmm_manager 借一段物理页，作为独立区域交给 Buddy 管理，
 * 覆盖初始化、分裂、分配、释放、逐级合并与耗尽等行为，结束后把页还给
 * 当前管理器。要求此时 pmm_init() 已完成（pages / va_pa_offset 已就绪）。
 * 注意：该函数会重置 Buddy 的全局状态，只能在 Buddy 不是当前活动的
 * pmm_manager 时调用。
 */
void buddy_self_test(void);

#endif /* !__KERN_MM_BUDDY_PMM_H__ */
