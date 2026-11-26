#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for get_recent_commits.py."""

from __future__ import annotations

import io
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import get_recent_commits

REPO_ROOT = Path(__file__).resolve().parents[2]


class GetRecentCommitsTest(unittest.TestCase):

  def setUp(self):
    self.temp_dir = tempfile.TemporaryDirectory()
    self.repo_dir = Path(self.temp_dir.name)
    self._old_cwd = Path.cwd()
    os.chdir(self.repo_dir)
    self._git("init", "-b", "main")
    self._git("config", "user.name", "Test User")
    self._git("config", "user.email", "test@example.com")

  def tearDown(self):
    os.chdir(self._old_cwd)
    self.temp_dir.cleanup()

  def _git(self, *args: str) -> str:
    res = subprocess.run(
        ["git", "--no-pager", *args],
        cwd=self.repo_dir,
        capture_output=True,
        text=True,
        check=True,
    )
    return res.stdout

  def _write(self, rel_path: str, content: str) -> Path:
    path = self.repo_dir / rel_path
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return path

  def test_subsystem_docs_map_targets_exist_in_v8_checkout(self):
    for src_prefix, doc_targets in (
        get_recent_commits.SUBSYSTEM_DOCS_MAP.items()):
      with self.subTest(src_prefix=src_prefix):
        self.assertTrue((REPO_ROOT / src_prefix).exists(),
                        f"Missing src prefix: {src_prefix}")
        for doc_target in doc_targets:
          self.assertTrue((REPO_ROOT / doc_target).exists(),
                          f"Missing doc target {doc_target} for {src_prefix}")

  def test_candidate_docs_for_files_maps_and_deduplicates(self):
    files = [
        "src/maglev/maglev-compiler.cc",
        "src/maglev/maglev-ir.h",
        "src/sandbox/js-dispatch-table.h",
        "test/mjsunit/foo.js",
        "",
    ]
    candidates = get_recent_commits._candidate_docs_for_files(files)
    self.assertEqual(
        [
            "docs/compiler/maglev/",
            "docs/parsing/hole-checks-optimized.md",
            "docs/sandbox/",
        ],
        candidates,
    )

  def test_check_branch_diff_with_committed_changes(self):
    self._write("src/maglev/maglev-compiler.cc", "int x = 1;\n")
    self._git("add", ".")
    self._git("commit", "-m", "Initial commit")

    self._git("checkout", "-b", "feature", "main")
    self._write("src/maglev/maglev-compiler.cc", "int x = 2;\n")
    self._git("add", ".")
    self._git("commit", "-m", "[maglev] Update x")

    with mock.patch("sys.stdout", new_callable=io.StringIO) as stdout:
      self.assertEqual(0, get_recent_commits.main(["--branch", "--base=main"]))

    output = stdout.getvalue()
    self.assertIn("BRANCH DIFF AGAINST: main", output)
    self.assertIn("[maglev] Update x", output)
    self.assertIn("src/maglev/maglev-compiler.cc", output)
    self.assertIn("docs/compiler/maglev/", output)
    self.assertIn("+int x = 2;", output)

  def test_check_branch_diff_falls_back_to_uncommitted_src_changes(self):
    self._write("src/heap/heap.cc", "int heap_limit = 10;\n")
    self._git("add", ".")
    self._git("commit", "-m", "Initial commit")

    self._write("src/heap/heap.cc", "int heap_limit = 20;\n")

    with mock.patch("sys.stdout", new_callable=io.StringIO) as stdout:
      self.assertEqual(0, get_recent_commits.main(["--branch", "--base=main"]))

    output = stdout.getvalue()
    self.assertIn("showing uncommitted changes in src/ and include/", output)
    self.assertIn("src/heap/heap.cc", output)
    self.assertIn("docs/heap/", output)
    self.assertIn("+int heap_limit = 20;", output)

  def test_check_branch_diff_includes_both_committed_and_uncommitted_changes(
      self):
    self._write("src/maglev/maglev-compiler.cc", "int x = 1;\n")
    self._write("src/heap/heap.cc", "int heap_limit = 10;\n")
    self._git("add", ".")
    self._git("commit", "-m", "Initial commit")

    self._git("checkout", "-b", "feature", "main")
    self._write("src/maglev/maglev-compiler.cc", "int x = 2;\n")
    self._git("add", ".")
    self._git("commit", "-m", "[maglev] Update x")

    # Add an uncommitted change on top of the committed branch change.
    self._write("src/heap/heap.cc", "int heap_limit = 20;\n")

    with mock.patch("sys.stdout", new_callable=io.StringIO) as stdout:
      self.assertEqual(0, get_recent_commits.main(["--branch", "--base=main"]))

    output = stdout.getvalue()
    self.assertIn(
        "Including uncommitted changes in src/ and include/ alongside "
        "committed branch changes.",
        output,
    )
    self.assertIn("src/maglev/maglev-compiler.cc", output)
    self.assertIn("src/heap/heap.cc", output)
    self.assertIn("docs/compiler/maglev/", output)
    self.assertIn("docs/heap/", output)
    self.assertIn("+int x = 2;", output)
    self.assertIn("+int heap_limit = 20;", output)

  def test_check_uncommitted_diff(self):
    self._write("src/objects/string.h", "class String {};\n")
    self._git("add", ".")
    self._git("commit", "-m", "Initial commit")

    self._write("src/objects/string.h", "class String { int len; };\n")

    with mock.patch("sys.stdout", new_callable=io.StringIO) as stdout:
      self.assertEqual(0, get_recent_commits.main(["--uncommitted"]))

    output = stdout.getvalue()
    self.assertIn("UNCOMMITTED CHANGES (git diff HEAD)", output)
    self.assertIn("src/objects/string.h", output)
    self.assertIn("docs/objects/", output)
    self.assertIn("docs/heap/objects-and-maps.md", output)

  def test_get_recent_commits_count_and_range(self):
    self._write("src/wasm/wasm-engine.cc", "int v = 1;\n")
    self._git("add", ".")
    self._git("commit", "-m", "Commit 1")

    self._write("src/sandbox/sandbox.cc", "int s = 2;\n")
    self._git("add", ".")
    self._git("commit", "-m", "Commit 2")

    with mock.patch("sys.stdout", new_callable=io.StringIO) as stdout:
      self.assertEqual(0, get_recent_commits.main(["1"]))

    output = stdout.getvalue()
    self.assertIn("Commit 2", output)
    self.assertNotIn("Commit 1", output)
    self.assertIn("docs/sandbox/", output)

    with mock.patch("sys.stdout", new_callable=io.StringIO) as stdout_range:
      self.assertEqual(0, get_recent_commits.main(["--range", "HEAD~1..HEAD"]))

    range_output = stdout_range.getvalue()
    self.assertIn("Commit 2", range_output)
    self.assertNotIn("Commit 1", range_output)


if __name__ == "__main__":
  unittest.main()
