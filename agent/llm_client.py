from __future__ import annotations

from dataclasses import dataclass
import json
from typing import Any, Dict, List, Optional
from urllib import request, error

from .config import LLMConfig


@dataclass(frozen=True)
class ChatMessage:
    role: str  # system|user|assistant
    content: str


class LLMError(RuntimeError):
    pass


class OpenAICompatibleChatClient:
    """
    使用 OpenAI 兼容的 Chat Completions 接口：
      POST {base_url}/v1/chat/completions
    OpenAI 与 DeepSeek 均可通过该方式调用（取决于 base_url 与 api_key）。
    """

    def __init__(self, cfg: LLMConfig):
        self._cfg = cfg

    @property
    def cfg(self) -> LLMConfig:
        return self._cfg

    def chat(
        self,
        messages: List[ChatMessage],
        *,
        temperature: Optional[float] = None,
        extra_body: Optional[Dict[str, Any]] = None,
    ) -> str:
        base = self._cfg.resolved_base_url().rstrip("/")
        url = f"{base}/v1/chat/completions"

        if not self._cfg.api_key.strip():
            raise LLMError(
                "未配置 API Key。请在 `agent/agent_config.yaml` 或环境变量中设置：\n"
                "- SIMPLECQ_LLM_API_KEY\n"
                "也可以复制 `agent/agent_config.example.yaml` 为 `agent/agent_config.yaml` 后填写。"
            )

        body: Dict[str, Any] = {
            "model": self._cfg.model,
            "messages": [{"role": m.role, "content": m.content} for m in messages],
            "temperature": self._cfg.temperature if temperature is None else temperature,
            "stream": False,
        }
        if extra_body:
            body.update(extra_body)

        data = json.dumps(body).encode("utf-8")
        headers = {
            "Content-Type": "application/json",
            "Authorization": f"Bearer {self._cfg.api_key}",
        }

        req = request.Request(url=url, data=data, headers=headers, method="POST")
        try:
            with request.urlopen(req, timeout=self._cfg.timeout_seconds) as resp:
                raw = resp.read().decode("utf-8", errors="replace")
        except error.HTTPError as e:
            raw = e.read().decode("utf-8", errors="replace") if hasattr(e, "read") else str(e)
            raise LLMError(
                "LLM 请求失败（HTTPError）。\n"
                f"- url: {url}\n"
                f"- status: {getattr(e, 'code', 'unknown')}\n"
                f"- body: {raw[:2000]}"
            )
        except Exception as e:
            raise LLMError(f"LLM 请求失败：{e}")

        try:
            obj = json.loads(raw)
        except Exception:
            raise LLMError(f"LLM 返回内容不是合法 JSON：{raw[:2000]}")

        # 标准格式：choices[0].message.content
        try:
            choices = obj["choices"]
            msg = choices[0]["message"]["content"]
            if not isinstance(msg, str):
                raise TypeError("content is not string")
            return msg.strip()
        except Exception:
            raise LLMError(f"无法从返回中解析 content。返回片段：{raw[:2000]}")

