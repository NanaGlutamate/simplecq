from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, List, Optional, Set

from .common import (
    ValidationIssue,
    load_yaml,
    _expect_bool,
    _expect_int,
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
    *,
    members: Optional[Set[str]] = None,
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
            name = _expect_str(issues, mp.get("name"), f"{where}[{i}].name")
            if members is not None and name and name not in members:
                issues.append(ValidationIssue("WARN", f"name 不在 members 中：{name}", f"{where}[{i}].name"))
        else:
            if not (has_src and has_dst):
                issues.append(ValidationIssue("ERROR", "必须包含 name 或同时包含 src_name+dst_name", f"{where}[{i}]"))
            else:
                src = _expect_str(issues, mp.get("src_name"), f"{where}[{i}].src_name")
                _expect_str(issues, mp.get("dst_name"), f"{where}[{i}].dst_name")
                if members is not None and src and src not in members:
                    issues.append(ValidationIssue("WARN", f"src_name 不在 members 中：{src}", f"{where}[{i}].src_name"))


def check_scene_file(path: str | Path) -> List[ValidationIssue]:
    """
    交互关系文件：scene*.yml
    参考样例：config/file_simplecq/scene_planebattle.yml
    """
    p = Path(path)
    issues: List[ValidationIssue] = []
    if not p.exists():
        return [ValidationIssue("ERROR", "文件不存在", str(p))]

    obj = load_yaml(p)
    top = _expect_mapping(issues, obj, "root")
    if top is None:
        return issues

    # model_types
    model_types = _expect_list(issues, top.get("model_types"), "model_types")
    model_type_names: Set[str] = set()
    if model_types is not None:
        for i, mt in enumerate(model_types):
            mp = _expect_mapping(issues, mt, f"model_types[{i}]")
            if mp is None:
                continue
            name = _expect_str(issues, mp.get("model_type_name"), f"model_types[{i}].model_type_name")
            dll = _expect_str(issues, mp.get("dll_path"), f"model_types[{i}].dll_path")
            _expect_bool(issues, mp.get("output_movable"), f"model_types[{i}].output_movable")
            if dll:
                warn_if_path_missing(issues, dll, f"model_types[{i}].dll_path")
            if name:
                if name in model_type_names:
                    issues.append(ValidationIssue("ERROR", f"model_type_name 重复：{name}", f"model_types[{i}].model_type_name"))
                model_type_names.add(name)

    # models
    models = _expect_list(issues, top.get("models"), "models")
    if models is not None:
        for i, m in enumerate(models):
            mp = _expect_mapping(issues, m, f"models[{i}]")
            if mp is None:
                continue
            mt = _expect_str(issues, mp.get("model_type"), f"models[{i}].model_type")
            if mt and model_type_names and mt not in model_type_names:
                issues.append(ValidationIssue("ERROR", f"model_type 未在 model_types 中定义：{mt}", f"models[{i}].model_type"))
            _expect_int(issues, mp.get("side_id"), f"models[{i}].side_id")
            _expect_int(issues, mp.get("id"), f"models[{i}].id")
            init_val = _expect_str(issues, mp.get("init_value"), f"models[{i}].init_value")
            if init_val:
                if "<c>" not in init_val or "</c>" not in init_val:
                    issues.append(ValidationIssue("WARN", "init_value 看起来不像 <c>...</c> 的 XML 串（仅提醒）", f"models[{i}].init_value"))

    # topics
    topics = _expect_list(issues, top.get("topics"), "topics")
    if topics is not None:
        for i, t in enumerate(topics):
            mp = _expect_mapping(issues, t, f"topics[{i}]")
            if mp is None:
                continue

            frm = _expect_str(issues, mp.get("from"), f"topics[{i}].from")
            if frm and model_type_names and frm not in model_type_names and frm != "root":
                issues.append(ValidationIssue("WARN", f"from 未在 model_types 中定义（或不是 root）：{frm}", f"topics[{i}].from"))

            members_raw = mp.get("members")
            members_list = _expect_list(issues, members_raw, f"topics[{i}].members")
            members: Set[str] = set()
            if members_list is not None:
                for j, mem in enumerate(members_list):
                    s = _expect_str(issues, mem, f"topics[{i}].members[{j}]")
                    if s:
                        if s in members:
                            issues.append(ValidationIssue("WARN", f"members 重复：{s}", f"topics[{i}].members[{j}]"))
                        members.add(s)

            subs = _expect_list(issues, mp.get("subscribers"), f"topics[{i}].subscribers")
            if subs is None:
                continue
            for j, sub in enumerate(subs):
                smp = _expect_mapping(issues, sub, f"topics[{i}].subscribers[{j}]")
                if smp is None:
                    continue
                to = _expect_str(issues, smp.get("to"), f"topics[{i}].subscribers[{j}].to")
                if to and model_type_names and to not in model_type_names and to != "root":
                    issues.append(ValidationIssue("WARN", f"to 未在 model_types 中定义（或不是 root）：{to}", f"topics[{i}].subscribers[{j}].to"))
                _check_name_convert_items(
                    issues,
                    smp.get("name_convert"),
                    f"topics[{i}].subscribers[{j}].name_convert",
                    members=members if members else None,
                )

    return issues


def check_scene_file_text(path: str | Path) -> str:
    return summarize_issues(check_scene_file(path))

