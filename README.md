# TinyShell OS

基于 x86（i386）的教学型微内核操作系统课程设计。

当前里程碑是可重复的最小启动基线：GRUB Multiboot 加载 32 位内核，内核初始化串口和 VGA 文本输出，并在 QEMU 中打印启动信息。

## 开发环境

- Windows + WSL2 Ubuntu 24.04
- GCC 13（`-m32 -ffreestanding`）
- GNU binutils / GRUB 2
- QEMU 8
- GNU Make / GDB / NASM / xorriso

本机已经创建 WSL 用户 `tinyos` 并安装上述依赖。Windows 项目目录在：

```text
C:\Users\11719\Desktop\简历\OS课程项目
```

对应的 WSL 路径是：

```text
/mnt/c/Users/11719/Desktop/简历/OS课程项目
```

## 快速开始

打开 PowerShell：

```powershell
wsl -d Ubuntu-24.04
cd '/mnt/c/Users/11719/Desktop/简历/OS课程项目'
bash tools/check-env.sh
make
make test
```

启动交互式 QEMU：

```bash
make run
```

在 `-nographic` 模式下，按 `Ctrl+A`，再按 `X` 退出 QEMU。

## 当前验收标准

`make test` 应检查：

1. 内核是有效的 x86 Multiboot 镜像。
2. 能生成 `build/tinyshell.iso`。
3. QEMU 串口日志包含 `TinyShell OS booting` 和 `BOOT_OK`。

## 目录结构

```text
boot/       x86 启动入口与 Multiboot 头
config/     GRUB 配置
docs/       架构与开发约定
include/    内核公共头文件
kernel/     内核 C 代码
tools/      环境检查和辅助脚本
build/      生成物，不提交 Git
```

后续模块按 `docs/architecture.md` 中的边界逐步加入。

## 小组并行开发

第一轮给 A、B、C、D 四名成员及其 AI Agent 的完整任务书见：

- [`docs/agent-brief-cycle-01.md`](docs/agent-brief-cycle-01.md)
