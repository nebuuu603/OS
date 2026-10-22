#ifndef __KERN_MM_LAB2_C_TEST_H__
#define __KERN_MM_LAB2_C_TEST_H__

/*
 * lab2 成员 C 的自测入口（Buddy System + SLUB）。
 * 只有用 -DLAB2_C_TEST 编译（make ctest）时才会被 pmm_init() 调用，
 * 这样默认构建与官方 make grade 的行为完全不受影响。
 */
void lab2_c_self_test(void);

#endif /* !__KERN_MM_LAB2_C_TEST_H__ */
