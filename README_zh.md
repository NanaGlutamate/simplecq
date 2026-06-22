# SimpleCQ

[English](README.md) | [中文](README_zh.md) | [한국어](README_ko.md) | [日本語](README_ja.md)

## 项目概述

SimpleCQ 是一个面向计算机生成兵力（CGF）行为训练的开源深度强化学习仿真框架。

SimpleCQ 保留配置驱动的 CGF 想定执行方式，同时提供强化学习训练所需的运行接口。框架可以加载异构模型 DLL，通过 YAML 主题规则路由实体输出，使用可复用任务图推进仿真帧，并通过代理模型连接 Python 侧训练程序。

本项目 README 重点突出引擎与加速能力：

- 基于 Taskflow 的帧调度替代固定串行模型循环。
- 主题路由解耦模型输出、消息转换和目标输入。
- 双缓冲主题交换保证确定性的帧语义。
- 动态模型的创建与清理被纳入帧任务图。
- 异步 rollout worker 与类 Gym 代理接口可支持高吞吐强化学习采样。
- 交替自博弈 runner 可支持对抗策略训练。
- `output_movable` 在模型所有权安全时支持移动语义传输，减少输出数据拷贝。

## 运行时架构

引擎每一帧按以下因果顺序推进：

1. 每个模型通过 `GetOutput` 发布输出。
2. 主题规则检查输出是否包含所需成员。
3. 主题数据被转换并收集到订阅者。
4. 主题缓冲区交换，使所有模型读取同一帧对齐后的输入。
5. 目标模型通过 `SetInput` 接收数据。
6. 模型通过 `Tick(dt)` 推进状态。
7. 回调请求的动态模型被创建，失效模型在过期窗口后清理。

该流程实现在 `src/engine/executionengine.hpp` 中。`ExecutionEngine::buildGraph()` 将帧流程编译为 Taskflow DAG，`ExecutionEngine::run(times)` 复用该 DAG 执行多帧推进并统计 FPS。

## 仓库结构

| 路径 | 说明 |
| --- | --- |
| `src/engine/` | 运行时引擎、控制台接口、任务图调度和模型管理 |
| `src/model.cpp` | 组合模型 DLL，通过 `assemble.yml` 组合多个原子模型 |
| `src/agent.cpp` | 基于 socket 的学习代理模型 DLL |
| `src/gym_interface.py` | Python 侧类 Gym 通信辅助接口 |
| `config/` | 示例想定、组合模型配置和模型描述文件 |
| `scripts/` | 多进程、向量化环境和自博弈训练 notebook |
| `3rd-party/` | 随仓库提供的 Taskflow、yaml-cpp 和 RapidXML 源码 |
| `agent/` | 可选的项目说明与配置检查助手 |
| `doc/README_Detailed.md` | 更详细的引擎说明与扩展使用说明 |

## 构建

推荐环境：

- Windows 与 MSVC 2022
- 使用 `CMakePresets.json` 时需要 CMake 3.21 或更高版本
- Ninja
- `./vcpkg` 目录下可用的 vcpkg，或自行修改 `CMakePresets.json` 中的 `toolchainFile`

仓库通过 vcpkg 声明的唯一依赖是 `mimalloc`；Taskflow 和 yaml-cpp 已放在 `3rd-party/` 中。

```powershell
cmake --preset ninja-multi-vcpkg
cmake --build --preset ninja-vcpkg-release
```

主要产物位于 `bin/Release/`：

- `tinycq.exe`：交互式仿真控制器
- `mymodel.dll`：组合模型封装
- `agent.dll`：面向学习训练的 socket 代理模型

GitHub Actions 工作流 `.github/workflows/compile.yml` 在 `windows-latest` 上使用相同的 CMake preset。

## 快速运行

运行示例前，请先修改所选 YAML 文件中的 `dll_path`。`config/` 内部分文件保留了实验环境中的绝对路径。

```powershell
.\bin\Release\tinycq.exe
```

控制台命令示例：

```text
>>> load config\navigation.yml
>>> set dt 100
>>> set drawrate 100
>>> r 1000
>>> model
```

常用命令：

| 命令 | 含义 |
| --- | --- |
| `load <file>` 或 `l <file>` | 加载想定 YAML |
| `run <frames>` 或 `r <frames>` | 推进指定帧数 |
| `cfg` | 列出可配置运行参数 |
| `get <key>` | 查看运行参数 |
| `set <key> <value>` | 修改运行参数 |
| `print` 或 `p` | 打印当前主题缓冲区 |
| `model` | 列出静态和动态模型 |

## 详细引擎说明

更详细的引擎机制、主题调度、组合模型重启、想定路由规则、性能分析、可视化和源码结构说明见 [doc/README_Detailed.md](doc/README_Detailed.md)。

## 想定配置

想定 YAML 文件描述可执行的仿真图：

- `model_types`：模型类型名、DLL 路径和 `output_movable` 设置。
- `models`：模型实例，包含 `model_type`、`side_id`、`id` 和 XML 格式 `init_value`。
- `topics`：发布订阅规则，包含发布者、必要输出成员、订阅者和名称转换规则。

协同无人机侦察示例对应 `config/navigation.yml`。2-vs-2 对抗自博弈示例对应 `config/scene_planebattle2v2_self_learning.yml`。

## 组合模型配置

`mymodel.dll` 会从 DLL 所在目录加载 `assemble.yml`。组合模型用于把多个原子模型封装为一个 CGF 模型。配置文件包括：

- `config`：可选的性能分析路径、重启键、阵营过滤字段和日志级别。
- `models`：原子模型 DLL 与逻辑名称。
- `init_convert`：根模型初始化数据到子模型的数据映射。
- `input_convert`：根模型输入数据到子模型的数据映射。
- `output_convert`：子模型输出到根模型或其他子模型的数据映射。

如果配置了 `restart_key`，并且某次 `SetInput` 数据包含该键，组合模型会在下一次 `GetOutput` 前重新创建并初始化子模型。

## 强化学习接口

`src/agent.cpp` 中的代理模型连接 `127.0.0.1:<port>`，并与 Python 交换序列化后的 `CSValueMap` 数据。它把仿真侧 `Init`、`SetInput`、`GetOutput` 和 `Tick` 包装为学习侧的 reset-step 循环：

- `reset` 初始化想定并返回对齐后的观测。
- `step(action)` 将动作写入主题数据，推进帧，并返回观测、奖励和结束标志。
- 训练脚本可支持多环境和异步 rollout worker，以提高采样吞吐。

## 示例

以下示例展示当前可支持的工作流：

- 协同无人机侦察：`config/navigation.yml`，`scripts/Train_vecenv.ipynb`。
- 可支持异步与多进程 rollout 加速：`scripts/Train_Multiprocess.ipynb`。
- 可支持 2-vs-2 交替自博弈：`config/scene_planebattle2v2_self_learning.yml`，`scripts/Train2v2_self_learning.ipynb`。

## 许可证

本项目使用 MIT License。详见 `LICENSE`。
