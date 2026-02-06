from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional
import os


def _repo_root_from_this_file() -> Path:
    # .../simplecq/agent/config.py -> parents[1] == .../simplecq
    return Path(__file__).resolve().parents[1]


@dataclass(frozen=True)
class LLMConfig:
    provider: str = "deepseek"  # openai | deepseek
    api_key: str = ""
    base_url: str = ""  # e.g. https://api.deepseek.com or https://api.openai.com
    model: str = "deepseek-chat"
    temperature: float = 0.2
    timeout_seconds: int = 60

    def resolved_base_url(self) -> str:
        if self.base_url.strip():
            return self.base_url.strip().rstrip("/")
        if self.provider.lower() == "openai":
            return "https://api.openai.com"
        # default deepseek
        return "https://api.deepseek.com"


@dataclass(frozen=True)
class ProjectConfig:
    root: Path
    docs: List[str]


@dataclass(frozen=True)
class AgentConfig:
    llm: LLMConfig
    project: ProjectConfig


def _load_yaml_config(path: Path) -> Dict[str, Any]:
    try:
        import yaml  # type: ignore
    except Exception as e:  # pragma: no cover
        raise RuntimeError(
            "未检测到 PyYAML。请先安装依赖：\n"
            "  pip install -r agent/requirements.txt\n"
            f"原始错误：{e}"
        )

    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}
    if not isinstance(data, dict):
        raise ValueError(f"配置文件顶层必须是 YAML mapping：{path}")
    return data


def load_config(
    config_path: Optional[str] = None,
    project_root: Optional[str] = None,
) -> AgentConfig:
    """
    加载配置优先级：
    1) 环境变量 SIMPLECQ_LLM_*（若提供则覆盖）
    2) agent/agent_config.yaml（若存在）
    3) 默认值
    """
    repo_root = _repo_root_from_this_file()
    default_cfg_path = repo_root / "agent" / "agent_config.yaml"
    cfg_path = Path(config_path).expanduser().resolve() if config_path else default_cfg_path

    raw = _load_yaml_config(cfg_path)
    llm_raw: Dict[str, Any] = raw.get("llm", {}) if isinstance(raw.get("llm", {}), dict) else {}
    project_raw: Dict[str, Any] = raw.get("project", {}) if isinstance(raw.get("project", {}), dict) else {}

    llm = LLMConfig(
        provider=str(llm_raw.get("provider", "deepseek")),
        api_key=str(llm_raw.get("api_key", "")),
        base_url=str(llm_raw.get("base_url", "")),
        model=str(llm_raw.get("model", "deepseek-chat")),
        temperature=float(llm_raw.get("temperature", 0.2)),
        timeout_seconds=int(llm_raw.get("timeout_seconds", 60)),
    )

    # repo root override: 参数 > yaml > 自动推断
    root_str = (
        project_root
        or str(project_raw.get("root", "")).strip()
        or str(repo_root)
    )
    root = Path(root_str).expanduser().resolve()

    docs = project_raw.get("docs", None)
    if not isinstance(docs, list) or not all(isinstance(x, str) for x in docs):
        docs = [
            "readme.md",
            "doc/SimpleCQ代码文件说明 .md",
            "config/file_simplecq/assemble.yml",
            "config/file_simplecq/scene_planebattle.yml",
        ]

    # env override
    llm_provider = os.getenv("SIMPLECQ_LLM_PROVIDER")
    llm_key = os.getenv("SIMPLECQ_LLM_API_KEY")
    llm_base = os.getenv("SIMPLECQ_LLM_BASE_URL")
    llm_model = os.getenv("SIMPLECQ_LLM_MODEL")
    llm_temp = os.getenv("SIMPLECQ_LLM_TEMPERATURE")
    llm_timeout = os.getenv("SIMPLECQ_LLM_TIMEOUT_SECONDS")

    if llm_provider:
        llm = LLMConfig(**{**llm.__dict__, "provider": llm_provider})
    if llm_key:
        llm = LLMConfig(**{**llm.__dict__, "api_key": llm_key})
    if llm_base:
        llm = LLMConfig(**{**llm.__dict__, "base_url": llm_base})
    if llm_model:
        llm = LLMConfig(**{**llm.__dict__, "model": llm_model})
    if llm_temp:
        try:
            llm = LLMConfig(**{**llm.__dict__, "temperature": float(llm_temp)})
        except ValueError:
            pass
    if llm_timeout:
        try:
            llm = LLMConfig(**{**llm.__dict__, "timeout_seconds": int(llm_timeout)})
        except ValueError:
            pass

    project = ProjectConfig(root=root, docs=docs)
    return AgentConfig(llm=llm, project=project)

