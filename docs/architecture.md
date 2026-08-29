# TinyShell OS 架构

## 项目定位

TinyShell OS 是一个面向操作系统课程设计的 i386 教学型微内核原型。内核实现单内核地址空间、页映射、中断、调度和 IPC 等基础机制，并在 Ring 0 中提供可交互 Shell 与静态 RAMFS，用于演示完整运行链路。

当前实现只有一个内核地址空间。Shell 和 RAMFS 不是用户态服务；系统也没有 TSS、Ring 3、系统调用或用户态隔离。

## 模块划分

| 模块 | 目录 | 主要职责 |
|---|---|---|
| 启动与 CPU 基础 | `boot/`、`kernel/arch/x86/` | Multiboot、GDT、IDT、异常入口、上下文切换 |
| Console | `kernel/console/`、`kernel/serial.c` | VGA 文本和 COM1 串口输出 |
| 内存管理 | `kernel/boot/`、`kernel/mm/` | 内存图、物理页、分页和启动堆 |
| 中断与设备 | `kernel/arch/x86/` | PIC、IRQ、PIT 和 PS/2 键盘 |
| 任务与 IPC | `kernel/task/`、`kernel/ipc/` | 协作任务、轮转调度和固定消息队列 |
| Shell 与 RAMFS | `kernel/shell/`、`kernel/fs/` | 行编辑、命令解析、命令执行和内存文件 |
| 系统诊断 | `kernel/diag/` | 只读运行状态快照 |

## 启动流程

```text
GRUB (Multiboot EAX=0x2BADB002, EBX=info)
  → console_init
  → gdt_init / idt_init / int3 回归
  → 校验 Multiboot memory map
  → PMM 导入 usable 区并保留低端、内核和启动数据
  → PIC / PIT / keyboard 初始化
  → paging / VMM / heap / IPC 自检
  → RAMFS / line editor / parser 自检
  → sti
  → 等待真实 IRQ0
  → producer / consumer / timer-observer 任务流
  → SYSTEM_STATUS_OK / SHELL_READY / BOOT_OK
  → TinyShell 前台循环
```

任一受检初始化步骤失败都会输出 `BOOT_FAIL:<stage>`，随后执行 `cli; hlt`。`BOOT_OK` 只在全部初始化和自检完成后输出一次。

## 内存管理

### Multiboot 与 PMM

Multiboot 层先完整校验 memory map，再把每个区域交给 PMM。PMM 以 4 KiB 为单位管理物理页，并显式保留：

- 0–1 MiB 低端内存；
- 内核镜像和内核栈；
- Multiboot info 与 memory map 缓冲区；
- 固件标记为不可用的区域。

PMM 采用 reserved-wins 语义。首次分配后，`add` 和 `reserve` 操作会被拒绝，防止活跃分配期间改变页的归属。

### 分页与 VMM

VMM 使用非 PAE、4 KiB 页的单内核地址空间：

- 前 128 MiB identity-map，第 0 页保持 non-present；
- CR0.PG 和 CR0.WP 均开启；
- `0x40000000–0xF0000000` 用作动态映射窗口；
- 32 张静态页表负责 identity map，另 32 张用于动态映射；
- `vmm_unmap_page()` 只解除映射，物理页由所有者调用 PMM 释放。

Page fault handler 输出 CR2、error code 和访问类型后停机，不执行恢复。

### 启动堆

Heap 使用 BSS 中 256 KiB 静态 arena，返回地址按 16 字节对齐。分配器采用 first-fit，支持块拆分和前后相邻块合并。它不向 PMM 扩容，也不保证并发安全，因此不能在 IRQ handler 中调用。

## 中断与设备

IDT 包含 256 项。向量 0–31 对应 CPU 异常，硬件 IRQ 经 PIC 重映射到向量 32–47。IRQ dispatcher 统一完成三件事：记录次数、调用已注册 handler、发送 EOI。

PIT 配置为 100 Hz，IRQ0 handler 只增加 tick。PS/2 IRQ1 handler 每次最多读取一个扫描码，完成 Set 1 解码后把字符放入 64 字节队列。中断上下文不打印、不解析命令，也不访问 RAMFS。

## 任务与 IPC

任务系统最多保存 8 个内核任务，每个任务使用 16 KiB 静态栈。上下文切换保存 ESP、EBP、EBX、ESI、EDI 和 EFLAGS。调度器使用协作式 round-robin：任务只在 `yield`、`exit` 或入口返回时切换；IRQ 不触发抢占。

IPC 提供 8 个静态 endpoint。每个 endpoint 是深度为 8 的 FIFO，单条 payload 最大 32 字节。发送执行深拷贝；队列满时拒绝新消息，不覆盖旧消息。接口非阻塞，也不会自动 `yield`。

## Shell、RAMFS 与状态

Shell 的输入路径为：

```text
PS/2 IRQ1 → keyboard queue → keyboard_pop_char
          → line editor → parser → command runtime
          → RAMFS / system status
```

字符出队、命令解析和文件操作都在前台普通内核上下文完成。行编辑器支持 Enter 和 Backspace；parser 使用固定数组，不依赖 libc 或动态内存。

RAMFS 使用 16 个静态文件槽，每个文件最多 512 字节。它支持创建、覆盖、追加、读取、删除和槽位复用，但不支持目录、持久化或并发访问。

`status` 通过公开接口读取 PMM、Heap、PIT、IRQ0、键盘和任务统计。快照先写入局部对象，全部读取成功后再更新调用者输出。

## 关键约束

- 内核使用 freestanding C11，不依赖宿主机 libc。
- 硬件相关代码集中在 x86 架构目录。
- 启动阶段的内存和设备初始化在 `sti` 前完成。
- IRQ handler 只做有界、无阻塞的工作；dispatcher 统一发送 EOI。
- 页表、Heap、任务栈和 IPC 队列使用固定容量静态存储。
- 协作任务依赖主动让出 CPU；IPC 不阻塞，也不触发调度。
- Shell 与 RAMFS 运行在 Ring 0，不具备用户态故障隔离。

## 未实现功能

- TSS、Ring 3、系统调用和用户地址空间；
- 抢占调度、优先级、睡眠和多核同步；
- 阻塞 IPC、endpoint 销毁和访问控制；
- 磁盘驱动、持久化文件系统和目录；
- Shell 管道、重定向、引号、历史、补全和外部程序。
