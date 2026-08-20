# TinyShell OS

基于 x86（i386）的教学型微内核操作系统课程设计。

当前里程碑是可重复的第一轮内核基线：GRUB Multiboot 加载 32 位内核，内核初始化统一 Console、GDT 和 IDT，解析 BIOS memory map，并用真实 `int3` 验证异常入口能够返回。

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

脚本会构建 `tinyshell-os-dev:toolchain-v1` 镜像，在容器内编译内核、生成 ISO，并用无图形 QEMU 完成启动测试。

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
3. QEMU 串口日志包含 Console、GDT、IDT、Multiboot memory map、`int3` 与 `BOOT_OK` 的成功标记。

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
