from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List, Optional, Set

from .common import (
    ValidationIssue,
    load_yaml,
    _expect_bool,
    _expect_list,
    _expect_mapping,
    _expect_str,
    summarize_issues,
    warn_if_path_missing,
)


def _check_name_convert_items(
    issues: List[ValidationIssue],
    items: Any,
    where: str,
) -> None:
    lst = _expect_list(issues, items, where)
    if lst is None:
        return
    for i, it in enumerate(lst):
        if it is None:
            issues.append(ValidationIssue("WARN", "存在空条目（可能是多写了一个 '-'）", f"{where}[{i}]"))
            continue
        mp = _expect_mapping(issues, it, f"{where}[{i}]")
        if mp is None:
            continue
        has_name = "name" in mp
        has_src = "src_name" in mp
        has_dst = "dst_name" in mp
        if has_name and (has_src or has_dst):
            issues.append(ValidationIssue("ERROR", "name 与 src_name/dst_name 不能同时出现", f"{where}[{i}]"))
        if has_name:
            _expect_str(issues, mp.get("name"), f"{where}[{i}].name")
        else:
            if not (has_src and has_dst):
                issues.append(ValidationIssue("ERROR", "必须包含 name 或同时包含 src_name+dst_name", f"{where}[{i}]"))
            else:
                _expect_str(issues, mp.get("src_name"), f"{where}[{i}].src_name")
                _expect_str(issues, mp.get("dst_name"), f"{where}[{i}].dst_name")


def check_assemble_file(path: str | Path) -> List[ValidationIssue]:
    """
    组合关系文件：assemble.yml
    参考样例：config/file_simplecq/assemble.yml
    """
    p = Path(path)
    issues: List[ValidationIssue] = []
    if not p.exists():
        return [ValidationIssue("ERROR", "文件不存在", str(p))]

    obj = load_yaml(p)
    top = _expect_mapping(issues, obj, "root")
    if top is None:
        return issues

    # config
    cfg = top.get("config")
    cfg_mp = _expect_mapping(issues, cfg, "config")
    if cfg_mp is not None:
        # 常见字段做弱校验
        if "profile" in cfg_mp:
            prof = _expect_str(issues, cfg_mp.get("profile"), "config.profile")
            if prof:
                warn_if_path_missing(issues, prof, "config.profile")
        if "restart_key" in cfg_mp:
            _expect_str(issues, cfg_mp.get("restart_key"), "config.restart_key")
        if "side_filter" in cfg_mp:
            _expect_str(issues, cfg_mp.get("side_filter"), "config.side_filter")

    # models
    models = _expect_list(issues, top.get("models"), "models")
    model_names: Set[str] = set()
    if models is not None:
        for i, m in enumerate(models):
            mp = _expect_mapping(issues, m, f"models[{i}]")
            if mp is None:
                continue
            dll = _expect_str(issues, mp.get("dll_name"), f"models[{i}].dll_name")
            name = _expect_str(issues, mp.get("model_name"), f"models[{i}].model_name")
            _expect_bool(issues, mp.get("output_movable"), f"models[{i}].output_movable")
            if dll:
                warn_if_path_missing(issues, dll, f"models[{i}].dll_name")
            if name:
                if name in model_names:
                    issues.append(ValidationIssue("WARN", f"model_name 重复：{name}", f"models[{i}].model_name"))
                model_names.add(name)

    # init_convert / input_convert / output_convert
    for section in ["init_convert", "input_convert", "output_convert"]:
        sec = top.get(section)
        lst = _expect_list(issues, sec, section)
        if lst is None:
            continue
        for i, item in enumerate(lst):
            mp = _expect_mapping(issues, item, f"{section}[{i}]")
            if mp is None:
                continue

            if section == "output_convert":
                # 允许只有 from（用于声明标准输出字段），也允许 from+to（用于字段映射）
                if "from" in mp:
                    frm = _expect_str(issues, mp.get("from"), f"{section}[{i}].from")
                    if frm and model_names and frm not in model_names:
                        issues.append(ValidationIssue("WARN", f"from 未在 models.model_name 中出现：{frm}", f"{section}[{i}].from"))
                else:
                    issues.append(ValidationIssue("ERROR", "缺少 from", f"{section}[{i}]"))
                if "to" in mp:
                    to = _expect_str(issues, mp.get("to"), f"{section}[{i}].to")
                    if to and model_names and to not in model_names:
                        issues.append(ValidationIssue("WARN", f"to 未在 models.model_name 中出现：{to}", f"{section}[{i}].to"))
            else:
                to = _expect_str(issues, mp.get("to"), f"{section}[{i}].to")
                if to and model_names and to not in model_names:
                    issues.append(ValidationIssue("WARN", f"to 未在 models.model_name 中出现：{to}", f"{section}[{i}].to"))

            values = mp.get("values")
            _check_name_convert_items(issues, values, f"{section}[{i}].values")

    return issues


def check_assemble_file_text(path: str | Path) -> str:
    return summarize_issues(check_assemble_file(path))

