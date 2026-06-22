# SimpleCQ

[English](README.md) | [中文](README_zh.md) | [한국어](README_ko.md) | [日本語](README_ja.md)

## Overview

SimpleCQ is an open-source framework for computer-generated forces (CGF) behavior training with deep reinforcement learning (DRL).

SimpleCQ keeps scenario execution configurable while exposing the runtime interfaces needed by learning workloads. It loads heterogeneous model DLLs, routes entity outputs through YAML-defined topics, advances simulation frames with a reusable task graph, and connects agent-controlled entities to Python-side reinforcement-learning runners.

The project is especially focused on the simulation engine and rollout acceleration:

- Taskflow-based frame scheduling replaces a fixed serial model loop.
- Topic routing decouples model outputs, message transformation, and target inputs.
- Double-buffered topic exchange keeps deterministic frame semantics.
- Dynamic model creation and cleanup are integrated into the frame graph.
- Asynchronous rollout workers and Gym-style agent interaction can support high-throughput DRL sampling.
- Alternating self-play runners can support adversarial policy training.
- `output_movable` enables move-based output transfer when the model owner can guarantee safe ownership transfer.

## Runtime Architecture

The engine advances each frame through the following causal order:

1. Each model publishes output with `GetOutput`.
2. Topic rules check whether the required members are present.
3. Topic data are transformed and collected for subscribers.
4. Topic buffers are swapped, so all models read a complete frame-aligned input set.
5. Each target model receives data through `SetInput`.
6. Each model advances with `Tick(dt)`.
7. Dynamic models requested by callbacks are created, and destroyed models are cleaned after an expiry window.

This order is implemented in `src/engine/executionengine.hpp`. `ExecutionEngine::buildGraph()` compiles the frame into a Taskflow DAG, and `ExecutionEngine::run(times)` reuses that DAG for repeated frame advancement while recording FPS.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `src/engine/` | Runtime engine, console interface, task graph scheduling, model management |
| `src/model.cpp` | Assembled model DLL that composes multiple atomic models through `assemble.yml` |
| `src/agent.cpp` | Socket-based agent model DLL for Python-side learning interaction |
| `src/gym_interface.py` | Python-side Gym-style communication helper |
| `config/` | Example scenario, assembled-model, and model-description files |
| `scripts/` | Training notebooks for multiprocessing, vectorized environments, and self-play |
| `3rd-party/` | Vendored Taskflow, yaml-cpp, and RapidXML sources |
| `agent/` | Optional project assistant for documentation and configuration checking |
| `doc/README_Detailed.md` | Detailed engine guide and extended usage notes |

## Build

Recommended environment:

- Windows with MSVC 2022
- CMake 3.21 or newer when using `CMakePresets.json`
- Ninja
- vcpkg available at `./vcpkg`, or an adjusted `toolchainFile` in `CMakePresets.json`

The only vcpkg dependency declared by this repository is `mimalloc`; Taskflow and yaml-cpp are vendored in `3rd-party/`.

```powershell
cmake --preset ninja-multi-vcpkg
cmake --build --preset ninja-vcpkg-release
```

Expected build products are placed under `bin/Release/`, including:

- `tinycq.exe`: interactive simulation controller
- `mymodel.dll`: assembled model wrapper
- `agent.dll`: socket-based learning agent model

The GitHub Actions workflow `.github/workflows/compile.yml` uses the same CMake presets on `windows-latest`.

## Quick Start

Before running an example, update the `dll_path` fields in the selected YAML file. Some files in `config/` preserve absolute paths from the original experiment environment.

```powershell
.\bin\Release\tinycq.exe
```

Inside the console:

```text
>>> load config\navigation.yml
>>> set dt 100
>>> set drawrate 100
>>> r 1000
>>> model
```

Available console commands include:

| Command | Meaning |
| --- | --- |
| `load <file>` or `l <file>` | Load a scenario YAML file |
| `run <frames>` or `r <frames>` | Advance the simulation |
| `cfg` | List configurable runtime options |
| `get <key>` | Read a runtime option |
| `set <key> <value>` | Update a runtime option |
| `print` or `p` | Print the current topic buffer |
| `model` | List static and dynamic models |

## Detailed Engine Guide

For a more detailed explanation of the engine, topic scheduling, assembled-model restart, scenario routing rules, profiling, visualization, and source layout, see [doc/README_Detailed.md](doc/README_Detailed.md).

## Scenario Configuration

A scenario YAML file describes the executable simulation graph:

- `model_types`: model type names, DLL paths, and `output_movable` settings.
- `models`: model instances with `model_type`, `side_id`, `id`, and XML-formatted `init_value`.
- `topics`: publish-subscribe rules. A topic declares a publisher, required output members, subscribers, and optional name conversion rules.

The cooperative reconnaissance example is represented by `config/navigation.yml`. The 2-vs-2 adversarial self-play example is represented by `config/scene_planebattle2v2_self_learning.yml`.

## Assembled Model Configuration

`mymodel.dll` loads `assemble.yml` from its DLL directory. The assembled model allows multiple atomic models to be exposed as one CGF model. The file contains:

- `config`: optional profiling path, restart key, side filter, and log level.
- `models`: atomic model DLLs and logical names.
- `init_convert`: root initialization data routed to submodels.
- `input_convert`: root input data routed to submodels.
- `output_convert`: submodel outputs routed to the root or to other submodels.

If `restart_key` is configured and a `SetInput` package contains that key, the assembled model recreates and reinitializes its submodels before the next `GetOutput`.

## DRL Interface

The agent model in `src/agent.cpp` connects to `127.0.0.1:<port>` and exchanges serialized `CSValueMap` data with Python. It wraps simulator-side `Init`, `SetInput`, `GetOutput`, and `Tick` into the learning-side reset-step loop:

- `reset` initializes the scenario and returns aligned observations.
- `step(action)` writes commands into topic data, advances frames, and returns observations, rewards, and done flags.
- Training scripts can support multiple environments and asynchronous rollout workers to increase sample throughput.

## Examples

The following examples show currently supported workflows:

- Cooperative UAV reconnaissance: `config/navigation.yml`, `scripts/Train_vecenv.ipynb`.
- Asynchronous and multiprocess rollout acceleration can be supported through `scripts/Train_Multiprocess.ipynb`.
- 2-vs-2 alternating self-play can be supported through `config/scene_planebattle2v2_self_learning.yml` and `scripts/Train2v2_self_learning.ipynb`.

## License

This project is released under the MIT License. See `LICENSE`.
