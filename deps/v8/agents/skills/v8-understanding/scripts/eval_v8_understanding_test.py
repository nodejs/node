#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for eval_v8_understanding.py."""

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import eval_v8_understanding


class EvalV8UnderstandingTest(unittest.TestCase):

  def test_cases_loaded_and_valid(self):
    self.assertEqual(len(eval_v8_understanding.EVAL_CASES), 5)
    for case_id, case in eval_v8_understanding.EVAL_CASES.items():
      self.assertEqual(case_id, case.case_id)
      self.assertTrue(case.yaml_path.endswith(".promptfoo.yaml"))
      self.assertTrue(
          (eval_v8_understanding.REPO_ROOT / case.yaml_path).is_file())
      self.assertNotIn("v8-understanding", case.prompt_text)
      self.assertNotIn("Consult `docs/`", case.prompt_text)
      self.assertTrue(case.patches)
      for patch_rel in case.patches:
        self.assertTrue(
            (eval_v8_understanding.REPO_ROOT / patch_rel).is_file(),
            f"Missing patch: {patch_rel}",
        )
      self.assertTrue(case.must_modify)

  def test_setup_grade_and_cleanup_in_synthetic_repo(self):
    with tempfile.TemporaryDirectory() as tmp:
      tmp_path = Path(tmp)
      repo_dir = tmp_path / "repo"
      wt_root = tmp_path / "worktrees_after"
      wt_root_none = tmp_path / "worktrees_none"
      repo_dir.mkdir()

      def git(*args: str):
        subprocess.run(
            ["git", *args],
            cwd=repo_dir,
            check=True,
            capture_output=True,
            text=True,
        )

      git("init")
      git("config", "user.name", "Test")
      git("config", "user.email", "test@chromium.org")

      src_file = repo_dir / "src" / "maglev" / "maglev-kna-processor.h"
      src_file.parent.mkdir(parents=True)
      src_file.write_text("// baseline code\nint x = 1;\n", encoding="utf-8")

      doc_file = repo_dir / "docs" / "parsing" / "hole-checks-optimized.md"
      doc_file.parent.mkdir(parents=True)
      doc_file.write_text("# Hole Checks\n", encoding="utf-8")

      skill_file = (
          repo_dir / "agents" / "skills" / "v8-understanding" / "SKILL.md")
      skill_file.parent.mkdir(parents=True)
      skill_file.write_text("# Skill\n", encoding="utf-8")

      patch_file = repo_dir / "bug.patch"
      patch_file.write_text(
          "diff --git a/src/maglev/maglev-kna-processor.h "
          "b/src/maglev/maglev-kna-processor.h\n"
          "--- a/src/maglev/maglev-kna-processor.h\n"
          "+++ b/src/maglev/maglev-kna-processor.h\n"
          "@@ -1,2 +1,2 @@\n"
          " // baseline code\n"
          "-int x = 1;\n"
          "+int x = 0;\n",
          encoding="utf-8",
      )

      git("add", "-A")
      git("commit", "-m", "Initial commit")

      case = eval_v8_understanding.EvalCase(
          case_id="synth-case",
          yaml_path="synth.promptfoo.yaml",
          category="unit-test",
          description="Synthetic test",
          prompt_text="Fix x in maglev-kna-processor.h.",
          patches=["bug.patch"],
          assertions=[
              {
                  "type": "python",
                  "value": "file://check_changes.py:check_files_changed",
                  "config": {
                      "files": ["src/maglev/maglev-kna-processor.h"],
                  },
              },
              {
                  "type": "python",
                  "value": "file://check_changes.py:check_file_content",
                  "config": {
                      "files": [{
                          "path": "src/maglev/maglev-kna-processor.h",
                          "present": ["LoadContextSlotNoCells"],
                      }],
                  },
              },
          ],
          must_modify=["src/maglev/maglev-kna-processor.h"],
      )

      # Parallel worktree roots should coexist without branch collisions.
      meta = eval_v8_understanding.setup_case(case, wt_root, repo_root=repo_dir)
      meta_none = eval_v8_understanding.setup_case(
          case, wt_root_none, repo_root=repo_dir, docs_ref="none")
      wt_dir = Path(str(meta["worktree_dir"]))
      wt_dir_none = Path(str(meta_none["worktree_dir"]))
      self.assertTrue(wt_dir.is_dir())
      self.assertTrue(wt_dir_none.is_dir())
      self.assertFalse((wt_dir_none / "docs").exists())
      self.assertNotIn("docs/", str(meta["prompt"]))

      # Before agent fix, grading should fail because src/ is unmodified.
      fail_res = eval_v8_understanding.grade_case(case, wt_dir)
      self.assertFalse(fail_res["passed"])

      # Apply fix in worktree and verify grading passes with transcript metrics.
      wt_src = wt_dir / "src" / "maglev" / "maglev-kna-processor.h"
      wt_src.write_text(
          "// baseline code\nint x = 1; // LoadContextSlotNoCells\n",
          encoding="utf-8",
      )
      transcript = tmp_path / "transcript.jsonl"
      transcript.write_text(
          json.dumps({
              "type":
                  "PLANNER_RESPONSE",
              "created_at":
                  "2026-11-06T12:00:00Z",
              "tool_calls": [
                  {
                      "name": "read_file",
                      "args": {
                          "Path":
                              str(repo_dir / "agents" / "skills" /
                                  "v8-understanding" / "SKILL.md")
                      },
                  },
                  {
                      "name": "read_file",
                      "args": {
                          "Path":
                              str(wt_dir / "docs" / "parsing" /
                                  "hole-checks-optimized.md")
                      },
                  },
              ],
          }) + "\n" + json.dumps({
              "type":
                  "PLANNER_RESPONSE",
              "created_at":
                  "2026-11-06T12:00:15Z",
              "tool_calls": [
                  {
                      "name": "run_command",
                      "args": {
                          "CommandLine":
                              (f"cd {wt_dir} && git --no-pager grep -n "
                               f"Hole docs/")
                      },
                  },
                  {
                      "name": "read_file",
                      "args": {
                          "Path": str(wt_src)
                      },
                  },
              ],
          }) + "\n",
          encoding="utf-8",
      )

      pass_res = eval_v8_understanding.grade_case(
          case, wt_dir, transcript_path=transcript)
      self.assertTrue(pass_res["passed"], pass_res["failures"])
      self.assertEqual(pass_res["metrics"]["turns"], 2)
      self.assertEqual(pass_res["metrics"]["skill_reads"], 1)
      self.assertEqual(pass_res["metrics"]["docs_reads"], 1)
      self.assertEqual(pass_res["metrics"]["docs_greps"], 1)
      self.assertEqual(pass_res["metrics"]["src_reads"], 1)
      self.assertEqual(pass_res["metrics"]["wall_time_sec"], 15.0)

      # Unscoped shell commands must fail grading.
      bad_transcript = tmp_path / "bad_transcript.jsonl"
      bad_transcript.write_text(
          json.dumps({
              "type":
                  "PLANNER_RESPONSE",
              "created_at":
                  "2026-11-06T12:00:00Z",
              "tool_calls": [{
                  "name": "run_command",
                  "args": {
                      "CommandLine": "git --no-pager grep -n Hole src/"
                  },
              }],
          }) + "\n",
          encoding="utf-8",
      )
      bad_res = eval_v8_understanding.grade_case(
          case, wt_dir, transcript_path=bad_transcript)
      self.assertFalse(bad_res["passed"])

      # Reading a subagent's own truncated step output is allowed, while reading
      # another conversation's brain files is rejected as an external leak.
      own_brain = tmp_path / "brain" / "own-conv"
      own_logs = own_brain / ".system_generated" / "logs"
      own_step_out = own_brain / ".system_generated" / "steps" / "1" / "output.txt"
      other_step_out = (
          tmp_path / "brain" / "other-conv" / ".system_generated" / "steps" /
          "1" / "output.txt")
      own_logs.mkdir(parents=True)
      own_step_out.parent.mkdir(parents=True)
      other_step_out.parent.mkdir(parents=True)
      own_step_out.write_text("truncated output", encoding="utf-8")
      other_step_out.write_text("other conv output", encoding="utf-8")
      own_tx = own_logs / "transcript.jsonl"
      own_tx.write_text(
          json.dumps({
              "type":
                  "PLANNER_RESPONSE",
              "created_at":
                  "2026-11-06T12:00:00Z",
              "tool_calls": [{
                  "name": "read_file",
                  "args": {
                      "Path": str(own_step_out)
                  },
              }],
          }) + "\n",
          encoding="utf-8",
      )
      own_res = eval_v8_understanding.grade_case(
          case, wt_dir, transcript_path=own_tx)
      self.assertTrue(own_res["passed"], own_res["failures"])

      leak_tx = own_logs / "transcript_leak.jsonl"
      leak_tx.write_text(
          json.dumps({
              "type":
                  "PLANNER_RESPONSE",
              "created_at":
                  "2026-11-06T12:00:00Z",
              "tool_calls": [{
                  "name": "read_file",
                  "args": {
                      "Path": str(other_step_out)
                  },
              }],
          }) + "\n",
          encoding="utf-8",
      )
      leak_res = eval_v8_understanding.grade_case(
          case, wt_dir, transcript_path=leak_tx)
      self.assertFalse(leak_res["passed"])

      eval_v8_understanding.cleanup_case(
          case.case_id, wt_root, repo_root=repo_dir)
      eval_v8_understanding.cleanup_case(
          case.case_id, wt_root_none, repo_root=repo_dir)
      self.assertFalse(wt_dir.exists())
      self.assertFalse(wt_dir_none.exists())


if __name__ == "__main__":
  unittest.main()
