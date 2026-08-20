# TinyShell OS 第二轮 Agent 任务书

> 适用日期：2026-08-21 至 2026-08-22
>
> 轮次：第 3–4 天
>
> 仓库：<https://github.com/yuyu-yu-yu/tinyshell-os>
>
> 总目标：加入物理页管理、8259A IRQ、PIT 时钟和 PS/2 键盘输入

## 给四名成员的使用方法

组长继续指定 A、B、C、D。每名成员把本文件完整发送给自己的 AI Agent，再发送：

```text
我是 TinyShell OS 项目的成员 X，X 是 A、B、C、D 中分配给我的角色。
请完整阅读 docs/agent-brief-cycle-02.md，检查 GitHub 和当前仓库，只完成角色 X 的第二轮任务。
严格遵守文件所有权、Docker 验收、分支和报告要求。现在开始工作，不要等待我逐步指挥。
```

把 `X` 替换成自己的角色。Agent 必须操作完整仓库，不能只根据聊天中粘贴的代码工作。

## 为什么第二轮做这些内容

课程说明中，A级项目要求独立完成简单操作系统，覆盖引导程序、核心代码、文件系统、控制台等，并要求项目组完成至少一半代码。当前仓库已有启动、GDT、IDT、异常和 Console，但还没有可管理的内存、硬件中断、时钟或输入。

本轮完成后，TinyShell OS 应第一次具备持续运行的硬件中断主线：

```text
Multiboot BIOS memory map
              |
              v
     physical page allocator

IDT -> 8259A PIC -> IRQ0 -> PIT ticks
                 -> IRQ1 -> PS/2 keyboard -> character queue -> Console echo
```

本轮仍不实现分页、堆、调度、用户态、系统调用、IPC、文件系统或真正的用户 Shell。文档和答辩中不得把计划功能写成已完成功能。

## 开始前必须满足的基线

所有分支必须从第二轮开始时最新的 `main` 创建。该 `main` 应在 Docker/QEMU 中依次打印：

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

获取基线：

```bash
git switch main
git pull --ff-only
git status --short --branch
```

Windows PowerShell：

```powershell
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
```

Linux、macOS 或 WSL：

```bash
bash tools/docker-test.sh
```

看到 `QEMU boot test: PASS` 后才能创建角色分支。基线失败时先报告，不得在错误基线上继续堆代码。

## 两天加速节奏

### 第一天：并行开发

1. 前 1 小时：阅读接口、确认不变量和失败路径，运行基线测试；
2. 接下来 4–5 小时：只实现本角色模块，每完成一个小接口就编译；
3. 最后 1–2 小时：Docker 验收、边界测试、安全扫描、提交、推送 Draft PR；
4. 当天结束前：把完整报告和五个答辩问答发到组内，不得只回复“已完成”。

### 第二天：审查与集成

1. A 建立 `integration/cycle-02`；
2. B、C、D 各自先审查另一人的 PR；
3. A 按规定顺序合并，每合一个模块立即运行 Docker；
4. 四人共同处理接口冲突，但只有文件所有者提交对应修复；
5. 完成真实 PIT IRQ 测试、合成键盘扫描码测试和至少一次手动键盘演示；
6. 最终 CI 绿灯后才允许进入 `main`。

第二天 B、C、D 不是等待状态：B 负责核对 PMM 页数，C 负责检查 IRQ/PIC 计数与 EOI，D 负责 PIT 频率、扫描码和真实键盘演示，A 负责公共文件和最终提交。

## 固定分支

```text
A: feature/cycle-02-a-boot-map
B: feature/cycle-02-b-pmm
C: feature/cycle-02-c-irq-pic
D: feature/cycle-02-d-pit-keyboard
```

不要改用 `member-x/...` 等临时名称。禁止直接推送 `main`，禁止 `git push --force`、`git reset --hard` 和 `git clean -fd`。

## Docker 和 Git 统一要求

- `Dockerfile` 是唯一验收环境，不得靠宿主机临时安装依赖绕过构建。
- 每个角色分支必须独立通过 Docker；“合并后应该能编译”不算完成。
- Makefile 会自动发现 `boot/`、`kernel/` 下新增的 C 和汇编文件，不要为了登记源文件修改 Makefile。
- 只暂存任务书允许的路径，不使用不加检查的 `git add .`。
- 提交前运行 `git diff --check`、安全扫描和 Docker 测试。
- 不提交 `build/`、ISO、QEMU 日志、临时测试程序、编辑器配置、凭据或 `%USERPROFILE%` 等本机路径。
- 推送后建立 Draft PR，目标分支为 `main`；第一天不得自行合并。

推荐提交信息：

```text
A: boot: expose Multiboot memory regions
B: mm: add physical page allocator
C: x86: add PIC and IRQ dispatch
D: x86: add PIT and keyboard input
```

## 公共技术约束

- 目标仍为 i386、freestanding C11 和 GNU 汇编，不依赖 libc。
- IRQ/异常入口必须执行 `cld`，并在调用 C 前满足 16 字节调用边界。
- IRQ handler 中禁止 Console 输出、动态分配和长循环；只更新状态或入队。
- `sti` 只能由 A 在第二天完成 IDT、PIC、handler 注册和解屏蔽之后执行。
- 所有共享计数器按无符号回绕设计，不假设 tick 永远不溢出。
- 所有地址加法、页对齐、memory-map 步长和队列索引都必须防溢出。
- 外部参考必须记录来源与许可证；不得复制无法解释的教学内核代码。
- 每个成员必须逐行理解自己提交的核心逻辑，并能独立解释失败路径。

## 文件所有权总表

| 角色 | 第一天独占范围 | 第二天责任 |
|---|---|---|
| A | Multiboot region API、linker 符号、集成文档 | `kernel_main`、Makefile、最终集成 |
| B | `kernel/mm/` 与 `include/mm/` | PMM 页数与分配/释放自测 |
| C | IRQ、PIC、IRQ stubs、interrupt 扩展 | IRQ 计数、屏蔽、EOI 与异常回归 |
| D | PIT、PS/2 keyboard、字符环形队列 | 时钟频率、扫描码、真实键盘演示 |

第一天只有 A 可以创建 `docs/cycle-02-integration.md`。第一天所有人都不得修改 `kernel/kernel.c` 或 `Makefile`；这两个文件由 A 在第二天独占修改。

---

## 成员 A：启动内存图接口与集成负责人

### 第一天允许修改

```text
include/boot/multiboot.h
kernel/boot/multiboot.c
linker.ld
docs/cycle-02-integration.md
```

### 第一天任务

1. 在第一轮解析器上增加只读 memory-region 遍历接口，不重复实现一套松散检查。
2. 每个回调项提供 `base`、`length` 和 `type`，保持 64 位地址与长度。
3. 复用并保留第一轮对 flags、buffer 地址、`size + 4`、截断项和溢出的检查。
4. 提供 Multiboot info 结构和 memory-map buffer 的占用范围，供第二天从 PMM 中保留。
5. 在 linker script 导出 `__kernel_start` 和 `__kernel_end`，范围必须覆盖代码、只读数据、数据和 BSS。
6. 写出 A/B/C/D 的建议合并顺序、公共接口和回滚点。
7. 第一天不调用 B、C、D 的接口，不修改 `kernel_main` 或 Makefile。

### 建议接口

```c
struct boot_memory_region {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

typedef bool (*boot_memory_region_visitor)(
    const struct boot_memory_region *region,
    void *context
);

bool multiboot_for_each_memory_region(
    uint32_t info_address,
    boot_memory_region_visitor visitor,
    void *context
);

struct boot_owned_ranges {
    uint32_t info_address;
    uint32_t info_length;
    uint32_t mmap_address;
    uint32_t mmap_length;
};

bool multiboot_get_owned_ranges(
    uint32_t info_address,
    struct boot_owned_ranges *ranges
);
```

可以调整命名，但必须保持只读、无动态分配、失败不返回半成品。visitor 返回 `false` 时遍历应立即停止并向调用者报告失败。

### 关键不变量

- 只把 flags 已确认有效的字段交给外部。
- memory-map 项的 `size` 可以大于 20，下一项仍按 `size + 4` 定位。
- 不长期保存 GRUB 指针；分页布局改变前必须完成所需信息复制或转换。
- `type == 1` 才是可用 RAM，其他和未知类型一律视为不可分配。
- 物理地址能在当前阶段直接访问，是因为尚未启用分页并使用平坦段；魔数本身不能证明任意地址安全。

### 第二天 A 独占文件

```text
kernel/kernel.c
Makefile
docs/cycle-02-integration.md
```

A 负责把所有模块接线、加入日志标记和自动测试。B、C、D 不得为方便测试而抢改这些公共文件。

### 第一天验收

- 新 API 可独立编译链接；
- 现有第一轮全部 QEMU 标记不退化；
- 至少在临时测试中验证 visitor 提前停止和合法扩展 `size > 20`；
- linker map 中能找到 `__kernel_start`、`__kernel_end`，且 start < end；
- Docker 与 `git diff --check` 通过。

### A 必须能回答

1. 为什么只有 memory-map 的 `type == 1` 能交给 PMM？
2. 为什么可用区向内对齐，而保留区向外对齐？
3. 为什么必须保留低端内存、内核镜像和 Multiboot 数据？
4. 为什么不能长期保存 GRUB memory-map 指针？
5. linker symbols 与普通 C 变量有什么区别？

---

## 成员 B：物理页分配器负责人

### 独占文件

```text
include/mm/pmm.h
kernel/mm/pmm.c
```

B 不包含 Multiboot 头文件，不解析 GRUB 结构，也不修改 linker script。A 第二天通过固定接口逐段把可用区交给 PMM。

### 建议接口

```c
enum {
    PMM_PAGE_SIZE = 4096,
};

void pmm_reset(void);
bool pmm_add_usable_region(uint64_t base, uint64_t length);
bool pmm_reserve_region(uint64_t base, uint64_t length);
uintptr_t pmm_alloc_page(void);
bool pmm_free_page(uintptr_t physical_address);
uint32_t pmm_free_page_count(void);
uint32_t pmm_total_page_count(void);
```

### 第一天任务

1. 用静态 bitmap 管理完整 32 位物理地址空间：共 1,048,576 个 4 KiB 页。
2. 至少区分“可管理页”和“当前已分配页”，以便拒绝保留页和重复释放；两个 bitmap 总计约 256 KiB。
3. `pmm_reset()` 后所有页均不可用，页 0 永远不可分配。
4. `pmm_add_usable_region()` 只加入完全位于可用区内的整页，即起点向上、终点向下对齐。
5. `pmm_reserve_region()` 保留所有与区间相交的页，即起点向下、终点向上对齐。
6. 对 `base + length` 做 64 位溢出检查，并把范围限制在 4 GiB 内。
7. `pmm_alloc_page()` 返回页对齐物理地址；无页可用时返回 0。
8. `pmm_free_page()` 拒绝地址 0、未对齐地址、越界地址、保留页和重复释放。
9. 不清零返回物理页，不解引用返回地址，不实现连续多页、分页或内核堆。

### 初始化顺序约束

第二天必须按以下顺序调用：

```text
pmm_reset
加入全部 type == 1 的可用区
保留 0–1 MiB
保留 [__kernel_start, __kernel_end)
保留 Multiboot info 和 memory-map buffer
开始分配
```

一旦开始分配，就不能再调用 `pmm_add_usable_region()` 改写所有权。

### B 独立边界测试

临时测试至少覆盖：

- 空状态分配失败；
- 跨页且未对齐的可用区只加入完整页；
- reserve 会覆盖部分相交页；
- 分配地址非零且 4 KiB 对齐；
- 分配后 free count 减 1；
- 释放后 free count 恢复；
- 重复释放失败；
- 释放未对齐地址失败；
- 接近 4 GiB 的范围不会整数回绕。

临时测试文件不得提交。

### 第二天标记

```text
PMM_OK
PMM_ALLOC_FREE_OK
```

### B 必须能回答

1. 两个 bitmap 分别表达什么状态？
2. 为什么页 0 必须永久保留？
3. 可用区与保留区为什么使用相反的对齐方向？
4. 单页扫描分配的时间复杂度是多少，后续如何优化？
5. 为什么 PMM 不负责清零页、建立页表或提供 `malloc`？

## 成员 C：IRQ 框架与 8259A PIC

### 允许修改的文件

```text
include/arch/x86/irq.h
include/arch/x86/pic.h
kernel/arch/x86/irq.c
kernel/arch/x86/pic.c
include/arch/x86/interrupt.h
kernel/arch/x86/interrupt.c
boot/irq_stubs.S
```

C 负责把第一轮的“CPU 异常”扩展为可注册的硬件中断框架。不要实现 PIT 或键盘业务逻辑；它们属于 D。

### 建议接口

```c
typedef void (*irq_handler_t)(void);

void irq_init(void);
bool irq_register_handler(uint8_t irq, irq_handler_t handler);
bool irq_unregister_handler(uint8_t irq);
bool irq_set_enabled(uint8_t irq, bool enabled);
uint32_t irq_count(uint8_t irq);
```

PIC 层至少提供初始化、设置单条 IRQ mask 和发送 EOI 的内部能力；端口读写函数应保持局部、简短并带清楚命名。

### 第一天任务

1. 把主/从 8259A PIC 重映射到向量 `32–47`，避免与 CPU 异常 `0–31` 冲突。
2. 初始化结束时先屏蔽全部 16 条 IRQ；C 不执行 `sti`，也不提前打开定时器或键盘中断。
3. 为 IRQ 0–15 提供汇编入口。每个入口压入统一的伪 error code 和 vector，再复用当前 `interrupt_common_entry`。
4. 保留第一轮异常路径和 `int3` 返回路径，不能让 IRQ 改造破坏 `EXCEPTION vector=3` / `INT3_TEST_OK`。
5. 在 C 分发器中识别向量 `32–47`，递增对应计数器，再调用注册的 handler。
6. handler 返回后发送 EOI；从 PIC 的 IRQ 8–15 必须先给 slave EOI，再给 master EOI。
7. 未注册 handler 的 IRQ 也必须安全返回并发送 EOI，不能因空指针崩溃。
8. 拒绝越界 IRQ、空 handler、重复注册和错误注销。
9. IRQ 热路径不要打印串口/VGA；串口输出可能拖慢中断、造成时序问题。
10. 第一轮修复过的入口规则必须保留：进入 C 前 `cld`，并满足 i386 SysV 16 字节 call-site 栈对齐；`iret` 前恢复原中断现场。

### 并发边界

本轮约定注册、注销和 mask 修改只在 `IF=0` 的初始化阶段调用。C 不需要现在实现通用自旋锁，但必须在接口注释里写明约束，不能假装并发安全。

### C 独立边界测试

临时测试至少覆盖：

- 编译产物包含 16 个 IRQ stub，并分别压入向量 32–47；
- PIC 初始化使用正确的 ICW 顺序和偏移量；
- 初始 mask 为 `0xFFFF`；
- 单独打开/关闭 IRQ 0、1、8、15 时 mask 位正确；
- 注册、重复注册、越界注册、注销不存在 handler 的返回值正确；
- 人工调用分发器后计数器只增加对应 IRQ；
- 从 PIC 路径的 EOI 顺序为 slave 后 master；
- 第一轮 `int3` 真实启动测试仍通过。

临时测试文件不得提交。

### 第二天标记

```text
PIC_OK
IRQ_OK
```

### C 必须能回答

1. 为什么 PIC 必须从默认向量重映射到 32–47？
2. mask、EOI 和 `sti` 分别控制什么？
3. 为什么 slave IRQ 要发送两次 EOI，且顺序不能反？
4. 为什么中断入口需要 `cld` 和 16 字节栈对齐？
5. 为什么未注册 handler 的 IRQ 也必须完成 EOI？

## 成员 D：PIT 定时器与 PS/2 键盘

### 允许修改的文件

```text
include/arch/x86/pit.h
include/arch/x86/keyboard.h
kernel/arch/x86/pit.c
kernel/arch/x86/keyboard.c
```

D 只实现设备逻辑，不修改 `irq.h`、IRQ 汇编入口、PIC、IDT、`kernel_main`、Makefile，也不自行发送 EOI 或执行 `sti`。第二天由 A 通过 C 的接口完成注册。

### PIT 建议接口

```c
bool pit_configure(uint32_t requested_hz);
void pit_handle_irq(void);
uint32_t pit_ticks(void);
uint32_t pit_frequency_hz(void);
```

### PIT 第一天任务

1. 使用 PIT 输入频率 `1,193,182 Hz`，配置 channel 0、lo/hi byte、mode 3。
2. 校验计算出的 divisor 位于 `1–65535`；拒绝 0 Hz 和无法表示的请求。
3. 向命令端口 `0x43` 写控制字，再向数据端口 `0x40` 依次写 divisor 低字节、高字节。
4. 保存 `actual_hz = 1193182 / divisor`，不要谎报请求值就是硬件实际值。
5. IRQ handler 只做无符号 tick 递增，不打印、不阻塞；tick 自然回绕是允许的。
6. 第二天集成频率统一为 100 Hz。

### 键盘建议接口

```c
void keyboard_init(void);
void keyboard_handle_irq(void);
bool keyboard_feed_scancode(uint8_t scancode);
bool keyboard_pop_char(char *character);
uint32_t keyboard_dropped_count(void);
```

### 键盘第一天任务

1. 支持 PS/2 Set 1 的字母、数字、空格、Enter、Backspace。
2. 正确维护左/右 Shift 和 Caps Lock；字母使用 `Shift XOR Caps Lock`，数字在 Shift 下映射常用符号。
3. 识别 break code，不把按键释放事件放入字符队列。
4. 对未知码和 `0xE0` 扩展前缀安全忽略；不能越界索引映射表。
5. 使用固定大小 ring buffer；队满时丢弃新字符并递增 dropped count，不覆盖未读字符。
6. `keyboard_feed_scancode()` 是纯逻辑入口，所有译码都走这里，方便不依赖真硬件的自动测试。
7. `keyboard_handle_irq()` 只检查状态端口 `0x64` 是否有数据，再从 `0x60` 读一个扫描码并交给纯逻辑入口。
8. handler 内不打印，也不自行 EOI；字符由内核主循环取出后再输出。

### D 独立边界测试

临时测试至少覆盖：

- PIT 对 0 Hz、极低频、极高频的拒绝或合法边界处理；
- 100 Hz divisor、端口写入顺序和实际频率正确；
- tick 连续递增并按无符号语义回绕；
- `a`、Shift+`a`、Caps+`a`、Shift+Caps+`a`；
- 数字和 Shift 符号；
- Enter、Backspace、break code、未知码与 `0xE0`；
- ring buffer 首尾回绕、FIFO 顺序、队满丢弃和 dropped count；
- `keyboard_pop_char(NULL)` 安全失败。

临时测试文件不得提交。

### 第二天标记

```text
PIT_OK
TIMER_IRQ_OK
KEYBOARD_DECODE_OK
KEYBOARD_READY
```

### D 必须能回答

1. PIT divisor 如何从目标频率计算，为什么实际频率会有误差？
2. 为什么 IRQ handler 不能直接打印大量字符？
3. make code、break code 和 `0xE0` 前缀分别是什么？
4. Shift 与 Caps Lock 对字母和数字的影响为什么不同？
5. ring buffer 如何区分空和满，满时为什么选择丢弃新字符？

## 第二天：A 负责集成，B/C/D 负责联调与修复

### 集成分支与顺序

A 从最新 `origin/main` 创建：

```text
integration/cycle-02
```

按以下顺序合并；每合并一项都单独跑 Docker，出现失败立即停下定位：

1. A 的 memory-map visitor 与 linker symbols；
2. B 的 PMM；
3. C 的 IRQ/PIC；
4. D 的 PIT/keyboard；
5. A 的 `kernel_main`、Makefile 标记检查和集成文档。

### 统一启动顺序

```text
console_init
gdt_init
idt_init
int3 回归测试
multiboot_parse
pmm_reset
遍历并加入全部 type == 1 的可用区
保留 0–1 MiB、内核镜像、Multiboot info 与 mmap buffer
PMM 单页 alloc/free 自检
irq_init（此时 PIC 仍全部屏蔽）
pit_configure(100)
keyboard_init
注册 IRQ 0 -> pit_handle_irq
注册 IRQ 1 -> keyboard_handle_irq
只解除 IRQ 0 和 IRQ 1 的 mask
sti
hlt 循环等待至少 3 个 tick
验证 irq_count(0) 与 pit_ticks 均前进
用 synthetic scancode 验证键盘译码
输出 BOOT_OK
进入 hlt + 取字符 + console 输出主循环
```

等待 tick 时必须使用无符号差值，才能跨越 `uint32_t` 回绕：

```c
while ((uint32_t)(pit_ticks() - start) < 3U) {
    __asm__ volatile ("hlt");
}
```

### 最终串口证据

至少出现以下标记，并由 Makefile / Docker 测试自动检查：

```text
CONSOLE_OK
GDT_OK
IDT_OK
MULTIBOOT_OK
MEMORY_MAP_OK
EXCEPTION vector=3
INT3_TEST_OK
PMM_OK
PMM_ALLOC_FREE_OK
PIC_OK
IRQ_OK
PIT_OK
TIMER_IRQ_OK
KEYBOARD_DECODE_OK
KEYBOARD_READY
BOOT_OK
```

`TIMER_IRQ_OK` 必须来自真实 QEMU PIT 中断，不得用直接调用 handler 伪造。`KEYBOARD_DECODE_OK` 可以用 synthetic scancode 自动验证；真实键盘输入可在本地 QEMU 用 `sendkey` 或交互窗口演示，但不要让 CI 依赖易抖动的图形输入。

### 必跑矩阵

```text
Docker build + QEMU: 16 MiB
Docker build + QEMU: 64 MiB
Docker build + QEMU: 128 MiB
git diff --check
staged safe_git_scan
```

三种内存配置下必须都能启动、解析 mmap、完成一次 PMM alloc/free，并收到真实 timer IRQ。记录 free page 数随内存规模增大而增大；不要求数值完全线性，因为 BIOS 保留区会不同。

## 禁止合并的情况

出现任意一项就先修复，不得用“我的电脑能跑”绕过：

- 分支不是从当前 `origin/main` 创建；
- 修改了别人的所有权文件却没有提前说明；
- 临时测试、构建产物、ISO、日志或密钥进入提交；
- 非 Docker 环境通过、Docker 环境失败；
- IRQ handler 中打印、阻塞、重复发送 EOI，或在设备模块里执行 `sti`；
- PMM 可能分配页 0、内核镜像、Multiboot 数据或 BIOS 低内存；
- 删除第一轮成功标记或破坏 `int3` 返回；
- 只有代码，没有边界测试、提交说明和答辩问答。

## 每位成员交付给 A 的固定格式

把下面模板填完整，不要只发一句“做完了”：

```text
成员：A / B / C / D
分支：feature/...
提交：<commit hash>
改动文件：
- ...

实现内容：
- ...

测试命令与结果：
- ... PASS / FAIL

边界测试：
- ...

已知限制：
- ...

我能解释的五个问题：
1. ...
2. ...
3. ...
4. ...
5. ...
```

## 两天结束时我们应该处在什么位置

完成本轮后，TinyShell 不再只是“能进 C 内核并打印”，而是有了可追踪物理页、真实硬件时钟中断和最小键盘输入链路。它仍是教学内核骨架，还不是完整微内核。后续 8–10 天依次推进：

1. 分页、内核堆与页故障诊断；
2. 内核线程、上下文切换与抢占式调度；
3. TSS、Ring 3、系统调用与用户程序；
4. 消息传递 IPC，把服务边界做成“微内核”核心证据；
5. RAMFS / initrd 和用户态 TinyShell；
6. 测试矩阵、架构图、课程报告、PPT 与答辩演示。

本题属于自选题，组长还应尽快向老师确认题目与范围；不要等到代码完成后才发现方向未获确认。课程评分同时看文档/源码、难度工作量和答辩，因此每位成员必须边实现边保留设计理由、测试证据和可讲解内容。
