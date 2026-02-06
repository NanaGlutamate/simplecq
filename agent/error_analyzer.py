from __future__ import annotations

import re
from dataclasses import dataclass
from typing import List, Optional, Tuple


@dataclass(frozen=True)
class ErrorHint:
    title: str
    why: str
    next_steps: List[str]


def _contains(s: str, *subs: str) -> bool:
    ss = s.lower()
    return any(x.lower() in ss for x in subs)


def analyze_error_text(text: str) -> List[ErrorHint]:
    """
    对日志/报错做启发式分析（不依赖 LLM）。
    返回多个可能方向，供对话助手整合输出。
    """
    t = text.strip()
    if not t:
        return [ErrorHint("未提供报错内容", "输入为空", ["把完整 traceback / 引擎日志粘贴进来（建议包含前后 30 行）"])]

    hints: List[ErrorHint] = []

    # YAML 解析
    if _contains(t, "yaml::", "parserexception", "bad conversion", "yaml-cpp", "could not find expected", "mapping values are not allowed"):
        hints.append(
            ErrorHint(
                "YAML 配置解析失败",
                "引擎侧使用 yaml-cpp 解析 `scene*.yml` / `assemble.yml`，缩进/冒号/引号/列表格式错误会直接抛异常。",
                [
                    "确认报错指向的 yml 文件路径与行号（若有）。",
                    "重点检查：列表项 `-` 是否对齐、字符串是否缺失引号、冒号后是否有空格。",
                    "用本助手的 `/check <yml路径>` 先做结构校验（能提前发现不少低级错误）。",
                ],
            )
        )

    # DLL/LoadLibrary
    if _contains(t, "loadlibrary", "dlopen", "cannot open", "could not load", "the specified module could not be found", ".dll"):
        hints.append(
            ErrorHint(
                "动态库加载失败（dll 路径/依赖缺失）",
                "常见于 `scene*.yml` 的 `model_types[].dll_path` 或 `assemble.yml` 的 `models[].dll_name` 指向不存在的 dll，或 dll 依赖的其他 dll 不在 PATH。",
                [
                    "先检查 yml 里 dll 路径是否存在（Debug/Release/RelWithDebInfo 容易写错）。",
                    "如果 dll 存在仍加载失败：用 `Dependencies`/`dumpbin` 检查依赖项是否缺失。",
                    "确认运行目录与 PATH 是否包含 `yaml.dll`、`mimalloc*.dll` 等运行时依赖所在目录。",
                ],
            )
        )

    # 端口占用/连接
    if _contains(t, "address already in use", "10048", "bind", "eaddrinuse"):
        hints.append(
            ErrorHint(
                "端口被占用（Agent socket bind 失败）",
                "你的 Python `gym_interface.Agent` 会在本机 `bind(host, port)` 并 `accept()`；如果端口已被占用会报错。",
                [
                    "检查 `scene*.yml` 中 agent 模型的 `init_value` 里 `<port>` 是否与现有进程冲突。",
                    "关闭占用端口的进程，或换一个端口（同时保证引擎侧与 Python 侧一致）。",
                ],
            )
        )

    if _contains(t, "connection refused", "10061", "connect", "timed out", "timeout"):
        hints.append(
            ErrorHint(
                "连接失败（端口/时序/主机不一致）",
                "引擎与 Python 之间通过 socket 通信，常见问题是端口不一致、先后启动顺序不对，或 host 不是 localhost。",
                [
                    "确认 `scene*.yml` agent 的 `<port>` 与 Python 侧 `Agent(port=...)` 完全一致。",
                    "确认启动顺序：先启动 Python 侧监听，再启动仿真/引擎侧去连接（取决于你 agent.dll 实现）。",
                    "如果跨机器：确认防火墙与 host 配置。",
                ],
            )
        )

    # Python eval 风险点（gym_interface.py: eval(recv)）
    if _contains(t, "syntaxerror", "nameerror") and _contains(t, "eval("):
        hints.append(
            ErrorHint(
                "Python 侧解析失败（eval 输入不是合法 Python 字面量）",
                "当前 `gym_interface.Agent._recv()` 使用 `eval()` 解析引擎传回的字符串，如果返回不是 Python dict/list 字面量就会报错。",
                [
                    "打印/记录 socket 收到的原始字符串，确认其格式（是否为 JSON？是否含多余日志前缀？）。",
                    "更稳健的做法：双方改用 JSON，并用 `json.loads()` 解析（能避免执行任意代码的风险）。",
                ],
            )
        )

    # KeyError / 数据字段不匹配
    if re.search(r"\bKeyError\b", t) or _contains(t, "keyerror"):
        hints.append(
            ErrorHint(
                "字段缺失（topics members / name_convert / parse_Output 不匹配）",
                "Python 侧常见是 `parse_Output`/`outputToTensor` 访问了不存在字段；引擎侧常见是 topics 的 members/name_convert 配置与模型实际输出不一致。",
                [
                    "用 `/check <scene.yml>` 检查 members 与 name_convert 的一致性（本助手会提示 src_name/name 不在 members）。",
                    "对照模型 `GetOutput` 实际输出字段，逐个核对 scene.yml 的 members 列表。",
                    "检查 Python 侧 `parse_Output` 是否假设了某些 key 必定存在。",
                ],
            )
        )

    if not hints:
        hints.append(
            ErrorHint(
                "需要更多上下文",
                "当前报错没有命中已知模式，仍可通过上下文定位。",
                [
                    "粘贴更完整的日志（含文件路径/行号/调用栈）。",
                    "说明你在跑哪一步：加载 scene.yml、加载 dll、开始 run、训练 step/reset 等。",
                ],
            )
        )

    return hints


def format_hints(hints: List[ErrorHint]) -> str:
    lines: List[str] = []
    for h in hints:
        lines.append(f"## {h.title}")
        lines.append(h.why)
        lines.append("")
        lines.append("建议排查：")
        for s in h.next_steps:
            lines.append(f"- {s}")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"

