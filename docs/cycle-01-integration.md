# 第一轮集成记录

## 成员 A 当前接口

成员 A 在 `feature/cycle-01-a-multiboot` 分支实现 Multiboot v1 信息解析。公开接口为：

```c
bool multiboot_parse(
    uint32_t magic,
    uint32_t info_address,
    struct boot_memory_summary *summary
);
```

解析器只读取 GRUB 提供的信息，不保存启动结构指针，也不分配物理页。它要求 basic memory 和 BIOS memory map 两个 flags 均有效，并拒绝地址溢出、截断项、异常步长和零长度内存区域。结构布局依据 [GNU Multiboot Specification 0.6.96](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)。启动入口在调用 C 前清除方向标志、保持 16 字节调用边界，并在返回后回收参数与对齐空间。

## 第二天建议合并顺序

在最新 `main` 上建立 `integration/cycle-01`，依次审查并合并：

1. D：统一 Console；
2. B：GDT；
3. C：IDT 与 CPU 异常；
4. A：Multiboot 信息解析；
5. A：统一调整 `kernel_main` 和 `Makefile` 验收标记。

每次合并后都运行 Docker 测试。某一步失败时，只修复当前模块与公共接口的冲突，不顺带改动下一模块。

## 预期公共接口冲突

- D 合并后，应删除 `kernel/kernel.c` 内部的 VGA 实现，改用 `console_init()` 和 `console_write()`；Multiboot 成功与失败日志都通过 Console 输出。
- B 不修改 `kernel_main`。A 在 Console 初始化后调用 `gdt_init()`，成功后输出 `GDT_OK`。
- C 可以继续使用内核代码段选择子 `0x08`。A 在 GDT 后调用 `idt_init()`，再执行 `int3` 冒烟测试。
- A 的 `multiboot_parse()` 必须在启动信息仍可直接访问时调用。第一轮尚未启用分页并采用平坦段，因此当前信任 GRUB 提供的物理地址并直接转换为指针；魔数本身不能证明任意地址都可访问。后续建立非恒等映射前必须重新确认这一假设。
- 内存映射中的 `type == 1` 才表示未来页分配器可使用的 RAM。当前模块只校验并计数，其他或未知类型一律不得当作可用内存。
- 只有 A 修改 `Makefile` 的最终日志检查，B、C、D 不应各自加入互相依赖的标记。

最终初始化顺序为：

```text
console_init
gdt_init
idt_init
multiboot_parse
int3 smoke test
BOOT_OK
```

## 第一轮验收

集成后的串口日志至少应包含：

```text
CONSOLE_OK
GDT_OK
IDT_OK
MULTIBOOT_OK
MEMORY_MAP_OK
EXCEPTION vector=3
INT3_TEST_OK
BOOT_OK
```

提交前运行对应宿主机的 Docker 脚本，并确认 GitHub Actions 的 Docker CI 通过。

## 实际集成结果

第一轮按 D → B → C → A 的顺序完成集成。C 的公共中断入口在合并时补充了 `cld` 和 i386 SysV 16 字节 call-site 栈对齐，随后才接入真实 `int3` 测试；A 的启动入口也保持相同 ABI 约束。D 的正确分支只包含 Console 头文件与实现，未带入旧远端分支中的异常临时文件。

Docker 构建、GRUB 校验和无图形 QEMU 测试通过，实际串口日志为：

```text
TinyShell OS booting...
Architecture: i386
CONSOLE_OK
GDT_OK
IDT_OK
MULTIBOOT_OK
MEMORY_MAP_OK
EXCEPTION vector=3
INT3_TEST_OK
BOOT_OK
```

这证明第一轮模块不是只通过静态编译：新 GDT 和 IDT 已在同一启动路径中加载，异常确实进入 C dispatcher，并通过 `iret` 返回到内核继续输出 `BOOT_OK`。
