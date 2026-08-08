#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import annotations

from pathlib import Path
import os.path


def create_symlink(src: Path, dest: Path):
  try:
    symlink = Path(os.path.relpath(src, dest.parent))
    dest.symlink_to(symlink)
    print(f"Symlinked {src.name} to {symlink}")
  except OSError as e:
    print(f"Failed to create symlink for {src.name}: {e}")


FILE_PATH = Path(__file__).resolve()
repo_root = FILE_PATH.parents[2]
agents_src = repo_root / "agents"
agents_dest = repo_root / ".agents"

# 1. Check if .agents is a symlink to agents
if agents_dest.is_symlink():
  target = agents_dest.readlink()
  if str(target) in ("agents", str(agents_src)):
    print(f"Removing symlink .agents -> {target}")
    agents_dest.unlink()

# 2. Ensure .agents is a real directory
agents_dest.mkdir(parents=True, exist_ok=True)

# 3. Process top-level items in agents/
for item in agents_src.iterdir():
  # Skip hidden files/dirs
  if item.name.startswith("."):
    continue

  dest_item = agents_dest / item.name

  if item.name not in ["agents", "skills", "rules"]:
    # For other items, symlink directly
    if not dest_item.exists():
      create_symlink(item, dest_item)
    continue

  # Create real directory in .agents/
  dest_item.mkdir(parents=True, exist_ok=True)

  # Cleanup dead symlinks
  for sub_item in dest_item.iterdir():
    if sub_item.is_symlink() and not sub_item.exists():
      print(f"Removing dead symlink {sub_item}")
      sub_item.unlink()

  # Symlink individual items inside
  for sub_item in item.iterdir():
    dest_sub = dest_item / sub_item.name
    if not dest_sub.exists():
      create_symlink(sub_item, dest_sub)

gemini_md_path = repo_root / "GEMINI.md"
if not gemini_md_path.exists():
  with gemini_md_path.open("w") as f:
    f.write("@agents/prompts/templates/modular.md\n")
  print(f"Created {gemini_md_path}")
else:
  print(f"Skipping {gemini_md_path} creation since it already exists.")
