#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Evaluation harness for the v8-understanding skill and docs/ accuracy.

Loads declarative *.promptfoo.yaml test cases and .patch bug-injection fixtures
from agents/prompts/eval/v8-understanding/, sets up isolated worktrees on a
parentless root commit (with optional --docs-ref to compare Before-Docs vs.
After-Docs vs. No-Docs without git history leaks), and grades both fix
correctness (via agents/testing/asserts/check_changes.py) and agent exploration
efficiency (from transcript.jsonl).
"""

from __future__ import annotations

import argparse
import contextlib
from dataclasses import asdict
from dataclasses import dataclass
from dataclasses import field
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import yaml

REPO_ROOT = Path(
    os.environ.get("V8_UNDERSTANDING_EVAL_REPO_ROOT",
                   Path(__file__).resolve().parents[4]))
DEFAULT_EVAL_DIR = Path(
    os.environ.get(
        "V8_UNDERSTANDING_EVAL_DIR",
        REPO_ROOT / "agents" / "prompts" / "eval" / "v8-understanding"))
DEFAULT_WORKTREE_ROOT = REPO_ROOT / "worktrees"
META_DIRNAME = ".v8_understanding_eval_meta"
GIT_HISTORY_RE = re.compile(
    r"\bgit\b[^\n;&|]*\b(log|show|blame|reflog|whatchanged)\b")
DOCS_CMD_RE = re.compile(r"(?:<WT>/|\s|\"|')docs(?:/|\s|\"|'|$)")

sys.path.insert(0, str(REPO_ROOT / "agents" / "testing" / "asserts"))
import check_changes  # pylint: disable=wrong-import-position


@dataclass(frozen=True)
class EvalCase:
  case_id: str
  yaml_path: str
  category: str
  description: str
  prompt_text: str
  patches: list[str] = field(default_factory=list)
  assertions: list[dict] = field(default_factory=list)
  must_modify: list[str] = field(default_factory=list)


@contextlib.contextmanager
def _chdir(path: Path):
  prev = Path.cwd()
  os.chdir(path)
  try:
    yield
  finally:
    os.chdir(prev)


def load_eval_cases(
    eval_dir: Path = DEFAULT_EVAL_DIR,
    repo_root: Path = REPO_ROOT,
) -> dict[str, EvalCase]:
  cases: dict[str, EvalCase] = {}
  if not eval_dir.is_dir():
    return cases
  for yaml_file in sorted(eval_dir.glob("*.promptfoo.yaml")):
    data = yaml.safe_load(yaml_file.read_text(encoding="utf-8"))
    stem = yaml_file.stem.removesuffix(".promptfoo")
    case_id = stem.replace("_", "-")

    prompt_text = ""
    prompts = data.get("prompts", [])
    if prompts and isinstance(prompts[0], str):
      raw_prompt = prompts[0]
      if raw_prompt.startswith("file://"):
        prompt_path = yaml_file.parent / raw_prompt.removeprefix("file://")
        prompt_text = prompt_path.read_text(encoding="utf-8").strip()
      else:
        prompt_text = raw_prompt.strip()

    patches: list[str] = []
    for provider in data.get("providers", []):
      config = provider.get("config") or {}
      for change in config.get("changes", []):
        if "apply" in change:
          patches.append(change["apply"].removeprefix("file://"))

    test_entry = (data.get("tests") or [{}])[0]
    assertions = test_entry.get("assert", [])
    metadata = test_entry.get("metadata") or {}

    must_modify: list[str] = []
    for assertion in assertions:
      val = assertion.get("value", "")
      files = (assertion.get("config") or {}).get("files", [])
      if val.endswith(":check_files_changed"):
        must_modify.extend(files)

    try:
      rel_yaml = str(yaml_file.relative_to(repo_root))
    except ValueError:
      rel_yaml = str(yaml_file)

    cases[case_id] = EvalCase(
        case_id=case_id,
        yaml_path=rel_yaml,
        category=metadata.get("category", "general"),
        description=str(data.get("description", "")).strip(),
        prompt_text=prompt_text,
        patches=patches,
        assertions=assertions,
        must_modify=must_modify,
    )
  return cases


EVAL_CASES: dict[str, EvalCase] = load_eval_cases()


def _run_git(
    cwd: Path,
    args: list[str],
    check: bool = True,
) -> subprocess.CompletedProcess[str]:
  return subprocess.run(
      ["git", "--no-pager", *args],
      cwd=cwd,
      capture_output=True,
      text=True,
      check=check,
  )


def _strip_docs_instructions(prompt_text: str) -> str:
  text = re.sub(
      r"^Use the `v8-understanding` skill to investigate and fix ",
      "Investigate and fix ",
      prompt_text,
  )
  text = re.sub(
      r"Consult `docs/` first to [^,]+,\s*and\s*",
      "Identify all affected places and ",
      text,
  )
  return text


def _add_docs_instructions(prompt_text: str) -> str:
  if "v8-understanding" in prompt_text:
    return prompt_text
  return ("Use the `v8-understanding` skill and consult `docs/` first to "
          "understand the subsystem invariants before reading C++ files in "
          "`src/`.\n\n" + prompt_text)


def build_agent_prompt(
    case: EvalCase,
    worktree_dir: Path,
    docs_ref: str | None = None,
    prompt_docs: bool = False,
) -> str:
  if docs_ref == "none":
    isolation_header = (
        f"Task (working directory `{worktree_dir}`): "
        f"{_strip_docs_instructions(case.prompt_text)} "
        f"Constraints: every `run_command` shell starts in a different "
        f"checkout, so prefix EVERY `run_command` with "
        f"`cd {worktree_dir} && ` and use `{worktree_dir}/...` for all "
        f"codebase files; do not access other checkouts, memory files, "
        f"conversation transcripts, `v8-understanding` skill files, `docs/`, "
        f"git history commands (`git log`, `git show`, `git blame`, "
        f"`git reflog`), or `remote.py`.")
    return isolation_header

  body = (
      _add_docs_instructions(case.prompt_text)
      if prompt_docs else case.prompt_text)
  return (
      f"Task (working directory `{worktree_dir}`): {body} "
      f"Constraints: every `run_command` shell starts in a different "
      f"checkout, so prefix EVERY `run_command` with `cd {worktree_dir} && ` "
      f"and use `{worktree_dir}/...` for all codebase files; do not access "
      f"other checkouts, memory files, conversation transcripts, git history "
      f"commands (`git log`, `git show`, `git blame`, `git reflog`), or "
      f"`remote.py`.")


def _case_slug(case_id: str) -> str:
  norm_id = case_id.replace("_", "-")
  return hashlib.sha1(norm_id.encode("utf-8")).hexdigest()[:8]


def _branch_names_for_case(
    case_id: str,
    worktree_root: Path,
    repo_root: Path = REPO_ROOT,
) -> tuple[str, str]:
  slug = _case_slug(case_id)
  if worktree_root.resolve() == (repo_root / "worktrees").resolve():
    return f"u-base-{slug}", f"u-branch-{slug}"
  root_slug = hashlib.sha1(str(
      worktree_root.resolve()).encode("utf-8")).hexdigest()[:6]
  return f"u-base-{root_slug}-{slug}", f"u-branch-{root_slug}-{slug}"


def _worktree_dir_for_case(case_id: str, worktree_root: Path) -> Path:
  slug = _case_slug(case_id)
  return (worktree_root / f"wt-u-{slug}").resolve()


def _meta_path_for_case(case_id: str, worktree_root: Path) -> Path:
  norm_id = case_id.replace("_", "-")
  return (worktree_root / META_DIRNAME / f"{norm_id}.json").resolve()


def _resolve_patch_path(patch_rel: str, repo_root: Path) -> Path:
  for candidate in (
      (repo_root / patch_rel).resolve(),
      (REPO_ROOT / patch_rel).resolve(),
      (DEFAULT_EVAL_DIR / Path(patch_rel).name).resolve(),
  ):
    if candidate.is_file():
      return candidate
  raise FileNotFoundError(f"Patch file not found: {patch_rel}")


def setup_case(
    case: EvalCase,
    worktree_root: Path,
    repo_root: Path = REPO_ROOT,
    docs_ref: str | None = None,
    prompt_docs: bool = False,
) -> dict[str, object]:
  worktree_dir = _worktree_dir_for_case(case.case_id, worktree_root)
  base_branch, test_branch = _branch_names_for_case(
      case.case_id, worktree_root, repo_root=repo_root)

  cleanup_case(case.case_id, worktree_root, repo_root=repo_root)
  worktree_root.mkdir(parents=True, exist_ok=True)

  _run_git(
      repo_root,
      ["worktree", "add", "-f", "-B", base_branch,
       str(worktree_dir), "HEAD"],
  )

  if docs_ref == "none":
    _run_git(
        worktree_dir,
        [
            "rm",
            "-rf",
            "--quiet",
            "docs",
            "agents/skills/v8-understanding",
            ".agents/skills/v8-understanding",
        ],
        check=False,
    )
    shutil.rmtree(worktree_dir / "docs", ignore_errors=True)
    shutil.rmtree(
        worktree_dir / "agents" / "skills" / "v8-understanding",
        ignore_errors=True,
    )
    (worktree_dir / ".agents" / "skills" /
     "v8-understanding").unlink(missing_ok=True)
  elif docs_ref:
    _run_git(worktree_dir, ["rm", "-rf", "--quiet", "docs"], check=False)
    _run_git(worktree_dir, ["checkout", docs_ref, "--", "docs"])

  for patch_rel in case.patches:
    patch_path = _resolve_patch_path(patch_rel, repo_root)
    _run_git(worktree_dir, ["apply", str(patch_path)])

  # Strip any v8-understanding eval fixtures from the worktree so the agent
  # cannot discover test definitions inside the checkout.
  for eval_rel in (
      "agents/prompts/eval/v8-understanding",
      "agents/skills/v8-understanding/scripts",
  ):
    target_eval = worktree_dir / eval_rel
    if target_eval.exists():
      shutil.rmtree(target_eval, ignore_errors=True)

  _run_git(worktree_dir, ["add", "-A"])
  tree_sha = _run_git(worktree_dir, ["write-tree"]).stdout.strip()
  root_commit = _run_git(
      worktree_dir,
      [
          "-c",
          "user.name=Developer",
          "-c",
          "user.email=dev@chromium.org",
          "commit-tree",
          tree_sha,
          "-m",
          "V8 baseline",
      ],
  ).stdout.strip()

  _run_git(worktree_dir, ["checkout", "-B", base_branch, root_commit])
  _run_git(worktree_dir, ["checkout", "-B", test_branch, root_commit])
  _run_git(worktree_dir,
           ["branch", f"--set-upstream-to={base_branch}", test_branch])

  install_script = worktree_dir / "agents" / "scripts" / "install_for_jetski.py"
  if install_script.is_file():
    subprocess.run(
        [sys.executable, str(install_script)],
        cwd=worktree_dir,
        capture_output=True,
        text=True,
        check=True,
    )

  prompt = build_agent_prompt(
      case, worktree_dir, docs_ref=docs_ref, prompt_docs=prompt_docs)
  meta: dict[str, object] = {
      "case_id": case.case_id,
      "yaml_path": case.yaml_path,
      "category": case.category,
      "docs_ref": docs_ref or "HEAD",
      "prompt_docs": prompt_docs,
      "base_commit": root_commit,
      "test_commit": root_commit,
      "base_branch": base_branch,
      "test_branch": test_branch,
      "worktree_dir": str(worktree_dir),
      "prompt": prompt,
  }
  meta_path = _meta_path_for_case(case.case_id, worktree_root)
  meta_path.parent.mkdir(parents=True, exist_ok=True)
  meta_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
  return meta


def _is_own_generated_step_output(
    path_arg: str,
    transcript_path: Path,
) -> bool:
  sys_gen_dir = transcript_path.resolve().parent.parent
  if sys_gen_dir.name != ".system_generated":
    return False
  steps_dir = str((sys_gen_dir / "steps").resolve())
  try:
    resolved = str(Path(path_arg).resolve())
  except OSError:
    return False
  return (resolved.startswith(steps_dir + "/") and
          resolved.endswith("/output.txt"))


def _is_allowed_external_skill_or_rule(
    path_arg: str,
    allow_understanding_skill: bool,
) -> bool:
  if any(
      bad in path_arg for bad in (
          "/memory/",
          "/brain/",
          "/eval/",
          "/scripts/",
          "eval_v8_understanding",
      )):
    return False
  if not allow_understanding_skill and "v8-understanding" in path_arg:
    return False
  if path_arg.startswith(
      "/google/src/files/head/depot/google3/learning/gemini/agents/skills/"):
    return True
  if "/.gemini/" in path_arg and ("/skills/" in path_arg or
                                  "/rules/" in path_arg):
    return True
  return any(
      seg in path_arg for seg in (
          "/.agents/skills/",
          "/agents/skills/",
          "/.agents/rules/",
          "/agents/rules/",
      ))


def parse_transcript_metrics(
    transcript_path: Path,
    worktree_dir: Path,
    allow_understanding_skill: bool = True,
) -> dict[str, object]:
  steps = [
      json.loads(line)
      for line in transcript_path.read_text(encoding="utf-8").splitlines()
      if line.strip()
  ]
  planner_steps = [s for s in steps if s.get("type") == "PLANNER_RESPONSE"]
  turns = len(planner_steps)
  tool_calls = 0
  skill_reads = 0
  docs_reads = 0
  docs_greps = 0
  src_reads = 0
  shell_calls = 0
  external_reads: list[str] = []
  git_history_calls: list[str] = []
  unscoped_shell_calls: list[str] = []

  wt_str = str(worktree_dir.resolve())

  for step in planner_steps:
    for tc in step.get("tool_calls") or []:
      tool_calls += 1
      name = tc.get("name", "")
      args = tc.get("args") or {}
      if name == "read_file":
        path_arg = str(args.get("Path", "")).strip('"')
        if "v8-understanding/SKILL.md" in path_arg:
          if allow_understanding_skill:
            skill_reads += 1
          else:
            external_reads.append(path_arg)
            continue
        if path_arg.startswith(wt_str + "/docs/"):
          docs_reads += 1
        elif path_arg.startswith(wt_str + "/src/"):
          src_reads += 1
        elif (path_arg != wt_str and not path_arg.startswith(wt_str + "/") and
              not _is_own_generated_step_output(path_arg, transcript_path) and
              not _is_allowed_external_skill_or_rule(
                  path_arg, allow_understanding_skill)):
          external_reads.append(path_arg)
      elif name == "run_command":
        shell_calls += 1
        cmd = str(args.get("CommandLine", "")).strip('"')
        sanitized_cmd = cmd.replace(wt_str, "<WT>")
        if wt_str not in cmd or str(REPO_ROOT.resolve()) in sanitized_cmd:
          unscoped_shell_calls.append(cmd)
        if DOCS_CMD_RE.search(sanitized_cmd):
          docs_greps += 1
        if GIT_HISTORY_RE.search(sanitized_cmd):
          git_history_calls.append(cmd)

  wall_time_sec = 0.0
  if len(planner_steps) >= 2:
    t0_str = planner_steps[0].get("created_at", "")
    t1_str = planner_steps[-1].get("created_at", "")
    if t0_str and t1_str:
      t0 = datetime.fromisoformat(t0_str.replace("Z", "+00:00"))
      t1 = datetime.fromisoformat(t1_str.replace("Z", "+00:00"))
      wall_time_sec = round((t1 - t0).total_seconds(), 1)

  return {
      "turns": turns,
      "tool_calls": tool_calls,
      "skill_reads": skill_reads,
      "docs_reads": docs_reads,
      "docs_greps": docs_greps,
      "src_reads": src_reads,
      "shell_calls": shell_calls,
      "wall_time_sec": wall_time_sec,
      "external_reads": external_reads,
      "git_history_calls": git_history_calls,
      "unscoped_shell_calls": unscoped_shell_calls,
  }


def _run_promptfoo_assertions(
    case: EvalCase,
    worktree_dir: Path,
) -> list[str]:
  failures: list[str] = []
  with _chdir(worktree_dir):
    for assertion in case.assertions:
      if assertion.get("type") != "python":
        continue
      value = assertion.get("value", "")
      if ":" not in value:
        continue
      func_name = value.rsplit(":", 1)[1]
      fn = getattr(check_changes, func_name, None) or globals().get(func_name)
      if fn is None:
        failures.append(f"Unknown check_changes assertion: `{func_name}`")
        continue
      res = fn("", {"config": assertion.get("config", {})})
      if not res.get("pass", False):
        failures.append(res.get("reason", f"Assertion `{func_name}` failed."))
  return failures


def grade_case(
    case: EvalCase,
    worktree_dir: Path,
    transcript_path: Path | None = None,
) -> dict[str, object]:
  meta_path = _meta_path_for_case(case.case_id, worktree_dir.parent)
  if not meta_path.is_file():
    raise FileNotFoundError(
        f"Metadata file {meta_path} not found. Run `setup` first.")
  meta = json.loads(meta_path.read_text(encoding="utf-8"))
  base_commit = meta["base_commit"]
  test_commit = meta.get("test_commit", base_commit)

  head_commit = _run_git(worktree_dir, ["rev-parse", "HEAD"]).stdout.strip()
  if head_commit != test_commit:
    _run_git(worktree_dir, ["reset", "--soft", test_commit])

  diff_files_res = _run_git(worktree_dir,
                            ["diff", "--name-only", base_commit, "--", "src/"])
  modified_src = sorted({
      line.strip()
      for line in diff_files_res.stdout.splitlines()
      if line.strip()
  })

  failures = _run_promptfoo_assertions(case, worktree_dir)

  metrics: dict[str, object] = {}
  if transcript_path is not None and transcript_path.is_file():
    metrics = parse_transcript_metrics(
        transcript_path,
        worktree_dir,
        allow_understanding_skill=(meta.get("docs_ref") != "none"),
    )
    if meta.get("docs_ref") == "none" and (metrics.get("docs_reads", 0) > 0 or
                                           metrics.get("docs_greps", 0) > 0 or
                                           metrics.get("skill_reads", 0) > 0):
      failures.append(f"Used docs/ or v8-understanding in no-docs mode: "
                      f"docs_reads={metrics.get('docs_reads')}, "
                      f"docs_greps={metrics.get('docs_greps')}, "
                      f"skill_reads={metrics.get('skill_reads')}")
    if metrics.get("external_reads"):
      failures.append(
          f"Read files outside worktree: {metrics['external_reads']}")
    if metrics.get("git_history_calls"):
      failures.append(
          f"Used disallowed git history commands: {metrics['git_history_calls']}"
      )
    if metrics.get("unscoped_shell_calls"):
      failures.append(f"Ran shell commands without worktree path: "
                      f"{metrics['unscoped_shell_calls']}")

  return {
      "case_id": case.case_id,
      "yaml_path": case.yaml_path,
      "category": case.category,
      "docs_ref": meta.get("docs_ref", "HEAD"),
      "prompt_docs": meta.get("prompt_docs", False),
      "passed": not failures,
      "modified_src": modified_src,
      "metrics": metrics,
      "failures": failures,
  }


def cleanup_case(
    case_id: str,
    worktree_root: Path,
    repo_root: Path = REPO_ROOT,
) -> None:
  norm_id = case_id.replace("_", "-")
  worktree_dir = _worktree_dir_for_case(norm_id, worktree_root)
  base_branch, test_branch = _branch_names_for_case(
      norm_id, worktree_root, repo_root=repo_root)
  if worktree_dir.exists():
    _run_git(
        repo_root,
        ["worktree", "remove", "--force",
         str(worktree_dir)],
        check=False,
    )
    if worktree_dir.exists():
      shutil.rmtree(worktree_dir, ignore_errors=True)
  meta_path = _meta_path_for_case(norm_id, worktree_root)
  if meta_path.is_file():
    meta_path.unlink()
  _run_git(repo_root, ["worktree", "prune"], check=False)
  for branch in (test_branch, base_branch):
    _run_git(repo_root, ["branch", "-D", branch], check=False)


def _resolve_cases(case_arg: str) -> list[EvalCase]:
  if case_arg == "all":
    return list(EVAL_CASES.values())
  norm_id = case_arg.replace("_", "-")
  if norm_id not in EVAL_CASES:
    valid = ", ".join(["all", *EVAL_CASES.keys()])
    raise ValueError(f"Unknown case `{case_arg}`. Valid choices: {valid}")
  return [EVAL_CASES[norm_id]]


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(
      description="Evaluation harness for V8 v8-understanding skill and docs/.")
  subparsers = parser.add_subparsers(dest="subcommand", required=True)

  list_parser = subparsers.add_parser("list", help="List available eval cases.")
  list_parser.add_argument("--json", action="store_true")

  setup_parser = subparsers.add_parser(
      "setup", help="Create isolated worktree(s) with injected bug fixture(s).")
  setup_parser.add_argument("--case", required=True, help="Case ID or `all`.")
  setup_parser.add_argument(
      "--docs-ref",
      default=None,
      help="Optional git ref from which to checkout docs/ (e.g. origin/main).",
  )
  setup_parser.add_argument(
      "--prompt-docs",
      action="store_true",
      help=("Explicitly instruct the agent in the prompt to use "
            "`v8-understanding` and consult `docs/` first."),
  )
  setup_parser.add_argument(
      "--worktree-root", type=Path, default=DEFAULT_WORKTREE_ROOT)
  setup_parser.add_argument("--json", action="store_true")

  grade_parser = subparsers.add_parser(
      "grade", help="Grade src/ fixes and transcript metrics in worktree(s).")
  grade_parser.add_argument("--case", required=True, help="Case ID or `all`.")
  grade_parser.add_argument(
      "--worktree-root", type=Path, default=DEFAULT_WORKTREE_ROOT)
  grade_parser.add_argument(
      "--transcript",
      type=Path,
      default=None,
      help="Optional path to subagent transcript.jsonl for efficiency metrics.",
  )
  grade_parser.add_argument("--json", action="store_true")

  cleanup_parser = subparsers.add_parser(
      "cleanup", help="Remove temporary eval worktree(s) and branches.")
  cleanup_parser.add_argument("--case", required=True, help="Case ID or `all`.")
  cleanup_parser.add_argument(
      "--worktree-root", type=Path, default=DEFAULT_WORKTREE_ROOT)

  args = parser.parse_args(argv)

  if args.subcommand == "list":
    if args.json:
      print(json.dumps([asdict(c) for c in EVAL_CASES.values()], indent=2))
    else:
      for case in EVAL_CASES.values():
        print(f"- {case.case_id} [{case.category}] ({case.yaml_path}): "
              f"{case.description}")
    return 0

  cases = _resolve_cases(args.case)

  if args.subcommand == "setup":
    metas = [
        setup_case(
            c,
            args.worktree_root,
            docs_ref=args.docs_ref,
            prompt_docs=args.prompt_docs,
        ) for c in cases
    ]
    if args.json:
      print(json.dumps(metas, indent=2))
    else:
      for meta in metas:
        print("=" * 80)
        print(f"CASE: {meta['case_id']} ({meta['category']})")
        print(
            f"DOCS_REF: {meta['docs_ref']} (prompt_docs={meta['prompt_docs']})")
        print(f"WORKTREE: {meta['worktree_dir']}")
        print("PROMPT:")
        print(meta["prompt"])
        print("=" * 80)
    return 0

  if args.subcommand == "grade":
    results = []
    all_passed = True
    for case in cases:
      wt_dir = _worktree_dir_for_case(case.case_id, args.worktree_root)
      res = grade_case(case, wt_dir, transcript_path=args.transcript)
      results.append(res)
      if not res["passed"]:
        all_passed = False
    if args.json:
      print(json.dumps(results, indent=2))
    else:
      passed_count = sum(1 for r in results if r["passed"])
      print(f"EVAL SUMMARY: {passed_count}/{len(results)} cases passed.")
      for res in results:
        status = "PASS" if res["passed"] else "FAIL"
        print(f"  [{status}] {res['case_id']} (docs={res['docs_ref']}) -> "
              f"modified_src={res['modified_src']} metrics={res['metrics']}")
        for failure in res["failures"]:
          print(f"         - {failure}")
    return 0 if all_passed else 1

  if args.subcommand == "cleanup":
    for case in cases:
      cleanup_case(case.case_id, args.worktree_root)
    print(f"Cleaned up {len(cases)} case(s) under {args.worktree_root}")
    return 0

  return 1


if __name__ == "__main__":
  sys.exit(main())
