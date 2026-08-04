#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys
from pathlib import Path


def setup_worktree_symlinks(main_repo: Path, worktree_dir: Path):
  # If the worktree is the main repo itself, do nothing.
  if main_repo.resolve() == worktree_dir.resolve():
    return

  shared_dirs = [
      "build", "buildtools", "base", "testing", "tools/clang", "tools/rust",
      "tools/luci-go"
  ]
  for d in shared_dirs:
    target = main_repo / d
    link = worktree_dir / d
    if target.exists() and not link.exists():
      link.parent.mkdir(parents=True, exist_ok=True)
      rel = os.path.relpath(target.resolve(), link.parent.resolve())
      os.symlink(rel, link)

  gclient_entries_file = main_repo.parent / ".gclient_entries"
  if gclient_entries_file.is_file():
    entries = {}
    with open(gclient_entries_file) as f:
      exec(f.read(), entries)
    for key in entries.get('entries', {}).keys():
      if key.startswith("v8/"):
        rel_path = key[3:].split(':')[0]
        target = main_repo / rel_path
        link = worktree_dir / rel_path
        if target.exists() and not link.exists() and not link.is_symlink():
          link.parent.mkdir(parents=True, exist_ok=True)
          rel = os.path.relpath(target.resolve(), link.parent.resolve())
          os.symlink(rel, link)


if __name__ == "__main__":
  if len(sys.argv) != 3:
    print("usage: ./setup_worktree_build.py main_repo worktree_dir")
    exit(1)
  setup_worktree_symlinks(Path(sys.argv[1]), Path(sys.argv[2]))
