# TinyShell OS

[![Docker CI](https://github.com/yuyu-yu-yu/tinyshell-os/actions/workflows/docker-ci.yml/badge.svg)](https://github.com/yuyu-yu-yu/tinyshell-os/actions/workflows/docker-ci.yml)

TinyShell OS 是一个基于 x86（i386）的教学型微内核原型。系统由 GRUB Multiboot v1 加载，使用 freestanding C11 与少量 i386 汇编实现，打通了启动、内存管理、中断处理、任务调度、消息通信和 TinyShell 交互的完整运行链路。

项目强调操作系统机制的**可解释性、可观察性和可复现性**：核心模块拥有清晰接口与启动自检，一条命令即可在统一 Docker 环境中完成构建、三档内存启动和键盘中断交互测试。

## 项目亮点

- **完整运行闭环：** 从 GRUB 加载、CPU 初始化和内存运行时，到任务、IPC、RAMFS 与 TinyShell，各模块共同支撑一个可启动、可操作的内核系统。
- **清晰模块边界：** 启动、架构、内存、任务、IPC、文件系统和 Shell 分目录组织，通过 `include/` 中的公共接口连接，便于阅读、调试和讲解。
- **硬件中断路径：** PIT 通过 IRQ0 驱动 100 Hz 时钟；PS/2 控制器通过 IRQ1 通知内核读取 Set 1 扫描码，解码后的字符经过有界队列进入前台 Shell。
- **分层内存运行时：** Multiboot 内存图、4 KiB 物理页、非 PAE 分页、动态页映射和 256 KiB Kernel Heap 构成清晰的内存管理层次。
- **运行状态可观察：** VGA 与 COM1 同步输出启动信息，`status` 命令展示 PMM、Heap、PIT、IRQ0、键盘和任务状态。
- **端到端自动验收：** Docker 和 GitHub Actions 使用同一构建入口；QEMU 三档内存测试和 monitor `sendkey` 覆盖真实启动与交互链路。

## 系统架构

```text
┌────────────────────────────────────────────┐
│ TinyShell / RAMFS / System Status          │  交互与状态
├────────────────────────────────────────────┤
│ Cooperative Tasks / Scheduler / IPC        │  内核机制
├──────────────────────┬─────────────────────┤
│ PMM / VMM / Heap     │ PIC / PIT / PS/2    │  内存与设备
├──────────────────────┴─────────────────────┤
│ Console / GDT / IDT / Exceptions           │  CPU 基础
├────────────────────────────────────────────┤
│ GRUB Multiboot / QEMU i386                 │  启动环境
└────────────────────────────────────────────┘
```

| 层级 | 主要模块 | 作用 |
|---|---|---|
| 启动环境 | GRUB、Multiboot、QEMU i386 | 加载 32 位内核并传递启动信息 |
| CPU 基础 | Console、GDT、IDT、Exceptions | 建立执行环境、输出通道和异常入口 |
| 内存与设备 | PMM、VMM、Heap、PIC、PIT、PS/2 | 管理内存、分页映射、时钟和键盘 |
| 内核机制 | Tasks、Scheduler、IPC | 运行协作式任务并传递深拷贝消息 |
| 交互与状态 | TinyShell、RAMFS、System Status | 接收命令、操作内存文件并展示运行状态 |

### 启动链路

```text
GRUB Multiboot
  → Console / GDT / IDT / int3
  → Multiboot memory map / PMM
  → PIC / PIT / PS/2 keyboard / IRQ
  → VMM / Heap / Tasks / IPC / Shell 自检
  → sti / 真实 IRQ0
  → producer / consumer / timer-observer 任务流
  → SYSTEM_STATUS_OK / SHELL_READY / BOOT_OK
  → TinyShell 前台循环 tiny>
```

### 键盘输入链路

```text
QEMU 键盘事件
  → PIC IRQ1
  → PS/2 Set 1 解码
  → 64 字节字符队列
  → 前台行编辑器
  → 命令解析器
  → RAMFS / status
```

IRQ1 只完成扫描码读取、字符解码和入队；命令解析与文件操作由前台 Runtime 执行。这个分层让中断处理保持短小，也让 Shell 逻辑能够独立测试。

### 任务与 IPC 链路

```text
round-robin scheduler
  ├─ producer → ipc_send → endpoint FIFO
  ├─ consumer ← ipc_receive ←───────────┘
  └─ timer-observer → hlt → IRQ0 → yield
```

调度器最多运行 8 个内核任务，每个任务使用 16 KiB 栈。IPC 提供 8 个 endpoint，每个 FIFO 深度为 8，单条 payload 最大 32 字节；发送时执行深拷贝，消息所有权清晰。

## 运行方式

### 1. 获取项目

```bash
git clone https://github.com/yuyu-yu-yu/tinyshell-os.git
cd tinyshell-os
```

### 2. 一键构建与完整验收

Windows PowerShell：

```powershell
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
```

Linux 或 WSL：

```bash
bash tools/docker-test.sh
```

脚本会自动构建统一工具链镜像、编译 i386 内核、生成 `build/tinyshell.iso`、运行 16/64/128 MiB 三档启动测试，并通过 QEMU monitor `sendkey` 检查从 IRQ1 到 TinyShell 的真实输入链路。

### 3. 查看内核启动日志

```text
docker build --tag tinyshell-os-dev:toolchain-v1 .
docker run --rm -it --volume "${PWD}:/workspace" --workdir /workspace tinyshell-os-dev:toolchain-v1 make run
```

串口会依次输出模块启动标记、PMM 空闲页数、`SYSTEM_STATUS_OK`、`BOOT_OK` 和 TinyShell 提示符。按 `Ctrl+A`，再按 `X` 退出 QEMU。

### 4. 图形窗口手动交互

完成一键验收后，`build/tinyshell.iso` 可直接交给宿主机 QEMU。宿主机安装 QEMU 后运行：

```text
qemu-system-i386 -cdrom build/tinyshell.iso -m 64M -serial stdio -no-reboot
```

启动后点击 QEMU 图形窗口，使键盘焦点进入虚拟机；随后输入命令即可操作 TinyShell。

## TinyShell 演示

下面这组命令同时覆盖系统状态、行编辑、命令解析和 RAMFS 文件生命周期，适合作为现场演示流程：

```text
tiny> status
tiny> touch demo
tiny> write demo hello TinyShell
tiny> append demo from RAMFS
tiny> cat demo
tiny> ls
tiny> rm demo
tiny> status
tiny> about
```

| 命令 | 作用 |
|---|---|
| `help`、`about` | 查看命令列表和系统信息 |
| `clear`、`echo` | 清屏和输出文本 |
| `ls`、`touch`、`cat` | 查看、创建和读取 RAMFS 文件 |
| `write`、`append`、`rm` | 覆盖、追加和删除文件 |
| `status` | 查看 PMM、Heap、PIT、键盘和任务状态 |

## 自动验收结果

| 验证项 | 方法 | 证明内容 |
|---|---|---|
| Multiboot 内核 | `grub-file --is-x86-multiboot` | 内核 ELF 包含合法 Multiboot header，可由 GRUB 识别 |
| CPU 异常 | 真实 `int3` | 异常入口、dispatcher 和返回路径有效 |
| 时钟中断 | 真实 PIT IRQ0 | IDT、PIC、IRQ dispatcher 和 PIT 链路有效 |
| 内存扩展 | 16/64/128 MiB 启动矩阵 | PMM 使用 GRUB 提供的实际 memory map |
| 键盘交互 | QEMU monitor `sendkey` | IRQ1、键盘队列、行编辑、parser 和命令执行有效 |

当前基线输出：

```text
QEMU 16M boot test: PASS
QEMU 64M boot test: PASS
QEMU 128M boot test: PASS
PMM memory scaling: 3565 < 15853 < 32237
QEMU boot matrix: PASS
QEMU shell interaction: PASS
```

测试要求 28 个约定启动标记各出现一次，并检查 `BOOT_OK`、三档 PMM 页数和完整 Shell 交互。详细流程见 [`docs/testing.md`](docs/testing.md)。

## 核心参数

| 子系统 | 参数 |
|---|---|
| GDT / IDT | 5 项 GDT、256 项 IDT、32 个 CPU 异常入口 |
| PMM / VMM | 4 KiB 页、前 128 MiB identity map、动态映射窗口 |
| PIT / 键盘 | 100 Hz PIT、PS/2 Set 1、64 字节输入队列 |
| Heap | 256 KiB、16 字节对齐、first-fit 与相邻块合并 |
| Tasks | 最多 8 个任务、每任务 16 KiB 栈、round-robin |
| IPC | 8 个 endpoint、队列深度 8、payload 最大 32 字节 |
| Shell / RAMFS | 127 字符命令行、16 个文件、单文件最大 512 字节 |

## 项目结构

```text
boot/            x86 启动入口、中断桩和上下文切换
config/          GRUB 配置
include/         公共接口
kernel/arch/x86/ x86 架构与设备代码
kernel/mm/       PMM、VMM 与 Heap
kernel/task/     协作式任务调度
kernel/ipc/      固定消息 IPC
kernel/shell/    行编辑、命令解析与 Runtime
kernel/fs/       内存 RAMFS
kernel/diag/     系统状态快照
tools/           Docker 和 QEMU 验收脚本
docs/            架构与测试说明
```

## 进一步阅读

- [`docs/architecture.md`](docs/architecture.md)：启动流程、模块职责和设计约束。
- [`docs/testing.md`](docs/testing.md)：Docker/QEMU 测试流程、结果和验收判定。
