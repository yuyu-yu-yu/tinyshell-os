# 第三轮集成记录

## 合并边界

`integration/cycle-03` 从第二轮合入后的 `main` `b517f3e` 建立。集成保留四个成员的独立提交和 Draft PR：

| 成员 | 分支 | 提交 | Draft PR | 模块 |
|---|---|---|---|---|
| A | `feature/cycle-03-a-paging` | `de84b60`、`248e677` | #10 | i386 分页与 CR0.WP |
| B | `feature/cycle-03-b-kheap` | `5ab5640` | #12 | 256 KiB 启动堆 |
| C | `feature/cycle-03-c-kthreads` | `7acaea8` | #13 | 协作式内核任务 |
| D | `feature/cycle-03-d-ipc` | `fb708a4` | #11 | 静态拷贝式 IPC |

合并顺序是 A paging → B heap → D IPC → C tasks，然后再合入 A/B 互审提出的 CR0.WP 加固。每个模块使用独立 merge commit，并在合并后运行标准 Docker/QEMU 矩阵。A/B 已互审内存边界，C/D 已互审上下文切换和 IPC 回绕语义。

## 最终启动路径

内核保留第二轮全部启动与真实 IRQ 检查。在执行 `sti` 前，它依次完成：

1. 导入 Multiboot memory map，保留低端内存、内核和 bootloader 缓冲区，再通过 PMM alloc/free 自检。
2. 初始化 PIC、PIT 和 keyboard，在 IRQ1 masked 时完成 synthetic Shift+`a` 解码测试。
3. 载入页目录，打开 CR0.PG/WP，确认 identity-mapped 内核继续运行。
4. 从 PMM 分配一页，映射到 `0xD0000000`，通过虚拟地址写入哨兵值，从 identity 地址验证同一物理页，然后 translate、unmap 和 free。
5. 验证堆的对齐、calloc、非法释放、拆分、前后合并和统计完全恢复。
6. 验证 IPC endpoint 耗尽、0/3/32/33 字节、深拷贝、tail 清零、满队列、FIFO 和 endpoint 隔离，再重置出一个空 endpoint 供任务使用。

开启中断后，启动路径先等待 PIT tick 和 IRQ0 dispatcher count 同时前进三次，再创建三个协作任务。producer 发送 6 条带序号消息，consumer 检查 sender、type、payload、tail 和 FIFO，timer-observer 用 `hlt + task_yield()` 等待新的三次真实 IRQ0。producer 和 consumer 正常返回，observer 显式调用 `task_exit()`。验收还要求至少 17 次 dispatch，防止空壳 `yield` 通过调度标记。三任务全部完成、队列为空且 switch budget 未耗尽后，内核才输出 `BOOT_OK`。

## 测试证据

2026-08-25 标准 Docker/QEMU 结果：

| QEMU 内存 | `PMM_FREE_PAGES` | 全部第三轮标记 | 结果 |
|---:|---:|---|---|
| 16 MiB | 3569 | 完整 | 通过 |
| 64 MiB | 15857 | 完整 | 通过 |
| 128 MiB | 32241 | 完整 | 通过 |

三档日志都包含 `PAGING_OK`、`VMM_MAP_OK`、`HEAP_OK`、`HEAP_COALESCE_OK`、`TASK_OK`、`SCHEDULER_OK`、`IPC_OK`、`IPC_TASK_FLOW_OK` 和原有标记，且不含 `BOOT_FAIL`。`PMM_FREE_PAGES` 严格递增。

额外的 64 MiB QEMU monitor 检查读到 `CR0=80010011`，证明 PG 和 WP 都已打开。在完成任务 IPC 后发送 `sendkey a`，串口日志末尾回显 `a`。ELF 中 `__kernel_end=0x001ee0a0`，页表、堆和 8 个任务栈都位于 128 MiB identity-map 范围；GNU stack 为 RW、不可执行。

## 已知限制

- VMM 只有一个内核地址空间，动态页表 pool 最多同时覆盖 32 个 PDE，尚未实现每进程页表和 demand paging。
- heap 不会向 PMM 申请扩容，不允许在 IRQ 中使用，也没有并发锁。
- 调度是纯协作式；budget 只计数主动切换，无法抢回一个不 yield 的任务。没有 guard page、优先级、睡眠、FPU 上下文或 Ring 3。
- IPC 不阻塞，endpoint 不销毁；当前单核协作调度下不加锁。引入抢占或多核前必须增加同步。
- page fault 当前只输出 CR2 和 error-code 位并停机，不尝试恢复。
