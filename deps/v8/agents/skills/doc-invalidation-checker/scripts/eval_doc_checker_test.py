#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for eval_doc_checker.py and its promptfoo.yaml test cases."""

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import eval_doc_checker

REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "agents" / "testing"))
import lint_promptfoo_testcases  # pylint: disable=wrong-import-position


class EvalDocCheckerTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()
    self.root = Path(self.temp_dir.name)
    self.repo_dir = self.root / "repo"
    self.eval_dir = self.repo_dir / "evals"
    self.worktree_root = self.root / "worktrees"
    self.repo_dir.mkdir(parents=True)

    self._git(self.repo_dir, "init", "-b", "main")
    self._git(self.repo_dir, "config", "user.name", "Test User")
    self._git(self.repo_dir, "config", "user.email", "test@example.com")

    # Use self-contained synthetic C++, patch, and doc fixtures so unit tests
    # are decoupled from live src/ and docs/ edits at HEAD.
    self._write(
        "src/maglev/maglev-compiler.cc",
        ("namespace v8::internal::maglev {\n"
         "constexpr uint32_t kMaxStackSlots = 4 * KB / kSystemPointerSize;\n"
         "void Compile() {\n"
         "  GraphProcessor<TruncationProcessor> truncate(graph);\n"
         "  truncate.ProcessGraph(graph);\n"
         "}\n"
         "}  // namespace v8::internal::maglev\n"),
    )
    self._write(
        "docs/compiler/maglev/compiler-maglev.md",
        ("Here `constexpr uint32_t kMaxStackSlots = 4 * KB / "
         "kSystemPointerSize` (512 slots on 64-bit).\n"),
    )
    self._write("docs/security/triaging.md", "# Manual Triaging Guide\n")
    self._git(self.repo_dir, "add", "-A")
    self._git(self.repo_dir, "commit", "-m", "Initial commit")

    self._make_patch(
        "src/maglev/maglev-compiler.cc",
        ("namespace v8::internal::maglev {\n"
         "constexpr uint32_t kMaxStackSlots = 8 * KB / kSystemPointerSize;\n"
         "void Compile() {\n"
         "  GraphProcessor<TruncationProcessor> truncate(graph);\n"
         "  truncate.ProcessGraph(graph);\n"
         "}\n"
         "}  // namespace v8::internal::maglev\n"),
        "evals/positive_case.patch",
    )
    self._make_patch(
        "src/maglev/maglev-compiler.cc",
        ("namespace v8::internal::maglev {\n"
         "constexpr uint32_t kMaxStackSlots = 4 * KB / kSystemPointerSize;\n"
         "void Compile() {\n"
         "  GraphProcessor<TruncationProcessor> truncation_processor(graph);\n"
         "  truncation_processor.ProcessGraph(graph);\n"
         "}\n"
         "}  // namespace v8::internal::maglev\n"),
        "evals/noop_case.patch",
    )
    self._write("evals/prompt.md", "Run the doc-invalidation-checker skill.\n")
    self._write(
        "evals/positive_case.promptfoo.yaml",
        ("description: Positive synthetic eval case\n"
         "prompts:\n"
         "  - 'file://prompt.md'\n"
         "providers:\n"
         "  - id: python:gemini_provider.py\n"
         "    config:\n"
         "      changes:\n"
         "        - apply: evals/positive_case.patch\n"
         "tests:\n"
         "  - assert:\n"
         "      - type: python\n"
         "        value: 'file://check_changes.py:check_files_changed'\n"
         "        config:\n"
         "          files:\n"
         "            - docs/compiler/maglev/compiler-maglev.md\n"
         "      - type: python\n"
         "        value: 'file://eval_doc_checker.py:check_files_unchanged'\n"
         "        config:\n"
         "          files:\n"
         "            - docs/security/triaging.md\n"
         "      - type: python\n"
         "        value: 'file://check_changes.py:check_file_content'\n"
         "        config:\n"
         "          files:\n"
         "            - path: docs/compiler/maglev/compiler-maglev.md\n"
         "              present:\n"
         "                - '8 * KB / kSystemPointerSize'\n"
         "                - '1024'\n"
         "              absent:\n"
         "                - '4 * KB / kSystemPointerSize'\n"
         "    metadata:\n"
         "      category: positive-constant-invariant\n"),
    )
    self._write(
        "evals/noop_case.promptfoo.yaml",
        ("description: Negative synthetic eval case\n"
         "prompts:\n"
         "  - 'file://prompt.md'\n"
         "providers:\n"
         "  - id: python:gemini_provider.py\n"
         "    config:\n"
         "      changes:\n"
         "        - apply: evals/noop_case.patch\n"
         "tests:\n"
         "  - assert:\n"
         "      - type: python\n"
         "        value: 'file://eval_doc_checker.py:check_files_unchanged'\n"
         "        config:\n"
         "          files:\n"
         "            - docs/\n"
         "    metadata:\n"
         "      category: negative-no-doc-churn\n"),
    )
    self._git(self.repo_dir, "add", "-A")
    self._git(self.repo_dir, "commit", "-m", "Add synthetic eval cases")
    self.eval_cases = eval_doc_checker.load_eval_cases(
        self.eval_dir, repo_root=self.repo_dir)

  def tearDown(self):
    self.temp_dir.cleanup()

  def _git(self, cwd: Path, *args: str) -> str:
    res = subprocess.run(
        ["git", "--no-pager", *args],
        cwd=cwd,
        capture_output=True,
        text=True,
        check=True,
    )
    return res.stdout

  def _write(self, rel_path: str, content: str, base: Path | None = None):
    target_root = base or self.repo_dir
    path = target_root / rel_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")

  def _make_patch(self, rel_path: str, new_content: str, patch_rel: str):
    self._write(rel_path, new_content)
    diff = self._git(self.repo_dir, "diff", "--", rel_path)
    self._git(self.repo_dir, "checkout", "--", rel_path)
    self._write(patch_rel, diff)

  def test_all_promptfoo_yaml_and_patch_refs_valid(self):
    self.assertEqual(5, len(eval_doc_checker.EVAL_CASES))
    yaml_files = [
        str(REPO_ROOT / case.yaml_path)
        for case in eval_doc_checker.EVAL_CASES.values()
    ]
    self.assertEqual(0, lint_promptfoo_testcases.main(["prog", *yaml_files]))

    # Live-checkout `git apply --check` and `docs/` target checks are not run
    # here because `src/` and `docs/` CLs do not run `agents/PRESUBMIT.py`. Use
    # `eval_doc_checker.py check` or scoped patch presubmits instead.
    for case_id, case in eval_doc_checker.EVAL_CASES.items():
      with self.subTest(case_id=case_id):
        self.assertTrue(case.patches, f"Missing patches in {case_id}")
        for patch_rel in case.patches:
          patch_path = REPO_ROOT / patch_rel
          self.assertTrue(patch_path.is_file(), f"Missing patch {patch_rel}")
          patch_text = patch_path.read_text(encoding="utf-8")
          self.assertIn("diff --git ", patch_text)
          self.assertIn("@@ ", patch_text)

  def test_check_case_validates_patches_and_doc_targets(self):
    case = self.eval_cases["positive-case"]
    ok_res = eval_doc_checker.check_case(case, repo_root=self.repo_dir)
    self.assertTrue(ok_res["passed"], ok_res["failures"])

    self._write(
        "src/maglev/maglev-compiler.cc",
        "namespace v8::internal::maglev {}\n",
    )
    stale_res = eval_doc_checker.check_case(case, repo_root=self.repo_dir)
    self.assertFalse(stale_res["passed"])
    self.assertIn("git apply --check", "\n".join(stale_res["failures"]))

    self._git(self.repo_dir, "checkout", "--", "src/maglev/maglev-compiler.cc")
    (self.repo_dir / "docs/compiler/maglev/compiler-maglev.md").unlink()
    missing_doc_res = eval_doc_checker.check_case(case, repo_root=self.repo_dir)
    self.assertFalse(missing_doc_res["passed"])
    self.assertIn("Missing doc target", "\n".join(missing_doc_res["failures"]))

  def test_positive_case_fails_before_fix_and_passes_after_fix(self):
    case = self.eval_cases["positive-case"]
    meta = eval_doc_checker.setup_case(
        case,
        self.worktree_root,
        repo_root=self.repo_dir,
        mode="explicit",
        diff_kind="committed",
    )
    wt_dir = Path(meta["worktree_dir"])
    try:
      initial_grade = eval_doc_checker.grade_case(case, wt_dir)
      self.assertFalse(initial_grade["passed"])
      self.assertEqual([], initial_grade["modified_docs"])

      self._write(
          "docs/compiler/maglev/compiler-maglev.md",
          ("Here `constexpr uint32_t kMaxStackSlots = 8 * KB / "
           "kSystemPointerSize` (1024 slots on 64-bit).\n"),
          base=wt_dir,
      )
      passing_grade = eval_doc_checker.grade_case(case, wt_dir)
      self.assertTrue(passing_grade["passed"], passing_grade["failures"])
      self.assertEqual(
          ["docs/compiler/maglev/compiler-maglev.md"],
          passing_grade["modified_docs"],
      )

      # Also passes if the agent staged and amended the commit.
      self._git(wt_dir, "add", "docs/compiler/maglev/compiler-maglev.md")
      self._git(wt_dir, "commit", "--amend", "--no-edit")
      amended_grade = eval_doc_checker.grade_case(case, wt_dir)
      self.assertTrue(amended_grade["passed"], amended_grade["failures"])
    finally:
      eval_doc_checker.cleanup_case(
          case.case_id, self.worktree_root, repo_root=self.repo_dir)

  def test_guardrail_rejects_modifying_triaging_md_or_em_dashes(self):
    case = self.eval_cases["positive-case"]
    meta = eval_doc_checker.setup_case(
        case,
        self.worktree_root,
        repo_root=self.repo_dir,
        mode="implicit",
        diff_kind="uncommitted",
    )
    wt_dir = Path(meta["worktree_dir"])
    try:
      self._write(
          "docs/compiler/maglev/compiler-maglev.md",
          ("Here `constexpr uint32_t kMaxStackSlots = 8 * KB / "
           "kSystemPointerSize` \u2014 1024 slots on 64-bit "
           "[bad](missing.md).\n"),
          base=wt_dir,
      )
      self._write("docs/security/triaging.md", "# Modified!\n", base=wt_dir)

      grade = eval_doc_checker.grade_case(case, wt_dir)
      self.assertFalse(grade["passed"])
      joined = "\n".join(grade["failures"])
      self.assertIn("docs/security/triaging.md", joined)
      self.assertIn("Unicode em-dash (U+2014)", joined)
      self.assertIn("Broken relative markdown links", joined)
    finally:
      eval_doc_checker.cleanup_case(
          case.case_id, self.worktree_root, repo_root=self.repo_dir)

  def test_negative_noop_case_passes_only_when_no_docs_touched(self):
    case = self.eval_cases["noop-case"]
    meta = eval_doc_checker.setup_case(
        case,
        self.worktree_root,
        repo_root=self.repo_dir,
        mode="explicit",
        diff_kind="committed",
    )
    wt_dir = Path(meta["worktree_dir"])
    try:
      grade_clean = eval_doc_checker.grade_case(case, wt_dir)
      self.assertTrue(grade_clean["passed"], grade_clean["failures"])

      self._write(
          "docs/compiler/maglev/compiler-maglev.md",
          "Unnecessary churn.\n",
          base=wt_dir,
      )
      grade_churn = eval_doc_checker.grade_case(case, wt_dir)
      self.assertFalse(grade_churn["passed"])
      self.assertIn(
          "docs/compiler/maglev/compiler-maglev.md",
          "\n".join(grade_churn["failures"]),
      )
    finally:
      eval_doc_checker.cleanup_case(
          case.case_id, self.worktree_root, repo_root=self.repo_dir)


if __name__ == "__main__":
  unittest.main()
