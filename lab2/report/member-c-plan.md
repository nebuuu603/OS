# lab2 成员 C 工作计划（Buddy System + SLUB）

> 负责人：成员 C（报告中填 2412855-张雅程，请自行确认学号）
> 分支：`lab2-c`　基线标签：`pre-c-baseline`

## 0. 进度速览

| # | 任务 | 状态 | 证据 |
|---|------|------|------|
| 1 | 核对成员 A 的 `page_init()` | ✅ 通过 | 官方 `make grade` → `[Test 1] Page Init: PASS` |
| 2 | 核对成员 B 的 Best-Fit | ✅ 通过 | 官方 `make grade` → `[Test 2] Best-Fit: PASS` |
| 3 | Buddy System 分配器 | ✅ 已实现 | `kern/mm/buddy_pmm.{c,h}` |
| 4 | Buddy 自测 | ✅ 通过 | `make ctest` → `[Test C1] Buddy System: PASS` |
| 5 | SLUB 对象分配器 | ✅ 已实现 | `kern/mm/slub.{c,h}` |
| 6 | SLUB 自测 | ✅ 通过 | `make ctest` → `[Test C2] SLUB: PASS` |
| 7 | 集成到内核（不影响官方评分） | ✅ 完成 | 默认构建 `make grade` 仍 20/20 |
| 8 | 报告章节（功能模块三 + Challenge） | ⬜ 待写 | 见第 7 节 |
| 9 | 运行截图 / 报告图片 | ⬜ 待补 | `report/images/` |

## 1. 成员 A / B 的实现核对

### 1.1 核对方法

仓库自带官方评分链路：`make grade` → 复制到 `/tmp/lab2_grade` → `tools/inject.py` 注入 `tools/test_lab2.c`
（把 `test_page_init()` / `test_best_fit()` 插到 `pmm_init()` 末尾）→ 编译 → QEMU 运行 → `tools/grade.sh` 匹配
`[Test 1] Page Init: PASS` 与 `[Test 2] Best-Fit: PASS`，各 10 分。

结果：**Total Score: 20/20**，说明 A、B 的实现满足官方验收。

> 注意：仓库最初根本无法得到分数——`-device loader` 在 OpenSBI 0.9 下不会设置跳转地址，
> 内核从未执行；`grade.sh` 又依赖 `ulimit -t` 事后杀 QEMU，冷启动时串口日志还是空的。
> 这两处已在本分支修复（见 `git log` 中的 `fix(lab2): boot via -kernel` 与 `make grade deterministic`）。

### 1.2 成员 A：`page_init()`（`kern/mm/pmm.c`）

| 要求（练习 spec） | 实现情况 |
|---|---|
| 读取 DTB 内存范围并校验 | ✅ `get_memory_base()/get_memory_size()`，为 0 或 `mem_base != DRAM_BASE` 时 `panic` |
| 设置 `va_pa_offset` | ✅ `PHYSICAL_MEMORY_OFFSET` |
| 设置 `npage` 上界 | ✅ `PPN(mem_end)`，并检查 `npage > nbase` |
| `pages` 位于 `end` 之后且页对齐 | ✅ `ROUNDUP(end, PGSIZE)` |
| 描述符先全部 reserved | ✅ 逐页清 flags/property/ref 后 `SetPageReserved` |
| 跳过内核与描述符数组 | ✅ `free_base = PADDR(pages) + ROUNDUP(desc_bytes, PGSIZE)` |
| 尾部不满一页不交出 | ✅ `free_nr = (mem_end - free_base) / PGSIZE` |
| 无完整页时不调用 `init_memmap` | ✅ 改为 `panic("no free physical pages")`（终止初始化，符合"不得调用"） |
| 输出探测到的内存范围 | ⚠️ 未单独打印 `physical memory map:`；范围由 `dtb_init()` 打印 |

结论：功能正确、能过官方 8 项检查。两处小差异属于"可接受但可改进"：
1. 手册示例输出里有 `physical memory map: memory: ..., [base, end].`，当前实现没有这一行；
2. `npage` 没有用 `KMEMSIZE` 收敛（`-m` 大于 128MiB 时描述符数组会按实际内存变大，本实验默认 128MiB 无影响）。

建议（低优先级，不改也能过）：在 `page_init()` 里补一行 `cprintf("physical memory map: ...")`，与手册输出对齐。

### 1.3 成员 B：Best-Fit（`kern/mm/best_fit_pmm.c`）

| 要求 | 实现情况 |
|---|---|
| 只在 `lab2`/`YOUR CODE` 注释处补代码 | ✅ 骨架原有 138 行（两个测试函数）未改，仅新增实现 |
| 不修改 `basic_check` / `best_fit_check` | ✅ `git show` 对比可证 |
| 设计 `best_fit_pmm_manager` 并注册 | ✅ 结构体 + 7 个回调 |
| `init_pmm_manager()` 切换 | ✅ `pmm_manager = &best_fit_pmm_manager` |
| 算法语义 | ✅ 遍历全部空闲块取 `property >= n` 且最小者；不足则切分余块按地址插回；释放时前后各合并一次 |

结论：实现完整、能过 `best_fit_check`（`check_alloc_page() succeeded!`）。

## 2. 成员 C 的任务拆解

1. **Buddy System 分配器**：把空闲内存组织成 2^order 页的块，实现分裂、分配、释放、逐级合并。
2. **Buddy 测试**：覆盖初始化、最小分配、非 2 幂请求、耗尽、合并回单一块、非法状态。
3. **SLUB 对象分配器**：在页层（`slub_page.c`）之上做任意大小对象分配。
4. **SLUB 测试**：覆盖分级、互不重叠、跨 slab、复用、边界（0 与超限）。
5. **集成**：默认构建零影响；用 `-DLAB2_C_TEST` 单独开关跑自测。
6. **报告**：功能模块三 + Challenge（SLUB）两节，含 prompt、测试说明、改进空间。

## 3. Buddy System 设计

- **常量**：`BUDDY_MAX_ORDER = 15`（最大块 32768 页 = 128 MiB），共 16 个 order。
- **块头**：空闲块首页 `property` 存 order，`PG_property` 置位，挂在 `buddy_free_areas[order]` 链表上。
- **初始化**：把 `[base, base+n)` 按"绝对页号对齐 + 剩余足够"贪心切成尽量大的 2 幂块逐块插入。
  这样即使区域起点不是 2 幂对齐，也不会产生越界的伙伴块。
- **分配**：`order = ceil(log2(n))`，从该 order 向上找第一个非空链表；取块后逐级分裂，
  多余的后半块插回低一级链表。
- **释放**：把 `n` 还原成 `order`，与 `ppn ^ (1<<order)` 的伙伴块比较；若伙伴空闲且同 order 就合并，
  直到不能合并为止，最后插入对应链表。
- **区域保护**：`init_memmap` 记录区域 `[start, end)`，合并前检查合并后的整块是否仍在同一区域内，
  避免把别的管理器（例如 Best-Fit）的页误当伙伴块。
- **记账**：合并时同时减少 `buddy_free_areas[order].nr_free` 与全局 `buddy_nr_free`，
  最终插入合并块时再加回，保证 `sum(各级) == 全局`。

## 4. SLUB 设计

- **两级结构**：第一层 `slub_page.c`（成员 A 编写）负责申请/归还整页；第二层本模块把页切成对象。
- **大小分级**：8、16、…、2048 字节共 9 级，请求向上取整；`size==0` 或 `>2048` 返回 `NULL`。
- **slab 布局**：一页 = `struct slub_slab` 头 + 等长对象区；空闲对象用自身前 8 字节串成单向链表。
- **增长**：`partial` 链表为空时新建 slab（页层给一页），把对象逆序串链，挂到 `slabs` 与 `partial`。
- **释放**：按页对齐反推 slab 头，用 magic 与地址区间校验指针合法性；归还对象到 freelist；
  slab 全空且该级别 slab 数 > 1 时把页还给页层，否则挂回 `partial` 复用。
- **安全性**：`slub_free()` 对空指针、非 slab 指针、越界/未对齐对象返回 `E_INVAL`，不会误写内存。

## 5. 测试覆盖矩阵

| 用例 | 覆盖的行为 | 为什么有效 |
|---|---|---|
| `buddy_self_test` 初始化后 `nr_free == 1024` | 区域切分与记账 | 切分错误会立刻体现为页数不符 |
| 分配 1 页、3 页 | 最小分配 + 向上取整 | 3 页必须按 4 页记账 |
| 分配后立即整段分配失败 | 耗尽检测 | 剩余 1019 页不可能满足 1024 页 |
| 释放后整段分配成功且返回 base | 逐级合并 | 只有完全合并成 order 10 才能满足 |
| `buddy_check()` 每个阶段调用 | 链表结构、order、对齐、计数一致 | 任一处漏记账/漏合并都会被断言抓住 |
| SLUB 尺寸 1/24/200 | 分级取整 | 分别落到 8/32/256 级 |
| 128 个 128B 对象互不重叠并可回读 | 跨 slab 分配 | 一页只放得下 31 个，必然多 slab |
| 全部释放后再分配 | 复用与 slab 回收 | 释放后 freelist/slab 记账错误会失败 |
| `slub_alloc(0)`、`slub_alloc(2049)`、`slub_free(NULL)` | 边界与非法输入 | 要求明确返回 NULL / `E_INVAL` |

## 6. 构建与验证

```bash
# WSL: cd /mnt/c/Users/zyc/Desktop/操作系统/OS/lab2/code
make grade     # 官方评分，应输出 Total Score: 20/20
make ctest     # 成员 C 自测，应输出 [Test C1]/[Test C2] PASS（Ctrl+A 后 X 退出）
make qemu      # 普通启动
```

`make ctest` 通过 `CFLAGS3 = $(CFLAGS) -D LAB2_C_TEST` 编译，`pmm_init()` 末尾调用
`lab2_c_self_test()`；默认构建不定义该宏，自测代码不会执行，因此不影响官方评分。

## 7. 报告待写章节

1. **功能模块三：Buddy System**（任务三）
   - 两份 prompt：① 生成分配模块并接入内核；② 为它生成测试
   - 测试覆盖说明与有效性论证（可直接用第 5 节表格）
2. **Challenge：SLUB**（选做加分）
   - 两份 prompt：① 生成 SLUB 模块；② 生成测试
   - 设计说明、测试覆盖、与 Linux SLUB 的差异
3. **改进空间**（练习要求回答）：Buddy 可用位图/多级位图降低内存开销、按 CPU 分桶；
   SLUB 可加 per-CPU 缓存、对象构造函数、slab 着色、更细的 size class。
4. **AI 协作 bug 记录**：本次实际踩到的坑可作为素材——
   - `-device loader` 在 OpenSBI 0.9 下不设置跳转地址，内核根本没跑（0/20 的根因）
   - `grade.sh` 依赖 `ulimit -t` 事后杀 QEMU，冷启动时日志为空
   - Buddy 合并时只减分级计数、没减全局计数，`buddy_check()` 断言失败

## 8. Git 工作流

```bash
git log --oneline --decorate         # 查看提交
git tag                              # 查看检查点
git diff pre-c-baseline..HEAD        # 本次全部改动
git checkout pre-c-baseline          # 回到 C 开始前的状态（脱离分支）
git checkout lab2-c                  # 回到工作分支
git reset --hard c-buddy-slub        # 回到"Buddy+SLUB 已验证"的检查点
```

| 标签 | 含义 |
|---|---|
| `pre-c-baseline` | 成员 C 开始前的原始状态（A、B 的成果） |
| `c-tooling` | QEMU 启动 / 评分脚本修复完成、`make grade` 20/20 |
| `c-buddy-slub` | Buddy + SLUB 实现与自测通过 |

## 9. 待办与风险

- [ ] 报告两节 + prompt 文本（第 7 节）
- [ ] `make qemu` / `make ctest` / `make grade` 截图放入 `report/images/`
- [ ] 与 A、B 确认：`page_init` 是否补 `physical memory map` 打印（涉及 A 的文件，需其同意）
- [ ] 提交前确认代码里保留 `lab2` 注释、`YOUR CODE` 已按老师要求替换为学号
- 风险：`grade.sh` 属于官方脚本，本分支为适配本机 QEMU 做了两处修改，提交前需与组员确认是否保留
