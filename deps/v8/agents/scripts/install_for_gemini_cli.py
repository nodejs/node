#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import annotations

import json
from pathlib import Path
import yaml

FILE_PATH = Path(__file__).resolve()
repo_root = FILE_PATH.parents[2]
src_dir = FILE_PATH.parents[1] / "agents"
dest_dir = repo_root / ".gemini" / "agents"

# 1. Check if .gemini/agents is a symlink
if dest_dir.is_symlink():
  print(f"Removing symlink at {dest_dir}")
  dest_dir.unlink()

dest_dir.mkdir(parents=True, exist_ok=True)

for item in src_dir.iterdir():
  if not item.is_dir():
    continue

  agent_json_path = item / "agent.json"
  config_yaml_path = item / "config.yaml"

  if not (agent_json_path.exists() and config_yaml_path.exists()):
    print(f"Skipping {item.name}: missing agent.json or config.yaml")
    continue

  with agent_json_path.open("r") as f:
    agent_json = json.load(f)
  with config_yaml_path.open("r") as f:
    config_yaml = yaml.safe_load(f)

  name = agent_json.get("name")
  description = agent_json.get("description")

  custom_agent = config_yaml.get("custom_agent", {})
  system_prompt_sections = custom_agent.get("system_prompt_sections", [])
  tool_names = custom_agent.get("tool_names", [])

  # Concatenate system prompt sections
  body = ""
  for section in system_prompt_sections:
    body += f"# {section.get('title')}\n\n{section.get('content')}\n\n"

  # Map tools to Gemini CLI names
  gemini_tools = []
  for t in tool_names:
    if t == "view_file":
      gemini_tools.append("read_file")
    elif t == "run_command":
      gemini_tools.append("run_shell_command")
    elif t == "list_dir":
      gemini_tools.append("list_directory")
    elif t == "search_web":
      gemini_tools.append("google_web_search")
    elif t == "call_mcp_tool":
      pass
    else:
      gemini_tools.append(t)

  # Create frontmatter
  frontmatter = {
      "name": name,
      "description": description,
      "kind": "local",
      "tools": gemini_tools,
      "model": "inherit"
  }

  dest_file = dest_dir / f"{name}.md"
  with dest_file.open("w") as f:
    f.write("---\n")
    yaml.dump(frontmatter, f, default_flow_style=False)
    f.write("---\n")
    f.write(body)

  print(f"Installed {name}.md for Gemini CLI")

gemini_md_path = repo_root / "GEMINI.md"
if not gemini_md_path.exists():
  with gemini_md_path.open("w") as f:
    f.write("@agents/prompts/templates/modular.md\n")
  print(f"Created {gemini_md_path}")
else:
  print(f"Skipping {gemini_md_path} creation since it already exists.")
