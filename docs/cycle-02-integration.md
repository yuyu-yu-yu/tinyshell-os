# 第二轮集成约定

## 成员 A 的接口

成员 A 在 `feature/cycle-02-a-boot-map` 上扩展第一轮 Multiboot v1 解析器。三个公开入口都接收 bootloader magic 和信息结构地址；它们不会保存 GRUB 指针，也不会分配内存。

```c
bool multiboot_parse(
    uint32_t magic,
    uint32_t info_address,
    struct boot_memory_summary *summary
);

bool multiboot_for_each_memory_region(
    uint32_t magic,
    uint32_t info_address,
    boot_memory_region_visitor visitor,
    void *context
);

bool multiboot_get_owned_ranges(
    uint32_t magic,
    uint32_t info_address,
    struct boot_owned_ranges *ranges
);
```

解析器先完整验证 memory-map buffer，再按原顺序调用 visitor。每项输出保持 64 位 `base` 和 `length`；扩展项仍按 `size + 4` 定位。visitor 返回 `false` 时立即停止并向调用者返回失败。传给 visitor 的 region 指针只在当次调用期间有效。

所有输出参数都先写入局部对象，验证成功后才复制给调用者。无效 magic、缺少 flags、空 buffer、截断项、异常步长、零长度区域和地址溢出均返回 `false`，且不修改输出。

`boot_owned_ranges` 使用半开区间。`info_length` 当前固定为 52 字节，表示 TinyShell 实际读取的 Multiboot v1 信息前缀；它不代表命令行、模块、符号表等全部可选数据。`mmap_length` 是 GRUB 提供的完整 memory-map buffer 长度。后续若读取其他 Multiboot 字段，必须增加相应保留范围。

`linker.ld` 导出 `__kernel_start` 和 `__kernel_end`。前者位于 `.text` 前并覆盖 Multiboot header，后者位于 `.bss` 后并覆盖启动栈。它们是地址符号，不是普通 C 变量；集成代码应声明为数组并转换为 `uintptr_t`：

```c
extern const uint8_t __kernel_start[];
extern const uint8_t __kernel_end[];
```

## 合并顺序和回滚点

A 从最新 `main` 建立 `integration/cycle-02`，按以下顺序集成：

1. A：Multiboot visitor、owned ranges 和 linker symbols；
2. B：物理页分配器；
3. C：PIC、IRQ stubs 和 dispatcher；
4. D：PIT 和键盘；
5. A：`kernel_main` 接线、Makefile 标记和本文档的最终结果。

每次只合并一个角色，并立即运行 Docker/QEMU。失败时回滚当前未通过的合并，不把下一模块带入排查。A 的解析器可在没有 B、C、D 的情况下独立构建，因此第一步不得引入它们的头文件。

## PMM 接线

第二天由 A 在 `kernel_main` 中按以下顺序初始化：

```text
pmm_reset
遍历 memory map，把每个 type == 1 的区域交给 pmm_add_usable_region
保留 [0, 1 MiB)
保留 [__kernel_start, __kernel_end)
保留 Multiboot info 前缀和 mmap buffer
执行一次单页 alloc/free 自检
```

visitor 把 `type == 1` 项交给 `pmm_add_usable_region()`，把其他所有类型交给 `pmm_reserve_region()`。B 的 reserved-wins 状态使可用与保留描述即使重叠，最终也不会把固件保留页加入分配器。任何解析或 PMM 操作失败都停止启动。区域保持原始 64 位范围，4 GiB 限制和页对齐由 PMM 统一处理。

## IRQ、PIT 和键盘接线

PMM 自检成功后，A 才执行：

```text
irq_init
pit_configure(100)
keyboard_init
执行 synthetic keyboard decode 自检
再次 keyboard_init，清空测试字符队列、modifier 状态和 dropped count
注册 IRQ 0 和 IRQ 1 handler
只解除 IRQ 0 和 IRQ 1 的 mask
sti
用 hlt 等待至少三个真实 PIT tick
进入 hlt + keyboard_pop_char + Console 输出主循环
```

synthetic decode 必须在 IRQ1 解屏蔽和 `sti` 之前完成，避免测试入口与真实键盘 IRQ 同时写 ring buffer。自检后再次调用 `keyboard_init()`，确保测试字符、Shift/Caps 状态和 dropped count 不进入真实输入阶段。`sti` 必须晚于 IDT/PIC 初始化、handler 注册和 mask 设置。等待 tick 使用无符号差值以支持计数回绕。IRQ handler 不打印、不阻塞、不自行发送 EOI；EOI 由 C 的 IRQ dispatcher 统一完成。

## 集成验收

第一步合入 A 后，原第一轮日志必须完整保留。最终集成还要检查 PMM、PIC、IRQ、PIT、真实 timer IRQ、synthetic keyboard 和 `BOOT_OK` 标记。Docker/QEMU 分别以 16、64、128 MiB 运行；三种配置都必须完成 Multiboot 解析、PMM alloc/free 和真实 PIT tick。

提交前运行 `git diff --check`、staged 安全扫描和 Docker 测试。`build/`、kernel map、ISO、QEMU 日志与临时 synthetic harness 都是本地产物，不得进入提交。

## 实际合并与验收记录

`integration/cycle-02` 保留了四个成员的原提交，并按 A `1bf8431` → B `8691883` → C `ef5eda1` → D `625e618` 合并。每个合并后都在标准 Docker 镜像中重新编译 ISO 并启动 QEMU。

最终启动路径保持 IF=0，完成 Multiboot、PMM、PIC、PIT 和 synthetic keyboard 自检后，只解屏 IRQ0/1，再执行 `sti`。`TIMER_IRQ_OK` 同时要求 PIT tick 和 IRQ0 dispatcher count 前进三次，因此不能通过直接调用 handler 伪造。启动成功后，主循环使用 `hlt` 等待中断并回显键盘队列。

2026-08-25 本地 Docker/QEMU 矩阵结果：

| QEMU 内存 | `PMM_FREE_PAGES` | 结果 |
|---:|---:|---|
| 16 MiB | 3735 | 通过 |
| 64 MiB | 16023 | 通过 |
| 128 MiB | 32407 | 通过 |

三档数值严格递增，且每份日志只有一条 `PMM_FREE_PAGES`、不含 `BOOT_FAIL`。额外的 64 MiB 交互测试在 `KEYBOARD_READY` 之后通过 QEMU monitor 发送 `sendkey a`，串口日志末尾收到了单个 `a`，证明回显经过了真实 IRQ1。

当前已知限制是 PIC 尚未处理 spurious IRQ7/15，也不会在解屏 slave IRQ 时自动解屏 cascade IRQ2；本轮只使用 master IRQ0/1，因此不影响当前验收。
