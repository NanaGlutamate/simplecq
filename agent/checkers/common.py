from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


try:
    import yaml  # type: ignore
except Exception:  # pragma: no cover
    yaml = None


@dataclass(frozen=True)
class ValidationIssue:
    severity: str  # ERROR | WARN | INFO
    message: str
    where: str = ""


def load_yaml(path: Path) -> Any:
    if yaml is None:
        raise RuntimeError(
            "未检测到 PyYAML。请先安装依赖：\n"
            "  pip install -r agent/requirements.txt"
        )
    with path.open("r", encoding="utf-8") as f:
        return yaml.safe_load(f)


def _is_mapping(x: Any) -> bool:
    return isinstance(x, dict)


def _is_list(x: Any) -> bool:
    return isinstance(x, list)


def _expect_mapping(issues: List[ValidationIssue], x: Any, where: str) -> Optional[Dict[str, Any]]:
    if not _is_mapping(x):
        issues.append(ValidationIssue("ERROR", "必须是 mapping/dict", where))
        return None
    return x  # type: ignore[return-value]


def _expect_list(issues: List[ValidationIssue], x: Any, where: str) -> Optional[List[Any]]:
    if not _is_list(x):
        issues.append(ValidationIssue("ERROR", "必须是 list/array", where))
        return None
    return x  # type: ignore[return-value]


def _expect_str(issues: List[ValidationIssue], x: Any, where: str) -> Optional[str]:
    if not isinstance(x, str):
        issues.append(ValidationIssue("ERROR", "必须是 string", where))
        return None
    return x


def _expect_bool(issues: List[ValidationIssue], x: Any, where: str) -> Optional[bool]:
    if not isinstance(x, bool):
        issues.append(ValidationIssue("ERROR", "必须是 bool", where))
        return None
    return x


def _expect_int(issues: List[ValidationIssue], x: Any, where: str) -> Optional[int]:
    if not isinstance(x, int):
        issues.append(ValidationIssue("ERROR", "必须是 int", where))
        return None
    return x


def summarize_issues(issues: List[ValidationIssue]) -> str:
    if not issues:
        return "未发现问题。"
    lines: List[str] = []
    for it in issues:
        loc = f" ({it.where})" if it.where else ""
        lines.append(f"[{it.severity}] {it.message}{loc}")
    return "\n".join(lines)


def warn_if_path_missing(issues: List[ValidationIssue], p: str, where: str) -> None:
    # 仅做“提醒”，因为很多路径是环境相关（Debug/Release/RelWithDebInfo）
    try:
        pp = Path(p)
        if not pp.exists():
            issues.append(ValidationIssue("WARN", f"路径不存在或不可访问：{p}", where))
    except Exception:
        issues.append(ValidationIssue("WARN", f"无法解析路径：{p}", where))

