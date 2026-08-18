# TinyShell OS 第一轮 Agent 任务书

> 适用日期：2026-08-19 至 2026-08-20  
> 仓库：<https://github.com/yuyu-yu-yu/tinyshell-os>  
> 第一轮节奏：第 1 天独立开发，第 2 天审查、集成和验收

## 给组员的使用方法

组长把本文件发给四名成员，并分别指定 A、B、C、D。每名成员把本文件完整发送给自己的 AI Agent，再发送下面这句话：

```text
我是 TinyShell OS 项目的成员 X，X 是 A、B、C、D 中分配给我的角色。请完整阅读任务书，检查当前仓库状态，只完成角色 X 的第一轮任务。现在开始工作，不要等待我逐步指挥。
```

把 `X` 换成自己的角色。Agent 必须先检查仓库，再开始修改，不得只根据聊天中的代码片段工作。

## 项目现状

TinyShell OS 是一个 i386 教学型微内核课程项目。仓库当前已经具备：

- GRUB Multiboot 启动入口；
- freestanding C11 编译和 32 位链接；
- COM1 串口输出；
- VGA 文本输出；
- 可启动 ISO；
- QEMU 自动启动测试；
- `BOOT_OK` 串口验收标记。

当前尚未实现 GDT、IDT、分页、调度、用户态、IPC、文件系统或 Shell。Agent 不得把计划功能描述成已完成功能。

## Docker 是强制验收环境

所有成员和 Agent 必须使用仓库根目录的 `Dockerfile`。宿主机可以使用任何编辑器，也可以运行本地编译器做快速检查，但每次提交和 PR 前必须通过 Docker 测试。

禁止为了通过构建而在宿主机或容器中临时安装未写入 `Dockerfile` 的包。若确实缺少依赖，应在 PR 中说明，由成员 A 统一修改 Dockerfile。

## 所有 Agent 的执行顺序

### 1. 获取并检查仓库

如果本地没有仓库：

```bash
git clone https://github.com/yuyu-yu-yu/tinyshell-os.git
cd tinyshell-os
```

如果已经克隆：

```bash
git switch main
git pull --ff-only
```

确认 Docker Desktop 或 Docker Engine 已启动：

```bash
git status --short --branch
docker info
```

Windows PowerShell 运行：

```powershell
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
```

Linux、macOS 或 WSL 运行：

```bash
bash tools/docker-test.sh
```

第一次运行需要构建镜像，耗时可能较长。开始修改前，`git status` 必须干净，Docker 测试必须显示：

```text
QEMU boot test: PASS
```

如果测试失败，先保存完整日志并判断是 Docker 启动问题、镜像构建问题还是项目测试问题。不得绕过 Docker 后声称环境验证通过。

### 2. 阅读项目约定

Agent 必须阅读：

```text
README.md
docs/architecture.md
docs/development.md
Makefile
linker.ld
boot/multiboot.S
kernel/kernel.c
kernel/serial.c
include/io.h
```

### 3. 创建角色分支

各角色使用固定分支：

```text
A: feature/cycle-01-a-multiboot
B: feature/cycle-01-b-gdt
C: feature/cycle-01-c-idt
D: feature/cycle-01-d-console
```

命令示例：

```bash
git switch -c feature/cycle-01-x-name
```

不要直接修改或推送 `main`。不要使用 `git push --force`、`git reset --hard` 或 `git clean -fd`。

### 4. 实现、测试和提交

每个 Agent 只能修改自己任务中列出的文件。若发现必须修改公共文件，先在 PR 说明中提出，由成员 A 在第二天集成。

完成后再次运行与宿主机对应的 Docker 脚本：

```text
Windows: tools/docker-test.ps1
Linux/macOS/WSL: tools/docker-test.sh
```

随后运行 `git diff --check` 和 `git status --short`，然后只暂存本角色文件，提交并推送角色分支。禁止提交 `build/`、ISO、日志、编辑器配置、凭据或环境文件。

推荐提交信息：

```text
A: boot: parse Multiboot memory information
B: x86: add global descriptor table
C: x86: add interrupt descriptor table
D: console: add shared VGA and serial output
```

推送后创建 Draft PR，目标分支为 `main`。PR 不得自行合并。

## 公共技术约束

- 使用 i386、freestanding C11 和少量 GNU 汇编。
- Dockerfile 是工具链的唯一来源；不得在个人分支改用另一套基础镜像或编译器。
- 使用 `stdint.h`、`stddef.h` 等 freestanding 头文件；不得依赖宿主机 libc。
- 不使用 C++、动态分配、线程库、异常或标准输入输出库。
- C 代码必须通过现有 `-Wall -Wextra -Werror`。
- 汇编入口必须保持栈平衡，并在需要时遵守 32 位 cdecl 调用约定。
- 硬件结构体必须明确处理对齐和 `packed` 布局。
- 标识符使用英文；解释性文档和必要注释可以使用中文。
- 外部代码必须记录来源和许可证。不得整段复制教学内核或把未知代码冒充组员原创。
- AI 应先说明数据结构、不变量和失败路径，再修改代码。
- 每个成员必须理解并能答辩解释自己提交的每一项核心逻辑。

## 文件所有权

第一天只有成员 A 可以修改：

```text
kernel/kernel.c
Makefile
docs/cycle-01-integration.md
```

B、C、D 不得修改这些公共文件。Makefile 会自动发现 `boot/` 和 `kernel/` 下新增的 `.S`、`.c` 文件，因此各模块无需手动登记源文件。

如果不同角色需要同名公共类型，先在自己的模块内使用最小前置声明，并在 PR 中记录整合需求。不要跨角色修改文件来“顺手解决”。

## 成员 A：Multiboot 信息与集成负责人

### 允许修改

```text
include/boot/multiboot.h
kernel/boot/multiboot.c
kernel/kernel.c
docs/cycle-01-integration.md
```

如确有必要，A 可以在集成时修改 `Makefile`，但当前自动发现源码的机制通常不需要修改。

### 第一天任务

1. 定义项目实际使用的 Multiboot v1 信息结构和内存映射项。
2. 用启动时的 `EAX` 校验 `0x2BADB002` 魔数。
3. 检查 Multiboot flags，再读取 `mem_lower`、`mem_upper` 和 memory map。
4. 安全遍历变长 memory-map 项，拒绝越界、零长度或异常步长。
5. 提供只读查询接口，不实现物理页分配。
6. 在 `kernel_main` 中调用模块，并保留原有 `BOOT_OK`。
7. 为第二天记录 B、C、D 的建议合并顺序和公共接口冲突。

### 建议接口

```c
struct boot_memory_summary {
    uint32_t lower_kib;
    uint32_t upper_kib;
    uint32_t mmap_entry_count;
};

bool multiboot_parse(
    uint32_t magic,
    uint32_t info_address,
    struct boot_memory_summary *summary
);
```

可以调整类型名，但接口必须保持只读、边界明确，并能区分成功与失败。

### 验收

串口日志至少包含：

```text
MULTIBOOT_OK
MEMORY_MAP_OK
BOOT_OK
```

A 必须能解释：

- 为什么 GRUB 通过 `EAX` 和 `EBX` 传递启动信息；
- Multiboot flags 为什么必须先检查；
- memory-map 项为什么不能按固定数组直接递增；
- 当前为什么能直接访问物理地址；
- 该模块与后续物理页分配器的边界。

## 成员 B：GDT 负责人

### 允许修改

```text
include/arch/x86/gdt.h
kernel/arch/x86/gdt.c
boot/gdt_load.S
```

### 第一天任务

1. 定义 packed GDT entry 和 GDTR 描述符。
2. 创建五个描述符：null、Ring 0 code、Ring 0 data、Ring 3 code、Ring 3 data。
3. 实现设置描述符字段的内部函数。
4. 实现 `gdt_init()`。
5. 在汇编中执行 `lgdt`，重载数据段寄存器，并用远跳转重载 `CS`。
6. 暂不实现 TSS、Ring 3 切换或任务门。

### 固定选择子

```text
0x00: null
0x08: kernel code
0x10: kernel data
0x1B: user code，包含 RPL=3
0x23: user data，包含 RPL=3
```

### 必须提供

```c
void gdt_init(void);
```

B 不得调用控制台接口，也不得修改 `kernel_main`。成员 A 在第二天调用 `gdt_init()` 并加入 `GDT_OK` 日志。

### 验收

按“实现、测试和提交”一节运行与宿主机对应的 Docker 脚本，然后运行 `git diff --check`。

B 必须能解释：

- GDT entry 的 base、limit、access、granularity；
- `0x08`、`0x10`、`0x1B`、`0x23` 的计算；
- `lgdt` 后为什么还要重载段寄存器；
- 为什么重载 `CS` 需要远跳转；
- 为什么第一天不实现 TSS。

## 成员 C：IDT 与 CPU 异常负责人

### 允许修改

```text
include/arch/x86/idt.h
include/arch/x86/interrupt.h
kernel/arch/x86/idt.c
kernel/arch/x86/interrupt.c
boot/idt_load.S
boot/isr_stubs.S
```

### 第一天任务

1. 定义 256 项 packed IDT 和 IDTR 描述符。
2. 实现 `idt_set_gate()` 和 `idt_init()`。
3. 为 CPU 异常 0 至 31 提供汇编 stub。
4. 正确区分 CPU 自动压入 error code 的异常与没有 error code 的异常。
5. 公共汇编入口保存通用寄存器，调用 C 层 `interrupt_dispatch()`，恢复现场后执行 `iret`。
6. 第一轮只安装异常门，不配置 PIC、PIT、键盘或硬件 IRQ。
7. 让向量 3 的 `int3` 能返回原执行流；其他未处理异常可以记录后停止 CPU。

### 必须提供

```c
struct interrupt_frame;

void idt_init(void);
void interrupt_dispatch(struct interrupt_frame *frame);
```

成员 C 可以引用固定内核代码段选择子 `0x08`，但不能调用尚未合并的 GDT 或 Console 模块。需要输出时先保留清楚的 TODO，由成员 A 第二天接入。

### 验收

第一天必须保证新增汇编和 C 文件可以在 Docker 中通过完整链接。第二天由 A 在 GDT 初始化后调用 `idt_init()` 和 `int3`，目标日志为：

```text
IDT_OK
EXCEPTION vector=3
INT3_TEST_OK
```

C 必须能解释：

- IDT gate 中 selector、offset 和 type attributes；
- interrupt gate 与 trap gate 的区别；
- 哪些异常由 CPU 自动压入 error code；
- 汇编 stub 为什么要统一栈布局；
- `iret` 前为什么必须恢复正确的栈。

## 成员 D：统一控制台负责人

### 允许修改

```text
include/console.h
kernel/console/console.c
```

只有发现原串口接口存在明确缺陷时，才允许同时修改：

```text
include/serial.h
kernel/serial.c
```

此时必须在 PR 中单独解释原因。

### 第一天任务

1. 把 VGA 文本输出封装为独立控制台模块，不直接修改 `kernel.c`。
2. `console_write()` 同时写入 VGA 和 COM1 串口。
3. 支持 `\n`、`\r`、`\b` 和可打印 ASCII。
4. 到达屏幕底部时向上滚动一行，不得直接回到左上角覆盖内容。
5. 提供十六进制和无符号十进制 `uint32_t` 输出，供启动信息和异常处理使用。
6. 不实现完整 `printf`、格式字符串、键盘输入、Shell 或颜色命令。

### 必须提供

```c
void console_init(void);
void console_clear(void);
void console_putc(char value);
void console_write(const char *text);
void console_write_u32_hex(uint32_t value);
void console_write_u32_dec(uint32_t value);
```

D 不得删除 `kernel.c` 中的旧 VGA 代码。成员 A 在第二天改用新接口并删除旧实现。

### 验收

第二天集成后的目标日志为：

```text
CONSOLE_OK
```

D 必须能解释：

- VGA 文本缓冲区为什么位于 `0xB8000`；
- 一个 VGA 字符单元的字符与颜色布局；
- COM1 发送前为什么检查 line status register；
- 屏幕滚动时如何避免越界访问；
- 为什么现在实现数字输出而不是完整 `printf`。

## 第二天集成顺序

成员 A 建立 `integration/cycle-01` 分支，按以下顺序审查和集成：

1. D：Console；
2. B：GDT；
3. C：IDT 和异常；
4. A：Multiboot 信息解析；
5. A：统一修改 `kernel_main`、测试标记和必要的构建配置。

最终启动顺序应为：

```text
console_init
gdt_init
idt_init
multiboot_parse
int3 smoke test
BOOT_OK
```

最终串口至少包含：

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

集成完成后必须运行 Docker 测试：

```text
Windows: tools/docker-test.ps1
Linux/macOS/WSL: tools/docker-test.sh
```

随后运行 `git diff --check`。容器内的 `make test` 也应扩展为检查本轮新增标记，只有成员 A 修改该测试。

## Agent 完成后必须返回的报告

每个 Agent 在结束时向成员返回：

```text
角色：
分支：
提交哈希：
修改文件：
实现内容：
测试命令与结果：
Docker 镜像 ID：
GitHub Actions 状态：
已知限制：
AI 使用说明：
需要 A 在第二天处理的集成事项：
```

Agent 还要列出五个本成员必须能回答的答辩问题及简短答案。若测试失败，Agent 必须保留真实失败信息，不得把失败描述为完成。
