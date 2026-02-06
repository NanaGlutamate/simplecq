from __future__ import annotations

import argparse
from pathlib import Path
import sys
from typing import List, Optional

from .assistant import SimpleCQAssistant
from .config import load_config
from .llm_client import ChatMessage, LLMError


def _print(s: str) -> None:
    sys.stdout.write(s)
    if not s.endswith("\n"):
        sys.stdout.write("\n")


def _ensure_utf8_stdio() -> None:
    # Windows/某些终端下默认编码可能导致中文乱码
    try:
        sys.stdout.reconfigure(encoding="utf-8")  # type: ignore[attr-defined]
    except Exception:
        pass
    try:
        sys.stderr.reconfigure(encoding="utf-8")  # type: ignore[attr-defined]
    except Exception:
        pass


def _read_multiline_until_end(prompt: str = "") -> str:
    if prompt:
        _print(prompt)
    _print("请粘贴内容，结束请输入单独一行：END")
    lines: List[str] = []
    while True:
        try:
            line = input()
        except EOFError:
            break
        if line.strip() == "END":
            break
        lines.append(line)
    return "\n".join(lines).strip()


def _help_text() -> str:
    return (
        "SimpleCQ AI 助手（交互式）\n"
        "\n"
        "常用命令：\n"
        "  /overview               平台整体说明（需要配置 LLM）\n"
        "  /guide                  平台使用指导（需要配置 LLM）\n"
        "  /format                 解释 assemble.yml 与 scene*.yml 格式（需要配置 LLM）\n"
        "  /check <path>           检查 yml 文件格式（不需要 LLM）\n"
        "  /analyze                粘贴报错/日志进行分析（无 LLM 也可启发式分析）\n"
        "  /exit                   退出\n"
        "\n"
        "配置方式（二选一）：\n"
        "  1) 复制 `agent/agent_config.example.yaml` 为 `agent/agent_config.yaml` 并填写 api_key\n"
        "  2) 设置环境变量：SIMPLECQ_LLM_PROVIDER / SIMPLECQ_LLM_API_KEY / SIMPLECQ_LLM_MODEL / SIMPLECQ_LLM_BASE_URL\n"
    )


def _cmd_overview(a: SimpleCQAssistant) -> int:
    try:
        _print(a.overview())
        return 0
    except LLMError as e:
        _print(str(e))
        _print("\n（未配置 LLM，先给你 readme 的兜底内容）\n")
        _print(a.ctx.quick_overview_fallback())
        return 2


def _cmd_guide(a: SimpleCQAssistant) -> int:
    try:
        _print(a.usage_guide())
        return 0
    except LLMError as e:
        _print(str(e))
        return 2


def _cmd_format(a: SimpleCQAssistant) -> int:
    try:
        _print(a.explain_file_format("scene"))
        return 0
    except LLMError as e:
        _print(str(e))
        return 2


def _cmd_check(a: SimpleCQAssistant, path: str) -> int:
    try:
        r = a.check_file(path)
        _print(f"文件类型：{r.kind}")
        _print(r.issues_text)
        return 0 if "ERROR" not in r.issues_text else 1
    except Exception as e:
        _print(f"检查失败：{e}")
        return 3


def _cmd_analyze(a: SimpleCQAssistant, text: str) -> int:
    if not text.strip():
        text = _read_multiline_until_end()
    _print(a.analyze_error(text, use_llm=True))
    return 0


def _run_repl(a: SimpleCQAssistant) -> int:
    _print(_help_text())
    history: List[ChatMessage] = []

    while True:
        try:
            s = input("\n> ").strip()
        except (EOFError, KeyboardInterrupt):
            _print("\nbye")
            return 0

        if not s:
            continue
        if s in {"/exit", "exit", "quit", ":q"}:
            _print("bye")
            return 0
        if s in {"/help", "help", "?"}:
            _print(_help_text())
            continue

        if s == "/overview":
            _cmd_overview(a)
            continue
        if s == "/guide":
            _cmd_guide(a)
            continue
        if s == "/format":
            _cmd_format(a)
            continue
        if s.startswith("/check "):
            _cmd_check(a, s[len("/check ") :].strip().strip('"'))
            continue
        if s == "/analyze":
            _cmd_analyze(a, "")
            continue

        # 普通对话：如果消息里像是给了 yml 路径且文件存在，先自动检查再回答
        maybe_path = Path(s.strip('"'))
        if maybe_path.exists() and maybe_path.suffix.lower() in {".yml", ".yaml"}:
            r = a.check_file(maybe_path)
            _print(f"（自动检查：{maybe_path}）")
            _print(r.issues_text)
            # 同时让 LLM 解释这些问题（若可用）
            try:
                explain = a.chat(
                    history,
                    "我给你一个 yml 文件路径。请根据你对 assemble.yml/scene.yml 的理解，解释下面检查结果意味着什么，"
                    "并给出修复建议。\n\n"
                    f"路径：{maybe_path}\n"
                    f"检查结果：\n{r.issues_text}",
                )
                _print(explain)
            except LLMError as e:
                _print(f"（LLM 不可用：{e}）")
            continue

        # 纯聊天
        try:
            reply = a.chat(history, s)
            _print(reply)
            history.append(ChatMessage("user", s))
            history.append(ChatMessage("assistant", reply))
        except LLMError as e:
            _print(str(e))
            _print("（提示：你仍可用 /check 和 /analyze 这些不强依赖 LLM 的能力）")


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(prog="python -m agent", add_help=True)
    parser.add_argument("--config", default=None, help="配置文件路径（默认：agent/agent_config.yaml）")
    parser.add_argument("--project-root", default=None, help="项目根目录（默认自动推断）")

    sub = parser.add_subparsers(dest="cmd")
    sub.add_parser("repl", help="进入交互式对话（默认）")

    sub_over = sub.add_parser("overview", help="输出平台整体说明（需 LLM）")
    sub_guide = sub.add_parser("guide", help="输出平台使用指导（需 LLM）")
    sub_fmt = sub.add_parser("format", help="解释文件格式（需 LLM）")

    sub_check = sub.add_parser("check", help="检查 yml 文件")
    sub_check.add_argument("path", help="yml 文件路径")

    sub_an = sub.add_parser("analyze", help="分析报错/日志（可粘贴或从文件读取）")
    sub_an.add_argument("--file", default=None, help="从文件读取日志")

    args = parser.parse_args(argv)
    _ensure_utf8_stdio()
    cfg = load_config(config_path=args.config, project_root=args.project_root)
    assistant = SimpleCQAssistant(cfg)

    cmd = args.cmd or "repl"
    if cmd == "overview":
        return _cmd_overview(assistant)
    if cmd == "guide":
        return _cmd_guide(assistant)
    if cmd == "format":
        return _cmd_format(assistant)
    if cmd == "check":
        return _cmd_check(assistant, args.path)
    if cmd == "analyze":
        if args.file:
            text = Path(args.file).read_text(encoding="utf-8", errors="replace")
            return _cmd_analyze(assistant, text)
        return _cmd_analyze(assistant, "")

    return _run_repl(assistant)

