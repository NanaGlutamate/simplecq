"""
SimpleCQ AI 助手（Python 侧）。

目标：
- 通过配置 OpenAI/DeepSeek API 提供对话能力
- 提供平台概述、使用指导
- 检查两类关键配置文件：`assemble.yml`（组合关系）与 `scene*.yml`（交互关系）
- 对常见报错/日志进行分析并给出排查建议
"""

from .config import AgentConfig, load_config
from .llm_client import OpenAICompatibleChatClient

__all__ = ["AgentConfig", "load_config", "OpenAICompatibleChatClient"]

