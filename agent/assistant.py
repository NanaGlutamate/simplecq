from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple

from .config import AgentConfig
from .llm_client import ChatMessage, OpenAICompatibleChatClient
from .project_context import ProjectContext
from .checkers import check_assemble_file, check_scene_file
from .checkers.common import summarize_issues
from .error_analyzer import analyze_error_text, format_hints


_SYSTEM_PROMPT = """你是 SimpleCQ 平台的内置 AI 助手。

语言规则：
- 如果用户当前这条提问主要由英文单词/句子组成（例如全英文报错、配置、问题描述），请用**英文**回答。
- 其他情况下（包含中文或主要为中文），请用**中文**回答。
- 不要在答案里解释你是如何选择语言的，直接用选定语言作答即可。

你擅长的事情：
1) 解释平台整体架构（assembled model / agent model / sim controller）
2) 指导平台使用（加载 scene.yml、运行、训练脚本如何接入）
3) 解释并检查两类关键配置文件：
   - 组合关系：assemble.yml（组合模型/字段转换）
   - 交互关系：scene*.yml（model_types/models/topics 发布订阅与字段映射）
4) 对错误日志进行分析：给出“最可能原因 + 证据 + 下一步怎么做”

重要约束：
- 不要泄露任何密钥/令牌内容；遇到疑似密钥应提示用户做脱敏。
- 当用户给出文件路径时，优先结合文件内容与项目资料回答。
"""


def _is_assemble_path(p: Path) -> bool:
    name = p.name.lower()
    return "assemble" in name or "assamble" in name  # readme 里有拼写差异


def _is_scene_path(p: Path) -> bool:
    name = p.name.lower()
    return name.startswith("scene") and name.endswith((".yml", ".yaml"))


@dataclass
class CheckResult:
    kind: str  # assemble|scene|unknown
    issues_text: str


class SimpleCQAssistant:
    def __init__(self, cfg: AgentConfig):
        self.cfg = cfg
        self.ctx = ProjectContext(cfg.project.root, cfg.project.docs)
        self.llm = OpenAICompatibleChatClient(cfg.llm)

    def project_context_block(self) -> str:
        return self.ctx.build_context_block()

    def overview(self) -> str:
        context = self.project_context_block()
        msgs = [
            ChatMessage("system", _SYSTEM_PROMPT),
            ChatMessage(
                "user",
                "请基于以下项目资料，给出平台整体架构的说明：包含模块职责、数据流/控制流、典型运行流程。\n\n"
                "要求：\n"
                "- 用 6-12 条要点\n"
                "- 点出关键配置文件（assemble.yml、scene*.yml）分别解决什么问题\n"
                "- 点出 C++ 引擎与 Python 训练脚本如何通过 agent.dll/socket 交互\n\n"
                f"项目资料：\n{context}",
            ),
        ]
        return self.llm.chat(msgs)

    def usage_guide(self) -> str:
        context = self.project_context_block()
        msgs = [
            ChatMessage("system", _SYSTEM_PROMPT),
            ChatMessage(
                "user",
                "请基于以下项目资料，给一个“从零到跑起来”的使用指导。\n\n"
                "要求：\n"
                "- 分成：构建/准备 -> 编写配置 -> 启动仿真 -> 训练脚本接入 -> 常见坑\n"
                "- 给出具体命令（例如 tinycq 交互命令 load/edit/r 等）\n"
                "- 如果某些路径/命令可能因环境不同而变化，请明确标注\n\n"
                f"项目资料：\n{context}",
            ),
        ]
        return self.llm.chat(msgs)

    def explain_file_format(self, file_kind: str) -> str:
        context = self.project_context_block()
        if file_kind not in {"assemble", "scene"}:
            file_kind = "scene"
        prompt = (
            "请解释以下两类文件的格式与常见错误点，并给出检查清单：\n"
            "A) assemble.yml（组合关系/字段转换）\n"
            "B) scene*.yml（交互关系/topics 发布订阅）\n\n"
            "要求：\n"
            "- 先用表格列出关键字段、类型、含义\n"
            "- 然后列出至少 10 条常见错误与排查方法\n"
            "- 最后给出一个最小可用示例（只要骨架，不要长 XML）\n\n"
            f"项目资料：\n{context}"
        )
        msgs = [ChatMessage("system", _SYSTEM_PROMPT), ChatMessage("user", prompt)]
        return self.llm.chat(msgs)

    def check_file(self, path: str | Path) -> CheckResult:
        p = Path(path)
        if _is_assemble_path(p):
            issues = check_assemble_file(p)
            return CheckResult("assemble", summarize_issues(issues))
        if _is_scene_path(p) or p.suffix.lower() in {".yml", ".yaml"}:
            issues = check_scene_file(p)
            return CheckResult("scene", summarize_issues(issues))
        return CheckResult("unknown", "不支持的文件类型（仅支持 .yml/.yaml）。")

    def analyze_error(self, text: str, *, use_llm: bool = True) -> str:
        hints = analyze_error_text(text)
        heuristic = format_hints(hints)
        if not use_llm:
            return heuristic

        context = self.project_context_block()
        msgs = [
            ChatMessage("system", _SYSTEM_PROMPT),
            ChatMessage(
                "user",
                "下面是用户遇到的报错/日志。请你：\n"
                "1) 先用 1 段话复述问题\n"
                "2) 给出 3-5 个最可能原因（按概率排序），每个原因要引用日志中的证据关键词\n"
                "3) 给出下一步排查顺序（可操作，尽量具体到文件/字段/命令）\n"
                "4) 如果日志包含疑似密钥，提醒脱敏\n\n"
                f"项目资料：\n{context}\n\n"
                f"启发式线索（供参考，可改写）：\n{heuristic}\n\n"
                f"报错/日志：\n{text}",
            ),
        ]
        try:
            return self.llm.chat(msgs)
        except Exception:
            # LLM 不可用时返回启发式分析
            return heuristic

    def chat(self, history: List[ChatMessage], user_text: str) -> str:
        context = self.project_context_block()
        msgs = [ChatMessage("system", _SYSTEM_PROMPT + "\n\n项目资料：\n" + context)]
        msgs.extend(history[-12:])  # 简单截断，避免上下文过长
        msgs.append(ChatMessage("user", user_text))
        return self.llm.chat(msgs)

