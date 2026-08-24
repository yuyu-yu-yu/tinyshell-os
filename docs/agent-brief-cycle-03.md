# TinyShell OS 第三轮 Agent 任务书

> 日期：2026-08-25 至 2026-08-26
> 目标：在两天内打通“物理页 → 分页 → 启动堆 → 协作任务 → 消息 IPC”的内核演示路径。

## 给四位组员的使用方法

组长给每人分配 A、B、C、D 中的一个字母。请把这份 Markdown 完整发给你的 AI Agent，然后只补一句：

```text
我是 TinyShell OS 成员 X。请严格执行任务书中 X 的第一天任务，在指定分支和文件边界内实现、测试、提交、推送并建立 Draft PR。遇到问题先给出证据，不得修改其他成员的文件。
```

可以让 Agent 负责写代码和跑测试，但成员必须读懂公开接口、核心数据结构和 PR 中的五个答辩问答。每人上传前至少自己解释一次正常路径和两个失败边界。

## 统一基线和 Git 规则

四个分支必须从第二轮已合入后的最新 `main` 建立：

```text
A: feature/cycle-03-a-paging
B: feature/cycle-03-b-kheap
C: feature/cycle-03-c-kthreads
D: feature/cycle-03-d-ipc
集成: integration/cycle-03
```

开工前执行：

```powershell
git fetch origin
git switch main
git pull --ff-only origin main
git status --short --branch
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
git switch -c <本人的固定分支>
```

如果 `status` 不干净、`main` 无法 fast-forward，或基线 Docker 失败，立即停止并把证据发给组长。禁止 `reset --hard`、`clean -fd`、force push、删除他人文件或把本机生成物提交到 Git。

第一天 Makefile 会自动发现 `kernel/**/*.c` 和 `boot/**/*.S`，四人都不得修改 Makefile、`kernel/kernel.c`、Dockerfile、CI、linker script 或集成文档。只有 A 在第二天的集成分支修改这些公共文件。

## A：i386 分页

### 文件所有权

```text
include/arch/x86/paging.h
kernel/arch/x86/paging.c
include/mm/vmm.h
kernel/mm/vmm.c
kernel/arch/x86/interrupt.c   # 只增加 vector 14 分派
```

### 冻结的公开接口

```c
enum {
    VMM_PAGE_SIZE = 4096,
    VMM_IDENTITY_LIMIT = 0x08000000,
    VMM_DYNAMIC_START = 0x40000000,
    VMM_DYNAMIC_END = 0xF0000000,
};

enum vmm_page_flags {
    VMM_WRITABLE = 1U << 0,
    VMM_USER = 1U << 1,
};

bool vmm_init(void);
bool vmm_map_page(uintptr_t virtual_address,
                  uintptr_t physical_address,
                  uint32_t flags);
bool vmm_unmap_page(uintptr_t virtual_address,
                    uintptr_t *physical_address);
bool vmm_translate(uintptr_t virtual_address,
                   uintptr_t *physical_address);
bool vmm_is_enabled(void);
```

### 实现与验收

- 使用一张 4 KiB 对齐页目录和 64 张静态页表。前 32 张 identity-map `[0x1000, 128 MiB)`，后 32 张按需服务动态窗口。
- 第 0 页保持 non-present；内核、栈、VGA 和静态 BSS 在 identity map 内。
- map/unmap 只接受 `[VMM_DYNAMIC_START, VMM_DYNAMIC_END)` 内的页对齐虚拟地址。map 同时要求物理地址页对齐，并拒绝未知 flag 和重复映射。
- translate 接受页内偏移；失败时保持输出参数原值。unmap 执行 `invlpg`，但不释放物理页。
- 页表 pool 耗尽时返回 false，不覆盖已有页表。未启用分页前 map/unmap/translate 都失败。
- `vmm_init()` 最后写 CR3，设置 CR0.PG，再将 enabled 状态设为 true。内部第二次调用应幂等成功。
- vector 14 读取 CR2，输出 fault address、error code 和 present/write/user 位后 `cli/hlt`。不得将其当作可恢复异常。
- 临时 host/freestanding 测试覆盖对齐、边界、重复映射、翻译偏移、unmap 不改失败输出和 pool 耗尽。

第二天由 A 在集成分支从 PMM 分配一页，映射到 `0xD0000000`，通过虚拟地址写入哨兵值，从 identity 物理地址验证内容，再 translate、unmap 和 free。成功标记是 `PAGING_OK` 和 `VMM_MAP_OK`。

## B：256 KiB 启动堆

### 文件所有权

```text
include/mm/heap.h
kernel/mm/heap.c
```

### 冻结的公开接口

```c
struct heap_stats {
    size_t total_bytes;
    size_t free_bytes;
    size_t largest_free_block;
    uint32_t allocated_blocks;
};

bool kheap_init(void);
void *kmalloc(size_t size);
void *kcalloc(size_t count, size_t size);
bool kfree(void *pointer);
bool kheap_get_stats(struct heap_stats *stats);
bool kheap_validate(void);
```

### 实现与验收

- arena 是 BSS 中 256 KiB、16 字节对齐的静态数组，不依赖 PMM 或 A 的分页。
- 使用 first-fit，支持块拆分和前后双向合并。返回地址至少 16 字节对齐；过小的剩余空间不得生成无法使用的块。
- 元数据包含 magic、state、payload size 和相邻关系。`kfree` 拒绝 NULL、arena 外地址、块内部地址和重复释放；失败不改变堆。
- `kmalloc(0)` 返回 NULL。`kcalloc` 先检查 `count * size` 溢出，再把请求的 payload 全部清零。
- validator 检查块边界、对齐、总长、相邻链接、magic/state 和统计一致性。`kheap_get_stats(NULL)` 失败且不改状态。
- 本轮堆不扩容、不在 IRQ handler 中调用、不保证并发安全。
- 临时测试覆盖对齐、拆分、碎片、前后合并、calloc 清零/溢出、非法释放和完全恢复。

第二天集成测试在释放中间块后分别与前后空闲块合并，并验证最终统计回到初始状态。成功标记是 `HEAP_OK` 和 `HEAP_COALESCE_OK`。

## C：协作式内核任务

### 文件所有权

```text
include/task/task.h
kernel/task/task.c
boot/context_switch.S
```

### 冻结的公开接口

```c
enum {
    TASK_MAX_TASKS = 8,
    TASK_STACK_SIZE = 16384,
};

typedef uint32_t task_id_t;
typedef void (*task_entry_t)(void *argument);

#define TASK_INVALID_ID UINT32_MAX

void task_system_init(void);
task_id_t task_create(task_entry_t entry, void *argument);
bool task_run(uint32_t switch_budget);
void task_yield(void);
_Noreturn void task_exit(void);
task_id_t task_current_id(void);
uint32_t task_switch_count(void);
uint32_t task_finished_count(void);
```

### 实现与验收

- 任务状态只有 UNUSED、READY、RUNNING、FINISHED。最多 8 个任务，每个使用 16 KiB 静态栈，不从 heap 或 PMM 分配。
- trampoline 调用 entry(argument)；entry 正常返回后自动进入 `task_exit()`。初始栈必须满足 i386 16 字节调用对齐。
- 上下文保存/恢复 ESP、EBP、EBX、ESI、EDI 和 EFLAGS。汇编文件必须包含 `.note.GNU-stack`。
- 调度器更改任务表时保存 IF 并临时 `cli`；切换后恢复对应任务的 EFLAGS。`task_yield()` 只允许普通任务上下文调用。
- round-robin 严格从当前位置后选择 READY 任务。全部结束后返回 bootstrap。
- budget 用尽时 `task_run` 返回 false，保留 READY/RUNNING 状态，后续调用可继续。有 READY 任务时 `task_run(0)` 返回 false。
- 本轮不从 IRQ 中调度，不实现抢占、睡眠、优先级、FPU 和用户任务。
- 临时测试用三任务生成严格轮转 trace，覆盖正常 return、显式 exit、budget 恢复、callee-saved 寄存器和 IF。

第二天成功标记是 `TASK_OK` 和 `SCHEDULER_OK`。

## D：非阻塞拷贝式 IPC

### 文件所有权

```text
include/ipc/ipc.h
kernel/ipc/ipc.c
```

### 冻结的公开接口

```c
enum {
    IPC_MAX_ENDPOINTS = 8,
    IPC_QUEUE_DEPTH = 8,
    IPC_PAYLOAD_MAX = 32,
};

typedef uint32_t ipc_endpoint_t;
#define IPC_INVALID_ENDPOINT UINT32_MAX

struct ipc_message {
    uint32_t sender;
    uint32_t type;
    uint32_t length;
    uint8_t payload[IPC_PAYLOAD_MAX];
};

void ipc_init(void);
ipc_endpoint_t ipc_endpoint_create(void);
bool ipc_send(ipc_endpoint_t endpoint,
              const struct ipc_message *message);
bool ipc_receive(ipc_endpoint_t endpoint,
                 struct ipc_message *message);
uint32_t ipc_pending(ipc_endpoint_t endpoint);
uint32_t ipc_rejected_count(ipc_endpoint_t endpoint);
```

### 实现与验收

- 使用 8 个静态 endpoint，每个包含深度 8 的 FIFO ring。endpoint 本轮不销毁、不复用。
- send 深拷贝 sender、type、length 和有效 payload，并清零剩余 payload，防止旧数据泄漏。
- 拒绝无效 endpoint、NULL、length > 32 和满队列。只有满队列才增加 rejected count，并且不覆盖旧消息。
- receive 在空队列、无效 endpoint 或 NULL 上失败；失败时不改调用者输出。
- IPC 不包含 task、heap 或 VMM 头文件，不阻塞、不自动 yield、不在 IRQ 中调用。本轮协作调度下不加锁；引入抢占前必须增加同步。
- 临时测试覆盖 endpoint 耗尽、0/32/33 字节、FIFO、ring 回绕、满队列、endpoint 隔离、深拷贝和失败输出不变。

第二天成功标记是 `IPC_OK` 和 `IPC_TASK_FLOW_OK`。

## 第一天的共同交付

每人用约 1 小时确认接口和文件所有权，5 小时实现与边界测试，2 小时完成 Docker、复核、提交和 PR。A/B 互审内存模块，C/D 互审任务和 IPC。

提交前必须执行：

```powershell
git status --short --branch
git diff --check
git diff -- <本人授权路径>
git add <逐个写出授权文件，禁止 git add . 和 git add -A>
git diff --cached --name-status
& "$env:USERPROFILE\.codex\skills\safe-git-workflow\scripts\safe_git_scan.ps1" -Mode staged
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
```

建议提交信息：

```text
A: mm: add i386 paging
B: mm: add bootstrap kernel heap
C: task: add cooperative kernel threads
D: ipc: add fixed message endpoints
```

推送后建立以 `main` 为 base 的 Draft PR。PR 正文必须包含：成员与分支、改动文件、实现内容、测试命令与结果、边界测试、已知限制、五个答辩问题及答案。每人都要等 GitHub Actions 通过，但第一天不得自行合并 PR。

## 第二天集成顺序

A 从最新 `main` 建立 `integration/cycle-03`，按 A paging → B heap → D IPC → C tasks 的顺序使用独立 merge commit 合并。每次合并前扫描 staged diff，合并后立即运行 Docker。只有 A 在集成分支修改 `kernel/kernel.c`、Makefile、`docs/architecture.md` 和 `docs/cycle-03-integration.md`。

集成启动流程是：

1. 保留第二轮的 Multiboot、PMM、PIC/IRQ、PIT 和 keyboard 全部自检。
2. 启用 VMM，完成 PMM page → `0xD0000000` → write/translate/unmap/free 测试。
3. 初始化 heap，验证对齐、calloc 和前后合并。
4. 初始化 IPC 和任务系统，建立 producer、consumer、timer-observer 三个任务。
5. producer 发送递增序号并 yield；consumer 检查 sender、type、payload 和 FIFO；timer-observer 用 `hlt + task_yield` 等待真实 PIT 前进。
6. `task_run(256)` 返回后检查三任务完成、switch count、全部消息和 IRQ0 计数。
7. 输出第三轮标记，然后返回键盘 echo 主循环。

新增必须标记：

```text
PAGING_OK
VMM_MAP_OK
HEAP_OK
HEAP_COALESCE_OK
TASK_OK
SCHEDULER_OK
IPC_OK
IPC_TASK_FLOW_OK
```

第二轮所有标记和 `BOOT_OK` 必须保留。最终 `make test` 要在 16、64、128 MiB 三档内存下通过，集成 PR 和合入后的 `main` GitHub Actions 也必须通过。

## 答辩准备清单

每人的五个问答至少覆盖以下五类：

1. 为什么选择当前数据结构？
2. 最重要的两个边界检查是什么？
3. 某个失败路径如何保证输出或状态不变？
4. 当前实现为什么还不支持抢占或并发？
5. 下一轮将这个模块扩展到 Ring 3 时要改什么？

回答必须引用自己的函数、状态字段或测试现象，不要只背操作系统概念。
