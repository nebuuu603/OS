#include <buddy_pmm.h>
#include <lab2_c_test.h>
#include <slub.h>

/* lab2 成员 C：在 pmm_init() 末尾按顺序运行 Buddy / SLUB 自测。 */
void
lab2_c_self_test(void) {
    buddy_self_test();
    slub_self_test();
}
