---
name: v8-setup
description: "Handles missing dependencies and configuration for V8 tools. Use when encountering missing tools, test runners, or workspace symlinks. Do not use for C++ coding issues."
---

# Skill: V8 Setup

Use this skill to handle missing dependencies or configuration issues in the V8 environment.

## Workspace Setup for Jetski

Jetski often expects a `.agents` directory or symlink in the V8 workspace root to find skills and rules.

- **Missing `.agents`**: If the workspace only has `agents` (no dot) and `.agents` is missing:
  - **Suggest symlinking individual files**: Advise the user to create individual symlinks for each skill and rule file inside `.agents/skills/` and `.agents/rules/` from the shared `agents/` directory, rather than symlinking the whole folders. This allows the user to mix shared skills with private ones in the same directory.
  - This ensures tools can find the shared skills while preserving the user's ability to customize `.agents`.
## Installation and Configuration of Specialized Tools

The V8 environment relies on specialized tools and MCP servers for advanced workflows. If they are missing, follow these guidelines:

### 1. Crossbench
Crossbench is the central benchmark runner.
- **Check**: Ask the user for the location of the Crossbench directory if not found in a standard location.
- **Installation**: If missing, suggest the user to fetch it using `depot_tools` or clone it from Chromium sources:
  ```bash
  fetch crossbench
  # or
  git clone https://chromium.googlesource.com/crossbench
  ```
- **Configuration**:
  - Check if `cb.py` is executable. You can usually run it directly by referencing its full path: `/path/to/crossbench/cb.py`.
- **MCP Server Installation**:
  - To install `crossbench-mcp`, add it to `mcp_config.json` pointing directly to the executable `cb.py`:
    ```json
    "crossbench-mcp": {
      "command": "/path/to/crossbench/cb.py",
      "args": [
        "mcp"
      ]
    }
    ```
  - It will automatically use the managed environment if run in an environment where `vpython3` is available (like `depot_tools`).

### 2. MCP Servers (v8-utils, gdb-mcp, buganizerMcp, clangd-mcp)
These servers provide advanced capabilities like benchmark running, debugging, and issue tracking.
- **Check**: Verify if they are listed in the available MCP servers at the start of the conversation.
- **Setup/Configuration**:
  - If missing, you cannot install them directly. You MUST ask the user to enable or configure them in their Jetski settings.
  - **clangd-mcp**: Ensure `compile_commands.json` is generated.
  - **gdb-mcp**: Requires GDB to be installed on the host.
  - **v8-utils**: Provides tools like `jsb_run_bench` and `v8log_analyze`.

- **Missing Dependencies**: If a required tool (like `poetry` for Crossbench) is missing or failing, do NOT try to install it yourself. Follow the fallback advice in the specific workflow skill or ask the user for guidance.
