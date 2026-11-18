#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import annotations

from pathlib import Path
import os
import os.path
import subprocess

OPT_IN_SKILLS = {
    "cl-priority-prefix": (
        "v8.presubmit-priority-prefix",
        "V8_PRESUBMIT_PRIORITY_PREFIX",
    ),
}


def is_skill_enabled(skill_name: str, repo_root: Path) -> bool:
  if skill_name not in OPT_IN_SKILLS:
    return True
  git_cfg_key, env_var = OPT_IN_SKILLS[skill_name]
  env_val = os.environ.get(env_var, "").strip().lower()
  if env_val in ("0", "false"):
    return False
  if env_val in ("1", "true"):
    return True
  res = subprocess.run(
      ["git", "config", "--bool", git_cfg_key],
      cwd=repo_root,
      capture_output=True,
      text=True,
      check=False,
  )
  return res.returncode == 0 and res.stdout.strip().lower() == "true"

# Shared skills from `agents/shared/skills/` (chromium/agents.git DEPS) to
# symlink into `.agents/skills/`.
#
# Omitted shared skills:
# - `chromium_memory_tools`: Targets Chromium's `tools/memory/` utilities.
# - `git-dependent-branches`: Mandates `git new-branch` before every edit/commit,
#   conflicting with V8's `v8-workflow` worktree isolation.
# - `jj`: Targets Chromium's `tools/jj/` setup and `jj upload` workflow.
# - `json_schema_compiler`: Targets Chrome Extension API IDL/JSON schemas.
# - `skia-autoroll-status`: Queries Skia autorollers in Chromium.
# - `skill-creation`: Duplicates V8's `agents/rules/writing-skills.md` and
#   references Chromium-only `//agents/prompts/eval` `.promptfoo.yaml` tests and
#   the `linux-prompt-evals` trybot.
# - `utr`: Targets Chromium's `tools/utr/` Universal Test Runner.
SHARED_SKILLS = (
    # Drafting and wrapping Chromium/V8 CL descriptions and commit messages.
    "cl-description",
    # Querying public Gerrit CLs, comments, and checks via `gerrit_client.py`.
    "gerrit-cli",
    # Chromium Gerrit CL workflow with `git cl`.
    "git-cl-helper",
    # Inspecting repository files and commits at older Git revisions.
    "time-travel-code-explorer",
)


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

# Cleanup top-level dead symlinks and legacy subdirectories in .agents/
for item in agents_dest.iterdir():
  if item.is_symlink() and (not item.exists() or item.name == "shared"):
    print(f"Removing top-level symlink {item}")
    item.unlink()
  elif (item.is_dir() and not item.is_symlink() and
        not (agents_src / item.name).exists()):
    for sub_item in item.iterdir():
      if sub_item.is_symlink() and not sub_item.exists():
        print(f"Removing dead symlink {sub_item}")
        sub_item.unlink()
    if not any(item.iterdir()):
      print(f"Removing empty legacy directory {item}")
      item.rmdir()

# 3. Process top-level items in agents/
for item in agents_src.iterdir():
  # Skip hidden files/dirs and shared DEPS directory
  if item.name.startswith(".") or item.name == "shared":
    continue

  dest_item = agents_dest / item.name

  if item.name not in ["skills", "rules", "plugins"]:
    # For other items, symlink directly
    if not dest_item.exists():
      create_symlink(item, dest_item)
    continue

  # Create real directory in .agents/
  if dest_item.is_symlink():
    print(f"Removing top-level symlink {dest_item}")
    dest_item.unlink()
  dest_item.mkdir(parents=True, exist_ok=True)

  # Cleanup dead or removed shared-skill symlinks
  for sub_item in dest_item.iterdir():
    if sub_item.is_symlink():
      is_removed_shared_skill = (
          item.name == "skills" and
          "shared/skills/" in str(sub_item.readlink()) and
          sub_item.name not in SHARED_SKILLS)
      if not sub_item.exists() or is_removed_shared_skill:
        print(f"Removing symlink {sub_item}")
        sub_item.unlink()

  # Symlink individual items inside
  for sub_item in item.iterdir():
    dest_sub = dest_item / sub_item.name
    if item.name == "skills" and not is_skill_enabled(sub_item.name, repo_root):
      if dest_sub.is_symlink():
        print(f"Removing disabled opt-in skill symlink {dest_sub}")
        dest_sub.unlink()
      continue
    if not dest_sub.exists():
      create_symlink(sub_item, dest_sub)

  if item.name == "skills":
    for shared_skill_name in SHARED_SKILLS:
      shared_skill = agents_src / "shared" / "skills" / shared_skill_name
      if shared_skill.exists():
        dest_sub = dest_item / shared_skill_name
        if not dest_sub.exists():
          create_symlink(shared_skill, dest_sub)

modular_rule = "@[Modular Rules](agents/prompts/templates/modular.md)"
gemini_md_path = repo_root / "GEMINI.md"
if not gemini_md_path.exists():
  with gemini_md_path.open("w") as f:
    f.write(f"{modular_rule}\n")
  print(f"Created {gemini_md_path}")
else:
  content = gemini_md_path.read_text()
  updated = False
  if "@agents/prompts/templates/modular.md" in content:
    content = content.replace("@agents/prompts/templates/modular.md",
                              modular_rule)
    updated = True
  if "@agents/prompts/common.md" in content:
    if modular_rule in content:
      content = content.replace("@agents/prompts/common.md\n", "")
      content = content.replace("@agents/prompts/common.md", "")
    else:
      content = content.replace("@agents/prompts/common.md", modular_rule)
      if not content.endswith("\n"):
        content += "\n"
    updated = True

  if updated:
    gemini_md_path.write_text(content)
    print(f"Upgraded legacy rules in {gemini_md_path}")
  else:
    print(f"Skipping {gemini_md_path} creation since it already exists.")
