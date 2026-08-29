# 构建与测试

TinyShell OS 使用 Docker 作为标准验收环境。宿主机上的编译可以提供快速反馈，但最终结果以仓库 Dockerfile 中的工具链为准。

## 标准入口

Windows PowerShell：

```powershell
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
```

Linux、macOS 或 WSL：

```bash
bash tools/docker-test.sh
```

脚本构建 `tinyshell-os-dev:toolchain-v1`，把仓库挂载到容器的 `/workspace`，然后运行 `make clean test`。

## 测试流程

```text
Docker toolchain check
  → 编译 freestanding i386 内核
  → 验证 Multiboot 镜像
  → 生成 build/tinyshell.iso
  → QEMU 16 MiB 启动测试
  → QEMU 64 MiB 启动测试
  → QEMU 128 MiB 启动测试
  → 检查 PMM 空闲页递增
  → 64 MiB QEMU monitor sendkey 交互测试
```

## 启动矩阵

每档 QEMU 都必须满足以下条件：

1. `TinyShell OS booting...` 出现一次；
2. `BOOT_OK` 和约定的启动、自检标记各出现一次；
3. `PMM_FREE_PAGES=<number>` 出现一次；
4. 日志不包含 `BOOT_FAIL:` 或 `PAGE_FAULT`；
5. QEMU 在 5 秒启动窗口内完成全部标记输出且不提前退出，随后由 `timeout` 终止。

当前基线结果：

| QEMU 内存 | `PMM_FREE_PAGES` | 结果 |
|---:|---:|---|
| 16 MiB | 3565 | PASS |
| 64 MiB | 15853 | PASS |
| 128 MiB | 32237 | PASS |

严格递增的空闲页数表明 PMM 使用了 GRUB 提供的实际 memory map，而不是写死内存容量。

## 启动标记

`make test` 检查以下标记各出现一次：

```text
CONSOLE_OK
GDT_OK
IDT_OK
MULTIBOOT_OK
MEMORY_MAP_OK
INT3_TEST_OK
PMM_OK
PMM_ALLOC_FREE_OK
PIC_OK
IRQ_OK
PIT_OK
TIMER_IRQ_OK
KEYBOARD_DECODE_OK
KEYBOARD_READY
PAGING_OK
VMM_MAP_OK
HEAP_OK
HEAP_COALESCE_OK
TASK_OK
SCHEDULER_OK
IPC_OK
IPC_TASK_FLOW_OK
RAMFS_OK
SHELL_INPUT_OK
SHELL_PARSE_OK
SYSTEM_STATUS_OK
SHELL_READY
BOOT_OK
```

`INT3_TEST_OK` 证明 CPU exception 能进入公共 dispatcher 并返回。`TIMER_IRQ_OK` 依赖真实 IRQ0 前进，不能通过直接调用 PIT handler 伪造。

## 真实键盘交互

启动矩阵通过后，`tools/qemu-shell-test.py` 会以 64 MiB 启动 QEMU，并通过 monitor `sendkey` 注入 PS/2 键盘事件。测试覆盖：

- `help` 与提示符；
- Backspace 行编辑；
- `touch`、`write`、`append`、`cat`、`ls` 和 `rm`；
- 两次 `status`，确认 PIT tick 与 IRQ0 继续前进；
- 未知命令；
- `about` 中的 Ring 0 边界说明。

`sendkey` 事件实际经过 PIC、IRQ1 stub、键盘 handler、有界队列和前台 Runtime。直接调用 parser 或 `keyboard_feed_scancode()` 只能验证局部函数，不能替代该端到端测试。

测试使用短按键时长和固定键间隔，避免连续输入填满 64 字节队列。QEMU monitor socket 放在容器 `/tmp`，避免 Windows bind mount 不支持 Unix socket 的问题。

## 失败判定

以下任一情况都会使测试非零退出：

- 工具链缺失或内核编译失败；
- 生成物不是有效 Multiboot 镜像；
- 任一启动标记缺失或重复；
- 出现 `BOOT_FAIL:` 或 `PAGE_FAULT`；
- 三档 PMM 空闲页数没有严格递增；
- Shell 提示符、输出或 RAMFS 行为不符合预期；
- `sendkey` 交互超时。

## 手动运行

进入交互式 QEMU：

```bash
docker build --tag tinyshell-os-dev:toolchain-v1 .
docker run --rm -it \
  --volume "$PWD:/workspace" \
  --workdir /workspace \
  tinyshell-os-dev:toolchain-v1 make run
```

在当前串口与 monitor 共用标准输入输出的模式下，按 `Ctrl+A`，再按 `X` 退出 QEMU。
