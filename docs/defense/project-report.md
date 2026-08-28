# TinyShell OS 项目文档（答辩内容稿）

> 文档状态：第四轮 Day 2 收尾稿（`docs/cycle-04-d-finalize`）。
> 代码基线：`integration/cycle-04`（Runtime 已接线）。
> 组号、姓名、学号：由组员本人填写，Agent 不得代填。
> 第四轮 Shell / RAMFS / 真实 sendkey：已接入 Ring 0 前台；标准 `make test` 在启动矩阵之后运行 64 MiB `sendkey`。仍不是用户态服务。

## 阅读导航

**3 分钟速读（答辩开场）：** 封面与成员 → 第 2 节范围与诚实边界 → 第 4 节启动路径 → 第 8 节已通过的测试表 → 第 11 节已知限制。

**技术审阅（老师或互审）：** 第 3 节 Docker 复现 → 第 4–7 节机制 → `docs/cycle-01-integration.md`、`docs/cycle-02-integration.md`、`docs/cycle-03-integration.md`、`docs/cycle-04-integration.md` → 第 8 节失败处理 → 第 10 节贡献与 AI 使用。

---

## 1. 封面、成员与阅读导航

| 项目 | TinyShell OS |
|---|---|
| 课程 | 操作系统课程设计 |
| 架构 | i386（32 位）、GRUB Multiboot v1、freestanding C11 |
| 仓库 | https://github.com/yuyu-yu-yu/tinyshell-os |
| 当前可演示基线 | `integration/cycle-04`，总集成 PR #18 |
| 组号 | （待填） |
| 组长 | （待填） |
| 成员 | A / B / C / D（姓名、学号待本人填写） |

本文件是内容完整、可直接排版的 Markdown 初稿。正式 Word/PDF 在代码冻结后的 8 月 28–29 日生成，不作为第四轮 CI 阻塞项。

## 2. 目标、选题范围和诚实边界

TinyShell OS 的长期目标是教学型微内核：把调度、地址空间、异常中断和 IPC 留在内核，把 Shell 与文件系统语义放到用户态服务。

**已经通过自动测试、可以答辩陈述的范围：**

- GRUB 加载 32 位内核；Console（VGA + COM1）；平坦 GDT；IDT 与 32 个 CPU 异常；真实 `int3` 返回。
- Multiboot v1 memory map；单页物理分配器（PMM）；PIC/IRQ；100 Hz PIT；PS/2 Set 1 键盘解码。
- 非 PAE 4 KiB 分页、CR0.PG/WP、前 128 MiB identity map、动态窗口映射。
- 256 KiB 启动堆（first-fit、相邻合并）。
- 最多 8 个协作式内核任务；静态 FIFO endpoint 上的非阻塞深拷贝 IPC。
- Ring 0 TinyShell：行编辑、固定命令集、静态 RAMFS、`status` 快照；IRQ1 只入队，前台执行命令。
- 64 MiB QEMU `sendkey` 交互脚本走真实键盘路径（Backspace 按终端可见文本断言，而不是原始 `\b \b` 字节）。

**明确不在本轮、也不能在答辩中宣称完成的范围：**

TSS、Ring 3、系统调用、用户地址空间、抢占调度、阻塞 IPC、磁盘驱动、持久化文件系统、引号/管道/重定向。

答辩口径（必须原样使用）：

> TinyShell OS 当前是具备内存、中断、任务和 IPC 核心机制的教学型微内核原型；本轮 Shell 与 RAMFS 运行在 Ring 0，尚未完成用户态隔离。

## 3. Docker 环境与复现步骤

Docker 是四人共用的唯一验收环境。仓库 `Dockerfile` 钉死 Ubuntu 24.04 摘要，并安装 GCC 13、32 位支持、GRUB 2、QEMU 8、binutils、GDB、NASM、xorriso 与 Python 3。禁止在宿主机或容器里临时 `apt-get` 未写入 Dockerfile 的包来“凑过”测试。

Windows：

```powershell
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
```

Linux / macOS / WSL：

```bash
bash tools/docker-test.sh
```

脚本构建 `tinyshell-os-dev:toolchain-v1`，在容器内 `make clean test`。`make test` 先用 16/64/128 MiB QEMU 检查启动标记、`BOOT_OK` 只出现一次、无 `BOOT_FAIL:` / `PAGE_FAULT`，且 `PMM_FREE_PAGES` 严格递增；然后运行 64 MiB `tools/qemu-shell-test.py` 真实 `sendkey` 交互。

交互式运行：

```bash
docker build --tag tinyshell-os-dev:toolchain-v1 .
docker run --rm -it --volume "$PWD:/workspace" --workdir /workspace \
  tinyshell-os-dev:toolchain-v1 make run
```

第四轮已把 64 MiB 真实 `sendkey` 接到标准 `make test` 末尾。脚本使用容器 `/tmp` 上的 monitor Unix socket，不把 socket 放到 Windows bind-mounted 的 `build/`。Backspace 断言先把串口的 `BS-space-BS` 还原成可见文本，再检查命令是 `echo hello`。

## 4. 总体架构和 GRUB 到 `BOOT_OK` 的启动路径

```text
GRUB (Multiboot EAX=0x2BADB002, EBX=info)
  → console_init
  → gdt_init / idt_init / int3
  → multiboot_parse（校验 mmap）
  → PMM：reset、导入可用区、保留低端/内核/boot 缓冲、单页 alloc/free
  → PIC + PIT 100 Hz + keyboard 软件自检
  → 注册 IRQ0/IRQ1，unmask，仍在 cli
  → 分页：载入页目录，CR0.PG|WP，VMM 窗口自检
  → heap / IPC 静态自检
  → RAMFS / 行编辑 / parser 合成自检（失败则 BOOT_FAIL:shell-runtime）
  → sti
  → 等待真实 IRQ0 三次
  → 三任务 producer / consumer / timer-observer
  → system_status_read 自检
  → SYSTEM_STATUS_OK / SHELL_READY / BOOT_OK
  → shell_runtime_start（`tiny> `）
  → 主循环：hlt；keyboard_pop_char → shell_runtime_handle_char
```

硬件结构集中在 `boot/` 与 `kernel/arch/x86/`。内存在 `kernel/mm/`，任务在 `kernel/task/`，IPC 在 `kernel/ipc/`。Shell 与 RAMFS 按架构文档最终应在用户态；本轮它们若合入，也仍在 Ring 0 内核中运行。

## 5. 中断、内存、分页和 heap

**中断：** IDT 覆盖 256 项；向量 0–31 为 CPU 异常 stub，按是否有 error code 统一栈布局。IRQ 从向量 32 起。`irq_dispatch` 先计数再调 handler，最后 EOI。IRQ0 只增加 PIT tick；IRQ1 只读一个扫描码并入队。命令解析、打印和 RAMFS 只在前台主循环执行。

**PMM：** 只接收 A 裁剪后的区域；首次 `pmm_alloc_page` 后冻结 add/reserve。不把物理地址当指针解引用。16/64/128 MiB 下空闲页数严格递增，证明使用了 GRUB 真实 memory map。

**分页：** 非 PAE、4 KiB、单内核地址空间，identity-map 前 128 MiB，CR0.WP 打开。`vmm_unmap_page()` 不释放物理页。page fault 目前输出 CR2 后停机。

**Heap：** 256 KiB 静态 arena，16 字节对齐，first-fit，释放时前后合并。`kheap_get_stats()` 失败不改调用者输出。不可在 IRQ 中分配，不会向 PMM 扩容。

## 6. 任务调度与 IPC

调度是协作式 round-robin，最多 8 个内核任务，每任务 16 KiB 静态栈。任务只在 `yield`、`exit` 或入口返回时切换；IRQ 不抢占。`task_run(budget)` 防止空壳 yield 刷计数。

IPC 使用静态 endpoint、有界 FIFO、发送时深拷贝。0/3/32 字节合法，33 字节拒绝。不阻塞、不自动 yield、endpoint 不销毁。引入抢占或多核前必须加锁。第三轮 task-flow 要求至少 17 次 dispatch，并与真实 IRQ0 交错。

## 7. RAMFS、Shell 输入与命令执行

第四轮已合入 `integration/cycle-04`。路径为：

```text
IRQ1 → 键盘队列 → keyboard_pop_char → 行编辑器 → parser → RAMFS / status
```

| 模块 | 成员 | 状态 |
|---|---|---|
| 行编辑器 | A | 已接线：127 字符、Backspace、ready 锁定 |
| RAMFS | B | 已接线：16 文件 × 512 字节静态数组 |
| 命令解析 | C | 已接线：无引号、固定命令集 |
| 状态快照 + sendkey 脚本 | D | `system_status_read` 只读公开 API；Makefile 末尾跑真实 IRQ1 测试 |
| Runtime / `kernel_main` | A | `tiny> ` 前台循环 |

固定命令：`help/clear/echo/ls/touch/write/append/cat/rm/status/about`。提示符为 `tiny> `。`about` 含 `Ring 0`。`status` 五行由 Runtime 打印，数据来自 `system_status_read()`。

这些模块都在 Ring 0 运行，不是用户态文件服务。

## 8. 测试方法、结果表和失败处理

### 已通过（第三轮合入 `main` 的证据）

来源：`docs/cycle-03-integration.md`，2026-08-25 Docker/QEMU。

| QEMU 内存 | `PMM_FREE_PAGES` | 标记 | 结果 |
|---:|---:|---|---|
| 16 MiB | 3569 | 含 `PAGING_OK` … `BOOT_OK` | 通过 |
| 64 MiB | 15857 | 同上 | 通过 |
| 128 MiB | 32241 | 同上 | 通过 |

三档均无 `BOOT_FAIL` / `PAGE_FAULT`，`PMM_FREE_PAGES` 严格递增。额外 monitor 读到 `CR0=80010011`（PG+WP）。64 MiB 下 `sendkey a` 可在串口末尾看到 `a`。

启动标记（各一次）：`CONSOLE_OK GDT_OK IDT_OK MULTIBOOT_OK MEMORY_MAP_OK INT3_TEST_OK PMM_OK PMM_ALLOC_FREE_OK PIC_OK IRQ_OK PIT_OK TIMER_IRQ_OK KEYBOARD_DECODE_OK KEYBOARD_READY PAGING_OK VMM_MAP_OK HEAP_OK HEAP_COALESCE_OK TASK_OK SCHEDULER_OK IPC_OK IPC_TASK_FLOW_OK BOOT_OK`。

### 第四轮（`integration/cycle-04` 记录）

来源：`docs/cycle-04-integration.md`。启动矩阵 16/64/128 MiB PASS，`PMM_FREE_PAGES` 为 `3565 < 15853 < 32237`。新增标记各一次，`BOOT_OK` 仍只一次。

`tools/qemu-shell-test.py` 用 monitor `sendkey` 走 IRQ1。Day 2 修复：串口 Backspace 是 `\b \b`，测试先还原可见文本再断言 `echo hello`。标准 `make test` 在启动矩阵之后调用该脚本。

失败处理：模块自检失败走 `BOOT_FAIL:<stage>` 并 `cli; hlt`。交互脚本任何阶段超时或输出不符则打印阶段名、终止 QEMU、删除 `/tmp` monitor socket、非零退出。

## 9. 开发问题及解决过程

1. **IRQ 与命令解析必须分层。** 若在 IRQ1 里跑 parser/RAMFS，会拉长关中断时间并重入非中断安全的 heap。约定 IRQ1 只入队，前台循环消费。
2. **Windows bind mount 上的 Unix socket。** QEMU monitor socket 若放在 `build/`，在 Docker Desktop 的 Windows 卷上会失败。脚本把 socket 放在容器 `/tmp/tinyshell-qemu-monitor-<pid>.sock`。
3. **sendkey 重叠会打满 64 字节键盘队列。** 脚本使用 `sendkey <key> 5`，键间隔 20 ms 且大于 hold。
4. **状态快照的原子性。** `system_status_read` 先填局部 `snapshot`，成功后才复制；NULL 或 heap stats 失败不改调用者内存。`size_t` 在写入 `uint32_t` 前做范围检查。PIT tick 与 IRQ0 允许不是同一周期。
5. **第四轮并行文件所有权。** Day 1 不修改 `kernel.c` / Makefile，避免与 A 抢接线。

## 10. 四人真实贡献、AI 使用和参考来源

**下表只记录仓库中已经发生的模块工作。真实姓名、具体贡献比例和本人修改内容必须由四位成员逐项核对，不能根据 Git 提交者或 Agent 记录推断。**

| 成员 | 已合入 main 的工作（前三轮，据集成记录） | 第四轮已集成工作 |
|---|---|---|
| A | Multiboot 解析与各轮 `kernel_main` 集成、分页 | 行编辑器、Runtime 与第四轮内核接线 |
| B | GDT、PMM、启动堆 | 静态 RAMFS |
| C | IDT/异常、PIC/IRQ、协作任务 | 固定命令解析器 |
| D | Console、PIT/键盘、IPC | 状态快照、真实 `sendkey` 测试、测试门禁与答辩材料框架 |

AI 使用：成员 D 的 Day 1 实现由 Agent 辅助起草，组员必须逐行审查、亲自运行测试并能够口头解释后才能合并。课程 A 级“至少一半代码量由项目组完成”须向老师确认统计口径；无法证明时不得宣称满足。

参考：GNU Multiboot 0.6.96、Intel SDM 中断与分页、QEMU Human Monitor `sendkey`、仓库内三轮 integration 文档。核心算法以组员理解的实现为准，不整段复制教学内核。

## 11. 已知限制、总结和后续工作

- Shell/RAMFS 即使合入也在 Ring 0，不是用户态文件服务。
- 无 TSS、无系统调用、无用户页表、无抢占、无阻塞 IPC。
- Heap 固定 256 KiB；PMM 在首次分配后冻结。
- page fault 不恢复。
- 键盘队列有界，无命令历史、引号、管道。
- RAMFS 重启丢失，无目录。

总结：第三轮已经构成可演示的内核机制闭环。第四轮补的是“人能打字、能看到文件”的前台，不改变微内核原型的定位。

后续：答辩冻结 tag 之后，只有老师明确要求且时间允许，才从稳定 tag 开新分支研究 Ring 3。不得在可答辩的 `main` 上做高风险重写。

## 12. GitHub 地址、最终提交和 Docker 命令

- 仓库：https://github.com/yuyu-yu-yu/tinyshell-os
- 第四轮总集成：`integration/cycle-04`，PR #18；D 的 Day 2 收口通过 PR #19 合入该分支。
- 最终不可变提交与答辩 tag：在最终文档 PR 合入 `main` 且新一轮 main CI 通过后填写，禁止提前写入临时 SHA。
- 复现：`powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1` 或 `bash tools/docker-test.sh`
- 答辩压缩包在**仓库外**制作，不含 `.git`、`build/`、镜像、密钥。邮件由组长发送至 `wang.box@163.com`，格式见 `docs/defense/submission-checklist.md`。
