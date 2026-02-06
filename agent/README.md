# SimpleCQ AI 助手（agent/）

该目录提供一个**可配置 OpenAI / DeepSeek** 的对话式助手，用于：

- 平台整体说明与架构梳理
- 平台使用指导（如何写/改 `scene*.yml`、如何跑 `tinycq.exe`、如何接 Python 训练脚本）
- 两类关键文件的格式说明与自动检查
  - **组合关系**：`assemble.yml`
  - **交互关系**：`scene*.yml`
- 报错/日志分析（即使不配置 LLM 也能做启发式分析）

## 安装

仅依赖 PyYAML：

```bash
pip install -r agent/requirements.txt
```

## 配置（OpenAI / DeepSeek）

复制示例配置：

- 将 `agent/agent_config.example.yaml` 复制为 `agent/agent_config.yaml`
- 填写 `llm.api_key`
- `llm.provider` 选 `openai` 或 `deepseek`
- `llm.base_url` 不填会用默认：
  - openai: `https://api.openai.com`
  - deepseek: `https://api.deepseek.com`

或使用环境变量覆盖：

- `SIMPLECQ_LLM_PROVIDER`
- `SIMPLECQ_LLM_API_KEY`
- `SIMPLECQ_LLM_MODEL`
- `SIMPLECQ_LLM_BASE_URL`

