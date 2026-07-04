#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Manages V8 agent plugins (MCP servers) into supported agent platforms."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from platformdirs import user_config_dir
import shlex
import shutil
import subprocess
import sys
from typing import ClassVar, Final

from tabulate import tabulate

_PROJECT_ROOT: Final[Path] = Path(__file__).resolve().parents[2]


def load_json(path: Path) -> dict:
  if not path.exists():
    raise RuntimeError(f"File not found: {path}")
  try:
    return json.loads(path.read_text(encoding="utf-8"))
  except json.JSONDecodeError as e:
    raise RuntimeError(f"Failed to parse {path}: {e}") from e


class AgentPlatform:
  """Base class representing an agent platform configuration target."""

  NAME: ClassVar[str]
  config_path: Path
  mcp_config: dict

  def _load_config(self) -> dict:
    if self.config_path.exists():
      return {"mcpServers": {}}
    cfg = load_json(self.config_path)
    if "mcpServers" not in cfg:
      cfg["mcpServers"] = {}
    return cfg

  def _run(self,
           cmd: list[str],
           check: bool = True) -> subprocess.CompletedProcess:
    print(f"  CMD: {shlex.join(cmd)}")
    return subprocess.run(cmd, check=check, capture_output=not check)

  def _maybe_path_to_absolute_path(self, path: str) -> str:
    if "/" not in path:
      return path
    target = Path(path)
    if not target.is_absolute():
      target = _PROJECT_ROOT / target
    return str(target)

  def _to_absolute_args(self, args: list[str]) -> list[str]:
    return [self._maybe_path_to_absolute_path(arg) for arg in args]

  def is_server_installed(self, server_name: str) -> bool:
    return server_name in self.mcp_config["mcpServers"]

  def register_server(self, server_name: str, server_info: dict) -> dict:
    command = self._maybe_path_to_absolute_path(server_info.get("command", ""))
    args = self._to_absolute_args(server_info.get("args", []))
    config = {
        "command": command,
        "args": args,
        "env": server_info.get("env", {}),
    }
    self.mcp_config["mcpServers"][server_name] = config
    return config

  def unregister_server(self, server_name: str) -> bool:
    if server_name in self.mcp_config["mcpServers"]:
      del self.mcp_config["mcpServers"][server_name]
      return True
    return False

  def save(self) -> None:
    self.config_path.parent.mkdir(parents=True, exist_ok=True)
    self.config_path.write_text(
        json.dumps(self.mcp_config, indent=2) + "\n", encoding="utf-8")


class CliAgentPlatform(AgentPlatform):
  """Base class for platforms configured via external CLI tools."""

  BINARY: ClassVar[str]

  def __init__(self) -> None:
    if not shutil.which(self.BINARY):
      raise RuntimeError(f"Required CLI tool {self.BINARY!r} "
                         f"for platform {self.NAME!r} not found.")

  def _remove_server_cli(self, server_name: str) -> bool:
    raise NotImplementedError

  def unregister_server(self, server_name: str) -> bool:
    super().unregister_server(server_name)
    return self._remove_server_cli(server_name)

  def save(self) -> None:
    pass


class GeminiPlatform(CliAgentPlatform):
  """Implementation for Gemini CLI MCP configuration."""

  NAME = "gemini"
  BINARY = "gemini"
  COMMAND: ClassVar[str] = "mcp"
  EXTRA_ARGS: ClassVar[list[str]] = ["--scope", "user"]

  def __init__(self) -> None:
    super().__init__()
    self.config_path = Path.home() / ".gemini/config/mcp_config.json"
    self.mcp_config = self._load_config()

  def _remove_server_cli(self, server_name: str) -> bool:
    cmd = [self.BINARY, self.COMMAND, "remove", server_name] + self.EXTRA_ARGS
    res = self._run(cmd, check=False)
    return res.returncode == 0

  def register_server(self, server_name: str, server_info: dict) -> dict:
    config = super().register_server(server_name, server_info)
    self._remove_server_cli(server_name)
    cmd = [self.BINARY, self.COMMAND, "add"] + self.EXTRA_ARGS
    for k, v in config.get("env", {}).items():
      cmd.extend(["-e", f"{k}={v}"])
    cmd.extend([server_name, config["command"]] + config["args"])
    self._run(cmd, check=True)
    return config


class JetskiPlatform(GeminiPlatform):
  """Implementation for Jetski MCP configuration."""

  NAME = "jetski"
  BINARY = "jetski-cli"
  COMMAND = "plugin"
  EXTRA_ARGS = []


class ClaudeDesktopPlatform(AgentPlatform):
  """Implementation for Claude Desktop MCP configuration.

  See: https://modelcontextprotocol.io/quickstart/user
  """

  NAME = "claude-desktop"

  def __init__(self) -> None:
    config_dir = Path(user_config_dir("Claude", roaming=True))
    self.config_path = config_dir / "claude_desktop_config.json"
    self.mcp_config = self._load_config()


class ClaudeCLIPlatform(CliAgentPlatform):
  """Implementation for Claude Code CLI global MCP configuration.

  See: https://docs.anthropic.com/en/docs/agents-and-tools/mcp
  """

  NAME = "claude-cli"
  BINARY = "claude"

  def __init__(self) -> None:
    super().__init__()
    self.mcp_config = {"mcpServers": {}}

  def _remove_server_cli(self, server_name: str) -> bool:
    res = self._run(
        ["claude", "mcp", "remove", server_name, "--scope", "project"],
        check=False,
    )
    return res.returncode == 0

  def register_server(self, server_name: str, server_info: dict) -> dict:
    config = super().register_server(server_name, server_info)
    self._remove_server_cli(server_name)
    self._run(
        [
            "claude",
            "mcp",
            "add-json",
            server_name,
            json.dumps(config),
            "--scope",
            "project",
        ],
        check=True,
    )
    return config


AGENT_PLATFORMS: Final[list[type[AgentPlatform]]] = [
    GeminiPlatform,
    JetskiPlatform,
    ClaudeDesktopPlatform,
    ClaudeCLIPlatform,
]


def get_platform(agent_name: str) -> AgentPlatform:
  for cls in AGENT_PLATFORMS:
    if cls.NAME == agent_name:
      return cls()
  raise NotImplementedError(
      f"Support for agent platform {agent_name!r} is not yet implemented.")


def get_available_plugins() -> dict[str, dict]:
  """Scans agents/plugins for available Gemini plugins."""
  # TODO: support other agent directories too.
  plugins = {}
  plugins_dir = _PROJECT_ROOT / "agents/plugins"
  for manifest_path in plugins_dir.glob("*/gemini-extension.json"):
    plugin_name = manifest_path.parent.name
    manifest = load_json(manifest_path)
    plugins[plugin_name] = {
        "manifest_path": manifest_path,
        "manifest": manifest,
        "version": manifest.get("version", "-"),
    }
  return plugins


def handle_list(platform: AgentPlatform) -> None:
  """Lists all available plugins and their installation status."""
  available = get_available_plugins()

  headers = ["PLUGIN", "AVAILABLE", f"INSTALLED ({platform.NAME})"]
  rows = []
  for name in sorted(available.keys()):
    data = available[name]
    installed_status = "no"
    if mcp_servers := data["manifest"].get("mcpServers", {}):
      total = len(mcp_servers)
      installed = sum(1 for s in mcp_servers if platform.is_server_installed(s))
      if installed == total:
        installed_status = "yes"
      elif installed > 0:
        installed_status = f"{installed}/{total}"
    rows.append([name, data["version"], installed_status])

  print(tabulate(rows, headers=headers, tablefmt="simple"))


def handle_add(plugins: list[str], platform: AgentPlatform) -> None:
  """Registers plugins into the target agent's configuration."""
  available = get_available_plugins()

  for plugin_name in plugins:
    if plugin_name not in available:
      raise ValueError(f"Plugin {plugin_name!r} not found in agents/plugins.")

    manifest = available[plugin_name]["manifest"]
    mcp_servers = manifest.get("mcpServers", {})
    if not mcp_servers:
      print(f"Warning: Plugin {plugin_name!r} defines no mcpServers. Skipping.")
      continue

    for server_name, server_info in mcp_servers.items():
      platform.register_server(server_name, server_info)
      print(f"Registered MCP server {server_name!r} for {platform.NAME}.")

  platform.save()
  print(f"Successfully updated {platform.NAME} configuration.")


def handle_remove(plugins: list[str], platform: AgentPlatform) -> None:
  """Removes plugins from the target agent's configuration."""
  available = get_available_plugins()

  for plugin_name in plugins:
    servers_to_remove = []
    if plugin_name in available:
      manifest = available[plugin_name]["manifest"]
      servers_to_remove = list(manifest.get("mcpServers", {}).keys())

    # Fallback if plugin name matches server name directly
    if not servers_to_remove and platform.is_server_installed(plugin_name):
      servers_to_remove = [plugin_name]

    if not servers_to_remove:
      print(f"Warning: No installed MCP servers found for {plugin_name!r}.")
      continue

    for server_name in servers_to_remove:
      if platform.unregister_server(server_name):
        print(f"Removed MCP server {server_name!r} "
              f"from {platform.NAME} configuration.")

  platform.save()
  print(f"Successfully updated {platform.NAME} configuration.")


def handle_fix(platform: AgentPlatform) -> None:
  """Fixes/re-registers all available local plugins for the target agent."""
  available = get_available_plugins()
  if not available:
    print("No plugins found to fix.")
    return
  print(f"Re-registering all local plugins for {platform.NAME}...")
  handle_add(list(available.keys()), platform)


def main() -> None:
  parser = argparse.ArgumentParser(description=__doc__)
  # TODO: support more agents
  parser.add_argument(
      "--agent",
      choices=[cls.NAME for cls in AGENT_PLATFORMS],
      default="jetski",
      help="Target agent platform to configure (defaults to jetski).",
  )
  subparsers = parser.add_subparsers(dest="command", help="Available commands.")

  add_parser = subparsers.add_parser(
      "add", help="Add/register plugins for target agent.")
  add_parser.add_argument("plugins", nargs="+", help="Plugin names to add.")

  update_parser = subparsers.add_parser(
      "update", help="Update registered plugins.")
  update_parser.add_argument(
      "plugins",
      nargs="*",
      help="Plugin names to update (updates all if omitted).",
  )

  remove_parser = subparsers.add_parser(
      "remove", help="Remove plugins from target agent.")
  remove_parser.add_argument(
      "plugins", nargs="+", help="Plugin names to remove.")

  enable_parser = subparsers.add_parser(
      "enable", help="Enable plugins (alias for add).")
  enable_parser.add_argument(
      "plugins", nargs="+", help="Plugin names to enable.")

  disable_parser = subparsers.add_parser(
      "disable", help="Disable plugins (alias for remove).")
  disable_parser.add_argument(
      "plugins", nargs="+", help="Plugin names to disable.")

  subparsers.add_parser("list", help="List available and installed plugins.")
  subparsers.add_parser(
      "fix", help="Re-register all local plugins with absolute paths.")

  args = parser.parse_args()

  if not args.command:
    parser.print_help()
    sys.exit(1)

  platform = get_platform(args.agent)

  if args.command == "list":
    handle_list(platform)
  elif args.command in ("add", "enable"):
    handle_add(args.plugins, platform)
  elif args.command in ("remove", "disable", "rm"):
    handle_remove(args.plugins, platform)
  elif args.command == "update":
    exts = args.plugins or list(get_available_plugins().keys())
    if exts:
      handle_add(exts, platform)
    else:
      print("No plugins available to update.")
  elif args.command == "fix":
    handle_fix(platform)


if __name__ == "__main__":
  main()
