# TinyShell OS 5–7 分钟演示脚本

目标时长 6 分钟。第一轮预演可看稿；第二轮合稿后随机抽问。第四轮交互步骤在合入前用「当前键盘回显」代替，并口头标明待集成。

## 0:00–0:40  开场与边界（不要超时）

口播：

> 我们做的是 i386 教学型微内核原型。已经有内存、中断、协作任务和拷贝式 IPC。Shell 和 RAMFS 即使今天能打字，也还在 Ring 0，不是用户态服务。没有 TSS，没有系统调用。

动作：打开 PPT 第 2 页，停 3 秒。不要念完所有没做的功能。

## 0:40–1:30  启动一次（Docker 或已准备的 QEMU）

动作：展示串口日志从头到 `BOOT_OK`。用鼠标指三个点：

1. `CONSOLE_OK` 到 `INT3_TEST_OK`：CPU 异常能返回。
2. `PMM_FREE_PAGES=...`：真实 memory map。
3. `TIMER_IRQ_OK` 与 `IPC_TASK_FLOW_OK`：真实 IRQ0 + 三任务。

口播一句：`BOOT_OK` 只出现一次，前面任何自检失败都会 `BOOT_FAIL` 停机。

若 Docker 现场过慢：改放预先保存的 `build/qemu-64M.log` 截图，并说明日志来自标准容器测试。

## 1:30–2:20  内存与分页

动作：PPT 第 6 页。口播：16/64/128 MiB 空闲页 3569 < 15857 < 32241；CR0 为 `80010011`，PG 和 WP 都开。Heap 只有 256 KiB，IRQ 里不分配。

## 2:20–3:00  任务与 IPC

动作：PPT 第 7 页。口播：三个内核任务，producer 发 6 条消息，consumer 检查 FIFO 和深拷贝，observer 等三次真实时钟。调度不抢占，不 yield 的任务会占住 CPU。

## 3:00–5:00  键盘与 Shell（分两种脚本）

**若第四轮尚未合入：**

动作：在 QEMU 里按几个字母，串口逐字回显。口播：这证明 IRQ1 已经进队列；行编辑和命令还在成员分支，待集成。

**若第四轮已合入且 `sendkey` 测试通过：**

按固定顺序，不要即兴加命令：

```text
help
echoo<Backspace> hello
touch note
write note hello tiny
cat note
append note again
cat note
ls
status
status
rm note
cat note
ls
about
```

讲解点（各不超过一句）：

- Backspace 把 `echoo` 改成 `echo`。
- `append` 后 `ls` 显示 `note 16`。
- 两次 `status` 的 ticks/irq0 变大，dropped 为 0。
- `about` 里有 `Ring 0`。

## 5:00–5:40  测试怎么证明

口播：启动测试证明模块初始化；`sendkey` 证明完整 IRQ1 链路。直接调用 `keyboard_feed_scancode` 或 parser 不能代替现场键。Monitor socket 在容器 `/tmp`，避免 Windows 卷上的 Unix socket 问题。

## 5:40–6:10  限制与收束

口播三句：没有用户态；没有磁盘；下一步只有老师要求才做 Ring 3。打开仓库 URL，结束。

## 超时与失败备用

| 现场问题 | 切换 |
|---|---|
| QEMU 起不来 | 展示第三轮 64 MiB 日志截图 + CR0 截图 |
| Shell 无提示符 | 退回键盘回显，承认待集成 |
| 命令输出与脚本不一致 | 不要现场改代码；说明自动测试阶段名，改口播限制 |
| 键盘 dropped 非 0 | 停止连打，说明 64 字节队列和 20 ms 键间隔 |

## 60–90 秒备用录像清单（代码冻结后补拍，Day 1 不伪称已有成片）

1. 容器内 `make test` 末尾 `QEMU boot matrix: PASS`（约 20 s）。
2. 64 MiB 串口从 `TinyShell OS booting` 到 `BOOT_OK`（约 25 s）。
3. 若交互已通：`tiny> ` 下 touch/write/cat/about（约 40 s）。
4. 口播 Ring 0 限制（约 10 s）。

存放位置：仓库外答辩目录，不提交 Git。
