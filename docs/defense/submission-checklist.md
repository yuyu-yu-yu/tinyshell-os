# 第四轮 / 答辩提交检查表

负责人：成员 D 汇总，组长最终勾选。未发生的事项保持未勾选，不得提前打勾。

## Day 1（本分支）

- [x] 从最新 `origin/main`（`5bbbda9`）建立 `feature/cycle-04-d-defense-qa`
- [x] 未修改 `kernel/kernel.c`、Makefile、Dockerfile、linker、CI、他人文件
- [x] 未合并 `main`
- [x] 实现 `include/diag/system_status.h`、`kernel/diag/system_status.c`
- [x] 实现 `tools/qemu-shell-test.py`（结构完整；不伪称交互 PASS）
- [x] 答辩初稿五件套位于 `docs/defense/`
- [ ] 基线 Docker `tools/docker-test.ps1` 在本机执行（当前 Windows 环境无 Docker CLI / Docker Desktop，见 PR 说明）
- [x] `python3 -B` 语法检查 `qemu-shell-test.py`（PASS）
- [x] 临时 mock 测试（仅 `build/`，不提交；宿主 gcc 64 位 PASS；另用 `-m32 -Werror` 编译 `system_status.c` PASS）
- [x] `git diff --check`、逐文件 `git add`、安全扫描脚本不存在，已人工阅读 `git diff --cached`（无密钥/环境文件/生成物）
- [ ] 推送分支并创建 base=`main` 的 Draft PR
- [ ] A/B/C 的一页说明与五问五答已收齐（Day 1 结束前他们提交）

## Day 2（禁止在未收到「进入 Day 2」前执行）

- [ ] 从最新 `integration/cycle-04` 建 `docs/cycle-04-d-finalize`
- [ ] 只改 D 的 Day 2 文件：Makefile、README、architecture、答辩材料
- [ ] Makefile 保留 16/64/128 MiB，并在末尾跑 64 MiB sendkey 测试
- [ ] 交互测试输出 `QEMU shell interaction: PASS`
- [ ] 仅把通过测试的功能改为「已完成」
- [ ] finalize PR base 为 `integration/cycle-04`，D 不合并 `main`

## 代码冻结后（8 月 28–29 日，仓库外）

- [ ] 组号、组长姓名、全体学号由本人填写
- [ ] Markdown 排版为项目文档 PDF，并目视检查
- [ ] 按大纲制作 PPTX（若课程最终不交 PPT，则从压缩包删除，不留空文件）
- [ ] `源码托管链接.txt`：仓库 URL、最终提交或 tag、Docker 命令
- [ ] 压缩包结构：

```text
[组号]组[组长姓名]操作系统课程设计.zip
├─ [组号]组TinyShell OS项目文档.pdf
├─ 源码托管链接.txt
└─ [组号]组TinyShell OS答辩PPT.pptx   （可选）
```

- [ ] 压缩包不含源码、`.git`、`build/`、Docker 镜像、临时日志、密钥
- [ ] 目录建在仓库外，不 `git add`

## 答辩前 ≥24 小时（组员本人）

- [ ] 确认自选题与分组已获老师认可
- [ ] 核对 AI 使用与课程 A 级代码量口径
- [ ] 四人都能讲解本人模块的正常路径、两个失败边界、一个限制
- [ ] 确认老师公布的实际答辩时间（若早于 8 月 31 日则提前发件）
- [ ] 组长发邮件到 `wang.box@163.com`
- [ ] 标题：`[组号]组[组长姓名]操作系统课程设计答辩`
- [ ] 组长 QQ 私信王老师确认已提交
- [ ] 准备现场陈述；备用 60–90 秒录像在仓库外

## 邮件与口径

- 收件人：`wang.box@163.com`
- 必须使用的口头边界：TinyShell OS 是具备内存、中断、任务和 IPC 的教学型微内核**原型**；Shell/RAMFS 在 Ring 0，尚未用户态隔离。
- 截止日期：课程要求 2026-08-31 答辩；材料按「答辩前 24 小时」倒推。
