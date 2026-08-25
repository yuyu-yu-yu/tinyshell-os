# TinyShell OS 答辩 PPT 逐页大纲（16:9）

> 内部准备项。课程 PPT 可选；若最终不提交，从压缩包删除对应文件，不留空文件。
> 第四轮未合入功能在幻灯片上必须标「待集成 / 计划演示」，不能写成已上线。
> 建议字体：标题 32–36 pt，正文 18–20 pt，代码 14 pt。深色标题条 + 浅底正文。

## 第 1 页 · 封面

- 版式：居中标题，底部一行仓库 URL 与组号空位。
- 标题：TinyShell OS
- 副标题：i386 教学型微内核原型
- 右下角：组号 / 组长姓名（现场手填或排版时替换）
- 不要出现 “完整微内核操作系统” 或 “用户态 Shell 已完成”。

## 第 2 页 · 问题与边界

- 左栏：要解决什么——从 GRUB 到可演示的内存/中断/任务/IPC。
- 右栏：明确不做——TSS、Ring 3、系统调用、磁盘 FS、抢占。
- 底部金句（原样）：当前是微内核**原型**；Shell/RAMFS 若演示成功，仍在 Ring 0。

## 第 3 页 · 架构总图

- 中央竖条「内核」：CPU 异常、IRQ、PMM/VMM/Heap、Task、IPC。
- 右侧虚线框「第四轮前台（待集成）」：键盘队列 → 行编辑 → parser → RAMFS / status。
- 箭头标注：IRQ1 只入队，命令不在中断里跑。

## 第 4 页 · 启动路径

- 时间轴：`console → GDT/IDT → mmap → PMM → PIC/PIT/KBD → paging → heap → IPC → sti → IRQ0 ×3 → tasks → BOOT_OK`。
- 强调 `BOOT_OK` 只出现一次，且必须在全部自检之后。
- 图表：可直接用报告第 4 节文本图。

## 第 5 页 · 中断与真实硬件

- 左：IDT gate（selector `0x08`、offset、type 0x8E）。
- 右：IRQ0 PIT tick；IRQ1 读一个 scancode。
- 脚注：第三轮已用真实 `int3` 与真实 IRQ0；键盘 `sendkey a` 曾在 64 MiB 下回显 `a`。

## 第 6 页 · 内存三层

- 三列：PMM（4 KiB 页，冻结归属） / VMM（identity 128 MiB，CR0.WP） / Heap（256 KiB first-fit）。
- 底部数字：16/64/128 MiB 的 `PMM_FREE_PAGES` = 3569 < 15857 < 32241（第三轮证据）。

## 第 7 页 · 协作任务与 IPC

- 左：8 任务、yield/exit 才切换、IRQ 不调度。
- 右：静态 endpoint、深拷贝、满队列失败、不阻塞。
- 小图：producer → FIFO → consumer，timer-observer 用 `hlt+yield` 等 IRQ0。

## 第 8 页 · Shell 与 RAMFS（待集成）

- 演示路径：`tiny> ` → touch/write/cat/append/ls/rm。
- 约束：16 文件、512 字节、非法名拒绝、超长写入不破坏旧内容。
- 角标大红字：待 `integration/cycle-04`。

## 第 9 页 · 测试策略

- 上层：启动 marker 矩阵（证明引导与模块自检）。
- 下层：QEMU `sendkey`（证明 IRQ1→队列→前台，而不是测试直接调 handler）。
- 说明 monitor Unix socket 在容器 `/tmp`，不在 Windows bind 的 `build/`。

## 第 10 页 · 测试证据

- 表：三档内存 PASS + CR0=80010011。
- 空行：第四轮十二步交互结果，Day 1 填「未跑通 / 待集成」。
- 失败样例：`BOOT_FAIL:shell-runtime`、脚本打印阶段名。

## 第 11 页 · 限制与后续

- 五条限制：Ring 0 Shell、无系统调用、无抢占、heap 不扩容、page fault 停机。
- 后续：老师要求且时间允许，才从冻结 tag 做 TSS/Ring 3。

## 第 12 页 · 结束页

- 仓库 URL、Docker 一条命令、Q&A。
- 备用：60–90 秒录像清单见 `demo-script.md`。
