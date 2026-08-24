# TinyShell OS 架构边界

## 目标

TinyShell OS 是一个教学型 i386 微内核。最终系统把调度、地址空间、异常中断和 IPC 留在内核，把 Shell 与文件系统语义放到用户态服务。

## 计划模块

| 模块 | 目录 | 责任 |
|---|---|---|
| x86 启动与硬件抽象 | `boot/`、`kernel/arch/x86/` | Multiboot、GDT、IDT、PIC、PIT、键盘 |
| 内存管理 | `kernel/mm/` | 物理页、分页、用户地址空间 |
| 任务与调度 | `kernel/task/` | 任务状态、上下文切换、轮转调度 |
| 系统调用与 IPC | `kernel/ipc/` | 用户态入口、消息发送与接收 |
| 文件服务 | `servers/fs/` | 可写 RAMFS 和文件协议 |
| Shell | `user/shell/` | 命令解析和文件服务客户端 |

## 当前里程碑

前两轮已实现统一 Console、平坦 GDT、32 个 CPU 异常入口、IDT、Multiboot v1 memory map、单页 PMM、PIC/IRQ、100 Hz PIT 和 PS/2 Set 1 键盘解码。内核用真实 `int3` 检查异常返回，用真实 IRQ0 检查中断路径，并在启动后回显 IRQ1 收到的字符。

第三轮已实现非 PAE、4 KiB 页的单内核地址空间，启用 CR0.PG 和 CR0.WP，并 identity-map 前 128 MiB。动态窗口可映射 PMM 页，256 KiB 静态堆提供 first-fit 分配和相邻块合并。协作式 round-robin 调度器可运行 8 个内核任务，静态 endpoint 为任务提供非阻塞、深拷贝 FIFO IPC。

Ring 3、TSS、系统调用、抢占调度、阻塞 IPC、用户地址空间和文件系统仍未实现。文档和答辩只能把通过自动测试的功能描述成已完成功能。

## 设计约束

- 内核代码使用 freestanding C11，不依赖宿主机 libc。
- 硬件相关代码集中在 x86 架构目录。
- 公共接口先写头文件，再分别实现与测试。
- 每个新模块都要有串口自测输出，最终由 `make test` 自动验证。
- 引用外部代码必须记录来源与许可证；核心算法必须由组员理解并能答辩。
- PMM 在首次分配后冻结物理页归属；当前启动阶段的内存和设备初始化都在 `sti` 前完成。
- IRQ handler 只做有界的无阻塞工作；IRQ dispatcher 统一记数并发送 EOI。
- VMM 只负责虚拟映射，`vmm_unmap_page()` 不释放物理页；物理页所有者负责调用 PMM 释放。
- 启动堆、任务栈、页表和 IPC 队列仍使用静态 BSS，优先保持本轮实现的可解释性和确定性。
- 协作任务只在主动 yield 或 exit 时切换；IRQ 不执行调度，IPC 也不会自动阻塞或 yield。
