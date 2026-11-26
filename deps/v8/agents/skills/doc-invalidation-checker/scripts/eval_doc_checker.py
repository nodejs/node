#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Evaluation harness for the doc-invalidation-checker skill and docs sync.

Loads declarative *.promptfoo.yaml test cases and .patch fixtures from
agents/prompts/eval/doc-invalidation-checker/ and evaluates worktrees using
agents/testing/asserts/check_changes.py plus markdown hygiene checks.
"""

from __future__ import annotations

import argparse
import contextlib
from dataclasses import asdict
from dataclasses import dataclass
from dataclasses import field
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
    os.environ.get("DOC_EVAL_REPO_ROOT",
                   Path(__file__).resolve().parents[4]))
DEFAULT_EVAL_DIR = Path(
    os.environ.get(
        "DOC_EVAL_DIR",
        REPO_ROOT / "agents" / "prompts" / "eval" / "doc-invalidation-checker"))
DEFAULT_WORKTREE_ROOT = REPO_ROOT / "worktrees"
META_DIRNAME = ".doc_eval_meta"
LINK_RE = re.compile(r"\[[^\]]+\]\(([^)#]+)(?:#[^)]*)?\)")

sys.path.insert(0, str(REPO_ROOT / "agents" / "testing" / "asserts"))
import check_changes  # pylint: disable=wrong-import-position


@dataclass(frozen=True)
class EvalCase:
  case_id: str
  yaml_path: str
  category: str
  description: str
  commit_message: str
  prompt_text: str
  patches: list[str] = field(default_factory=list)
  assertions: list[dict] = field(default_factory=list)
  must_modify: list[str] = field(default_factory=list)
  must_not_modify: list[str] = field(default_factory=list)
  allow_extra_docs: bool = False


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
    must_not_modify: list[str] = []
    for assertion in assertions:
      val = assertion.get("value", "")
      files = (assertion.get("config") or {}).get("files", [])
      if val.endswith(":check_files_changed"):
        must_modify.extend(files)
      elif val.endswith(":check_files_unchanged"):
        must_not_modify.extend(files)

    try:
      rel_yaml = str(yaml_file.relative_to(repo_root))
    except ValueError:
      rel_yaml = str(yaml_file)

    cases[case_id] = EvalCase(
        case_id=case_id,
        yaml_path=rel_yaml,
        category=metadata.get("category", "general"),
        description=str(data.get("description", "")).strip(),
        commit_message=str(metadata.get("commit_message",
                                        f"[eval] {case_id}")).strip(),
        prompt_text=prompt_text,
        patches=patches,
        assertions=assertions,
        must_modify=must_modify,
        must_not_modify=must_not_modify,
        allow_extra_docs=bool(metadata.get("allow_extra_docs", False)),
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


def build_agent_prompt(
    case: EvalCase,
    worktree_dir: Path,
    mode: str = "explicit",
) -> str:
  isolation_header = (
      f"Working directory: `{worktree_dir}` (run all shell and git commands "
      f"with cwd `{worktree_dir}` and resolve all file paths inside "
      f"`{worktree_dir}`; do not run `remote.py`).\n\n")
  if mode == "explicit":
    body = case.prompt_text or (
        "Run the `doc-invalidation-checker` skill on the changes in this "
        "checkout.")
    return isolation_header + body
  if mode == "implicit":
    return (isolation_header +
            "I just finished making a code change in this checkout and want "
            "to prepare it for upload (skipping C++ compilation and test "
            "execution). Follow V8's git-cl and v8-best-practices rules to "
            "run any required pre-upload checks and steps.")
  raise ValueError(f"Unsupported mode: {mode}")


def _case_slug(case_id: str) -> str:
  norm_id = case_id.replace("_", "-")
  return hashlib.sha1(norm_id.encode("utf-8")).hexdigest()[:8]


def _worktree_dir_for_case(case_id: str, worktree_root: Path) -> Path:
  slug = _case_slug(case_id)
  return (worktree_root / f"wt-{slug}").resolve()


def _meta_path_for_case(case_id: str, worktree_root: Path) -> Path:
  norm_id = case_id.replace("_", "-")
  return (worktree_root / META_DIRNAME / f"{norm_id}.json").resolve()


def setup_case(
    case: EvalCase,
    worktree_root: Path,
    repo_root: Path = REPO_ROOT,
    mode: str = "explicit",
    diff_kind: str = "committed",
) -> dict[str, str]:
  slug = _case_slug(case.case_id)
  worktree_dir = _worktree_dir_for_case(case.case_id, worktree_root)
  base_branch = f"cl-base-{slug}"
  test_branch = f"cl-branch-{slug}"

  cleanup_case(case.case_id, worktree_root, repo_root=repo_root)
  worktree_root.mkdir(parents=True, exist_ok=True)

  _run_git(
      repo_root,
      ["worktree", "add", "-f", "-B", base_branch,
       str(worktree_dir), "HEAD"],
  )

  base_commit = _run_git(worktree_dir, ["rev-parse", "HEAD"]).stdout.strip()

  _run_git(worktree_dir, ["checkout", "-B", test_branch, base_branch])
  _run_git(worktree_dir,
           ["branch", f"--set-upstream-to={base_branch}", test_branch])

  for patch_rel in case.patches:
    patch_path = (repo_root / patch_rel).resolve()
    if not patch_path.is_file():
      patch_path = (REPO_ROOT / patch_rel).resolve()
    if not patch_path.is_file():
      patch_path = (DEFAULT_EVAL_DIR / Path(patch_rel).name).resolve()
    _run_git(worktree_dir, ["apply", str(patch_path)])

  if case.patches:
    if diff_kind == "committed":
      _run_git(worktree_dir, ["add", "-A"])
      _run_git(
          worktree_dir,
          [
              "-c",
              "user.name=Developer",
              "-c",
              "user.email=dev@chromium.org",
              "commit",
              "-m",
              case.commit_message,
          ],
      )
    elif diff_kind != "uncommitted":
      raise ValueError(f"Unsupported diff_kind: {diff_kind}")

  test_commit = _run_git(worktree_dir, ["rev-parse", "HEAD"]).stdout.strip()
  install_script = worktree_dir / "agents" / "scripts" / "install_for_jetski.py"
  if install_script.is_file():
    subprocess.run(
        [sys.executable, str(install_script)],
        cwd=worktree_dir,
        capture_output=True,
        text=True,
        check=True,
    )

  prompt = build_agent_prompt(case, worktree_dir, mode=mode)
  meta = {
      "case_id": case.case_id,
      "yaml_path": case.yaml_path,
      "category": case.category,
      "mode": mode,
      "diff_kind": diff_kind,
      "base_commit": base_commit,
      "test_commit": test_commit,
      "base_branch": base_branch,
      "test_branch": test_branch,
      "worktree_dir": str(worktree_dir),
      "prompt": prompt,
  }
  meta_path = _meta_path_for_case(case.case_id, worktree_root)
  meta_path.parent.mkdir(parents=True, exist_ok=True)
  meta_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
  return meta


def _check_relative_links(
    worktree_dir: Path,
    modified_docs: list[str],
) -> list[str]:
  broken: list[str] = []
  for rel_doc in modified_docs:
    doc_path = worktree_dir / rel_doc
    if not doc_path.is_file():
      continue
    text = doc_path.read_text(encoding="utf-8")
    for match in LINK_RE.finditer(text):
      target = match.group(1).strip()
      if target.startswith(("http://", "https://", "mailto:", "file://")):
        continue
      resolved = (doc_path.parent / target).resolve()
      if not resolved.exists():
        broken.append(f"{rel_doc} -> {target}")
  return broken


def check_files_unchanged(_: str, context: dict) -> dict[str, object]:
  """Checks that specific files or directory prefixes have not been changed."""
  forbidden_paths = context.get("config", {}).get("files", [])
  file_statuses = check_changes._get_changed_files()  # pylint: disable=protected-access
  changed_forbidden: list[str] = []
  for forbidden in forbidden_paths:
    if forbidden.endswith("/"):
      changed_forbidden.extend(
          f for f in file_statuses if f.startswith(forbidden))
    elif forbidden in file_statuses:
      changed_forbidden.append(forbidden)

  if changed_forbidden:
    unexpected = "\n".join(sorted(set(changed_forbidden)))
    return {
        "pass":
            False,
        "reason":
            f"Expected files to remain unchanged, but modified:\n{unexpected}",
        "score":
            0,
    }
  return {
      "pass": True,
      "reason": "All specified files remained unchanged.",
      "score": 1,
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


def grade_case(case: EvalCase, worktree_dir: Path) -> dict[str, object]:
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
                            ["diff", "--name-only", base_commit, "--", "docs/"])
  modified_docs = sorted({
      line.strip()
      for line in diff_files_res.stdout.splitlines()
      if line.strip()
  })

  untracked_res = _run_git(
      worktree_dir,
      ["ls-files", "--others", "--exclude-standard", "--", "docs/"],
  )
  for line in untracked_res.stdout.splitlines():
    if line.strip() and line.strip() not in modified_docs:
      modified_docs.append(line.strip())
  modified_docs.sort()

  # 1. Run Promptfoo assertions from check_changes.py.
  failures = _run_promptfoo_assertions(case, worktree_dir)

  # 2. Extra doc churn check outside expected must_modify files (and
  # docs/README.md index when must_modify is non-empty).
  if not case.allow_extra_docs:
    allowed = set(case.must_modify)
    if case.must_modify:
      allowed.add("docs/README.md")
    unexpected = [d for d in modified_docs if d not in allowed]
    if unexpected:
      failures.append(
          f"Unexpected docs modified outside {sorted(allowed)}: {unexpected}")

  # 3. Hygiene checks: U+2014 em-dash, git diff --check, and relative links.
  for doc_rel in modified_docs:
    doc_path = worktree_dir / doc_rel
    if doc_path.is_file():
      content = doc_path.read_text(encoding="utf-8")
      if "\u2014" in content:
        failures.append(f"`{doc_rel}` contains Unicode em-dash (U+2014).")

  diff_check_res = _run_git(
      worktree_dir,
      ["diff", "--check", base_commit, "--", "docs/"],
      check=False,
  )
  if diff_check_res.returncode != 0:
    failures.append(
        f"`git diff --check` failed in docs/:\n{diff_check_res.stdout.strip()}")

  broken_links = _check_relative_links(worktree_dir, modified_docs)
  if broken_links:
    failures.append(f"Broken relative markdown links: {broken_links}")

  return {
      "case_id": case.case_id,
      "yaml_path": case.yaml_path,
      "category": case.category,
      "mode": meta.get("mode", "explicit"),
      "diff_kind": meta.get("diff_kind", "committed"),
      "passed": not failures,
      "modified_docs": modified_docs,
      "failures": failures,
  }


def check_case(case: EvalCase,
               repo_root: Path = REPO_ROOT) -> dict[str, object]:
  """Checks that a case's patches apply cleanly and doc targets exist."""
  failures: list[str] = []
  for patch_rel in case.patches:
    patch_path = (repo_root / patch_rel).resolve()
    if not patch_path.is_file():
      failures.append(f"Missing patch `{patch_rel}`.")
      continue
    res = _run_git(
        repo_root,
        ["apply", "--check", str(patch_path)],
        check=False,
    )
    if res.returncode != 0:
      detail = (res.stderr or res.stdout).strip()
      failures.append(
          f"Patch `{patch_rel}` failed `git apply --check`: {detail}")

  for doc_path in [*case.must_modify, *case.must_not_modify]:
    if not (repo_root / doc_path).exists():
      failures.append(f"Missing doc target `{doc_path}`.")

  return {
      "case_id": case.case_id,
      "yaml_path": case.yaml_path,
      "passed": not failures,
      "failures": failures,
  }


def cleanup_case(
    case_id: str,
    worktree_root: Path,
    repo_root: Path = REPO_ROOT,
) -> None:
  norm_id = case_id.replace("_", "-")
  slug = _case_slug(norm_id)
  for worktree_dir in (
      _worktree_dir_for_case(norm_id, worktree_root),
      (worktree_root / f"doc-eval-{norm_id}").resolve(),
  ):
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
  for branch in (
      f"cl-branch-{slug}",
      f"cl-base-{slug}",
      f"doc-eval-{norm_id}",
      f"doc-eval-base-{norm_id}",
  ):
    _run_git(repo_root, ["branch", "-D", branch], check=False)


def _resolve_cases(case_arg: str) -> list[EvalCase]:
  if case_arg == "all":
    return list(EVAL_CASES.values())
  norm_arg = case_arg.replace("_", "-")
  if norm_arg not in EVAL_CASES:
    valid = ", ".join(["all", *EVAL_CASES.keys()])
    raise ValueError(f"Unknown case `{case_arg}`. Valid choices: {valid}")
  return [EVAL_CASES[norm_arg]]


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(
      description="Evaluation harness for V8 doc-invalidation-checker skill.")
  subparsers = parser.add_subparsers(dest="subcommand", required=True)

  list_parser = subparsers.add_parser("list", help="List available eval cases.")
  list_parser.add_argument("--json", action="store_true")

  check_parser = subparsers.add_parser(
      "check",
      help="Verify .patch fixtures apply cleanly and doc targets exist.",
  )
  check_parser.add_argument("--case", required=True, help="Case ID or `all`.")
  check_parser.add_argument("--json", action="store_true")

  setup_parser = subparsers.add_parser(
      "setup", help="Create isolated worktree(s) and apply .patch fixture(s).")
  setup_parser.add_argument("--case", required=True, help="Case ID or `all`.")
  setup_parser.add_argument(
      "--mode",
      choices=("explicit", "implicit"),
      default="explicit",
      help="Prompt mode: explicit skill invocation or implicit workflow check.",
  )
  setup_parser.add_argument(
      "--diff-kind",
      choices=("committed", "uncommitted"),
      default="committed",
      help="Whether the src/ change is committed on the branch or uncommitted.",
  )
  setup_parser.add_argument(
      "--worktree-root", type=Path, default=DEFAULT_WORKTREE_ROOT)
  setup_parser.add_argument("--json", action="store_true")

  grade_parser = subparsers.add_parser(
      "grade", help="Grade docs/ updates in the eval worktree(s).")
  grade_parser.add_argument("--case", required=True, help="Case ID or `all`.")
  grade_parser.add_argument(
      "--worktree-root", type=Path, default=DEFAULT_WORKTREE_ROOT)
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

  if args.subcommand == "check":
    results = [check_case(c) for c in cases]
    all_passed = all(r["passed"] for r in results)
    if args.json:
      print(json.dumps(results, indent=2))
    else:
      passed_count = sum(1 for r in results if r["passed"])
      print(f"CHECK SUMMARY: {passed_count}/{len(results)} cases valid.")
      for res in results:
        status = "PASS" if res["passed"] else "FAIL"
        print(f"  [{status}] {res['case_id']} ({res['yaml_path']})")
        for failure in res["failures"]:
          print(f"         - {failure}")
    return 0 if all_passed else 1

  if args.subcommand == "setup":
    metas = [
        setup_case(
            c,
            args.worktree_root,
            mode=args.mode,
            diff_kind=args.diff_kind,
        ) for c in cases
    ]
    if args.json:
      print(json.dumps(metas, indent=2))
    else:
      for meta in metas:
        print("=" * 80)
        print(f"CASE: {meta['case_id']} ({meta['category']})")
        print(f"CONFIG: {meta['yaml_path']}")
        print(f"WORKTREE: {meta['worktree_dir']}")
        print(f"PROMPT ({meta['mode']}, {meta['diff_kind']}):")
        print(meta["prompt"])
        print("=" * 80)
    return 0

  if args.subcommand == "grade":
    results = []
    all_passed = True
    for case in cases:
      wt_dir = _worktree_dir_for_case(case.case_id, args.worktree_root)
      res = grade_case(case, wt_dir)
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
        print(f"  [{status}] {res['case_id']} ({res['mode']}, "
              f"{res['diff_kind']}) -> modified_docs={res['modified_docs']}")
        for failure in res["failures"]:
          print(f"         - {failure}")
    return 0 if all_passed else 1

  if args.subcommand == "cleanup":
    for case in cases:
      cleanup_case(case.case_id, args.worktree_root)
      print(f"Cleaned up doc-eval-{case.case_id}")
    return 0

  return 1


if __name__ == "__main__":
  sys.exit(main())
