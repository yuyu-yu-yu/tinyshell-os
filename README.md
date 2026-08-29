# TinyShell OS

TinyShell OS 是一个基于 x86（i386）的教学型微内核原型。系统由 GRUB Multiboot v1 加载，使用 freestanding C11 和少量汇编实现启动、中断、内存、任务、IPC，以及可交互的 Ring 0 Shell。

> 当前 Shell 与 RAMFS 仍运行在 Ring 0。项目尚未实现 TSS、Ring 3、系统调用和用户态隔离，因此不宣称是完整微内核。

## 功能概览

- **启动与 CPU 基础：** VGA/COM1 Console、平坦 GDT、256 项 IDT、32 个 CPU 异常入口和真实 `int3` 返回测试。
- **内存管理：** Multiboot memory map、4 KiB 物理页分配器、非 PAE 分页、前 128 MiB identity map、动态页映射和 256 KiB 启动堆。
- **中断与设备：** 8259 PIC、IRQ dispatcher、100 Hz PIT、PS/2 Set 1 键盘解码和 64 字节输入队列。
- **任务与 IPC：** 最多 8 个协作式内核任务、round-robin 调度、静态 endpoint 和非阻塞深拷贝 FIFO。
- **Shell 与 RAMFS：** 行编辑、固定命令解析、系统状态查询，以及 16 个文件、每个 512 字节的静态内存文件系统。
- **自动验收：** Docker 内完成构建，QEMU 以 16/64/128 MiB 启动，并通过真实 monitor `sendkey` 验证 IRQ1 到 Shell 的完整路径。

## 运行链路

```text
GRUB Multiboot
  → Console / GDT / IDT / CPU exception
  → memory map / PMM
  → PIC / PIT / keyboard
  → paging / heap
  → tasks / IPC
  → SHELL_READY / BOOT_OK
  → 进入 TinyShell 提示符 tiny>
```

键盘输入不会在 IRQ1 中直接执行命令：

```text
PS/2 IRQ1 → keyboard queue → foreground loop
          → line editor → parser → RAMFS / status
```

## 快速验收

Docker 是标准构建和测试环境。Dockerfile 使用固定摘要的 Ubuntu 24.04 基础镜像；环境检查要求 GCC 13.x、QEMU 8.x 和 GRUB 2.12，并安装 32 位编译支持、binutils、GDB、NASM 和 xorriso。

Windows PowerShell：

```powershell
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
```

Linux、macOS 或 WSL：

```bash
bash tools/docker-test.sh
```

完整测试会生成 Multiboot ISO，运行三档 QEMU 启动测试，再执行 64 MiB 真实键盘交互测试。当前基线结果为：

```text
QEMU 16M boot test: PASS
QEMU 64M boot test: PASS
QEMU 128M boot test: PASS
PMM memory scaling: 3565 < 15853 < 32237
QEMU boot matrix: PASS
QEMU shell interaction: PASS
```

测试方法和判定条件见 [`docs/testing.md`](docs/testing.md)。

## 交互运行

```bash
docker build --tag tinyshell-os-dev:toolchain-v1 .
docker run --rm -it \
  --volume "$PWD:/workspace" \
  --workdir /workspace \
  tinyshell-os-dev:toolchain-v1 make run
```

在当前串口与 monitor 共用标准输入输出的模式下，按 `Ctrl+A`，再按 `X` 退出 QEMU。

TinyShell 提供以下命令：

| 命令 | 作用 |
|---|---|
| `help`、`about` | 查看命令和系统边界 |
| `clear`、`echo` | 清屏和输出文本 |
| `ls`、`touch`、`cat` | 查看、创建和读取 RAMFS 文件 |
| `write`、`append`、`rm` | 覆盖、追加和删除文件 |
| `status` | 查看 PMM、Heap、PIT、键盘和任务状态 |

## 项目结构

```text
boot/           x86 启动入口、中断桩和上下文切换
config/         GRUB 配置
include/        公共接口
kernel/arch/x86/ x86 架构与设备代码
kernel/mm/      PMM、VMM 与 Heap
kernel/task/    协作式任务调度
kernel/ipc/     固定消息 IPC
kernel/shell/   行编辑、命令解析与 Runtime
kernel/fs/      静态 RAMFS
kernel/diag/    系统状态快照
tools/          Docker 和 QEMU 验收脚本
docs/           架构与测试说明
```

## 文档

- [`docs/architecture.md`](docs/architecture.md)：启动流程、模块职责和设计约束。
- [`docs/testing.md`](docs/testing.md)：Docker/QEMU 测试流程、结果和失败条件。

## 已知限制

- Shell 和 RAMFS 运行在 Ring 0，没有用户态隔离。
- 调度器是协作式的，没有抢占、优先级或睡眠。
- IPC 非阻塞，endpoint 不销毁；当前实现面向单核协作模型。
- Heap、页表、任务栈和 IPC 队列使用固定容量静态存储。
- RAMFS 不持久化，不支持目录；Shell 不支持管道、重定向、引号、历史或外部程序。
- Page fault 会输出诊断并停机，不执行恢复。
