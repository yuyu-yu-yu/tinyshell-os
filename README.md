# TinyShell OS

基于 x86（i386）的教学型微内核操作系统课程设计。

当前基线已完成四轮。内核仍由 GRUB Multiboot 加载 32 位镜像，并初始化 Console、GDT、IDT、BIOS memory map、PMM、PIC、PIT、PS/2 键盘、分页、启动堆、协作任务和拷贝式 IPC。第四轮在 Ring 0 接入可交互 TinyShell：真实 IRQ1 字符进入有界队列，前台主循环做行编辑、命令解析，并操作静态 RAMFS。这是教学型微内核原型，Shell 与 RAMFS 尚未放到用户态。

## 统一开发环境

Docker 是本项目的标准构建和验收环境。仓库中的 `Dockerfile` 固定 Ubuntu 24.04 基础镜像，并安装 GCC 13、32 位编译支持、GRUB 2、QEMU 8、binutils、GDB、NASM 和 xorriso。

组员的宿主机可以是 Windows、macOS 或 Linux，但提交前必须通过 Docker 测试。宿主机直接执行 `make test` 只用于快速开发，不能代替容器验收。

## 快速开始

Windows PowerShell：

```powershell
powershell -ExecutionPolicy Bypass -File tools/docker-test.ps1
```

Linux、macOS 或 WSL：

```bash
bash tools/docker-test.sh
```

脚本会构建 `tinyshell-os-dev:toolchain-v1` 镜像，在容器内编译内核、生成 ISO，先跑 16/64/128 MiB 启动矩阵，再跑 64 MiB 真实 QEMU `sendkey` 交互测试。

VS Code 用户可以安装 Dev Containers 扩展，然后选择 **Dev Containers: Reopen in Container**。容器创建后会自动检查工具链并运行测试。

启动交互式 QEMU：

```bash
docker build --tag tinyshell-os-dev:toolchain-v1 .
docker run --rm -it \
  --volume "$PWD:/workspace" \
  --workdir /workspace \
  tinyshell-os-dev:toolchain-v1 make run
```

在 `-nographic` 模式下，按 `Ctrl+A`，再按 `X` 退出 QEMU。

## 当前验收标准

Docker 测试应检查：

1. 内核是有效的 x86 Multiboot 镜像。
2. 能生成 `build/tinyshell.iso`。
3. QEMU 分别以 16、64、128 MiB 启动，串口日志包含原有启动标记以及 `RAMFS_OK`、`SHELL_INPUT_OK`、`SHELL_PARSE_OK`、`SYSTEM_STATUS_OK`、`SHELL_READY`，并且 `BOOT_OK` 只出现一次。
4. 三档 `PMM_FREE_PAGES` 严格递增，证明 PMM 使用了 GRUB 提供的真实内存图。
5. 日志不含 `BOOT_FAIL:` 或 `PAGE_FAULT`。
6. 64 MiB 下 `tools/qemu-shell-test.py` 通过 QEMU monitor `sendkey` 走真实 IRQ1，完成 help、Backspace、touch/write/cat/append/ls/rm、两次 `status` 和 `about`，并输出 `QEMU shell interaction: PASS`。不得用直接调用 parser 或 keyboard handler 代替该测试。

## 目录结构

```text
boot/       x86 启动入口与 Multiboot 头
config/     GRUB 配置
docs/       架构与开发约定
include/    内核公共头文件
kernel/     内核 C 代码
tools/      环境检查和辅助脚本
.devcontainer/  VS Code 容器配置
.github/    云端 Docker 构建与测试
build/      生成物，不提交 Git
```

后续模块按 `docs/architecture.md` 中的边界逐步加入。

## 小组并行开发

给 A、B、C、D 四名成员及其 AI Agent 的完整任务书见：

- [`docs/agent-brief-cycle-01.md`](docs/agent-brief-cycle-01.md)
- [`docs/agent-brief-cycle-02.md`](docs/agent-brief-cycle-02.md)：2026-08-21 至 2026-08-22 加速轮次
- [`docs/agent-brief-cycle-03.md`](docs/agent-brief-cycle-03.md)：2026-08-25 至 2026-08-26 分页、启动堆、协作任务与 IPC
- [`docs/cycle-03-integration.md`](docs/cycle-03-integration.md)：第三轮实际合并、启动路径与测试证据
- [`docs/cycle-04-integration.md`](docs/cycle-04-integration.md)：第四轮 Ring 0 Shell / RAMFS 集成记录
