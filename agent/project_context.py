from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Optional, Tuple


_DEFAULT_DENYLIST_NAMES = {
    # 避免把敏感信息塞给模型（例如你仓库根目录下的 keys.json）
    "keys.json",
    ".env",
    ".env.local",
    ".env.production",
    ".env.development",
}


def _read_text_limited(path: Path, *, limit_chars: int = 24_000) -> str:
    try:
        s = path.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        s = path.read_text(encoding="utf-8", errors="replace")
    if len(s) > limit_chars:
        return s[:limit_chars] + "\n\n...（已截断）\n"
    return s


@dataclass(frozen=True)
class DocSnippet:
    relpath: str
    content: str


class ProjectContext:
    def __init__(self, root: Path, docs: Iterable[str]):
        self.root = root
        self.docs = list(docs)

    def collect_snippets(self) -> List[DocSnippet]:
        snippets: List[DocSnippet] = []
        for rel in self.docs:
            rel = rel.strip()
            if not rel:
                continue
            p = (self.root / rel).resolve()
            if p.name in _DEFAULT_DENYLIST_NAMES:
                continue
            if not p.exists() or not p.is_file():
                continue
            snippets.append(DocSnippet(relpath=str(Path(rel)), content=_read_text_limited(p)))
        return snippets

    def build_context_block(self) -> str:
        """
        生成给 LLM 的“项目资料块”（尽量短且高信噪）。
        """
        snippets = self.collect_snippets()
        if not snippets:
            return "（未找到可用的项目说明文档/样例配置文件）"

        parts: List[str] = []
        for snip in snippets:
            parts.append(f"[文件：{snip.relpath}]\n{snip.content}")
        return "\n\n---\n\n".join(parts)

    def quick_overview_fallback(self) -> str:
        """
        当未配置 LLM 时，返回一个基于 readme 的兜底概述。
        """
        readme = (self.root / "readme.md")
        if readme.exists():
            return _read_text_limited(readme, limit_chars=4000)
        return "未找到 readme.md，且未配置 LLM。"

