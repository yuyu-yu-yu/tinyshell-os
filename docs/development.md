# 协作开发约定

## 分支

- `main` 必须始终可构建、可启动。
- 每项工作使用短生命周期分支，例如 `feature/page-allocator`。
- 合并前由至少一名非作者成员阅读代码，并确认 Docker CI 通过。

## 提交

- 一次提交只解决一个明确问题。
- 提交前运行 `tools/docker-test.ps1` 或 `tools/docker-test.sh`。
- 宿主机上的 `make test` 只能用于快速反馈，Docker 测试才是统一验收结果。
- GitHub Actions 的 Docker CI 失败时禁止合并。
- 不提交 `build/`、ISO、日志、编辑器状态或本地配置。
- 提交信息推荐使用 `模块: 动作`，例如 `boot: add Multiboot entry`。

## AI 协作

- 每次只让 AI 修改明确列出的文件和接口。
- 要求 AI 说明不变量、失败路径和测试方法。
- 组员必须逐行审查、运行测试并能口头解释后才能合并。
- 保留外部参考与 AI 辅助记录，不把无法解释的生成代码作为原创成果。
