---
name: clangd-setup
description: Handles installation and configuration of clangd-mcp for V8. Use when clangd-mcp is missing or needs configuration for the current workspace. Do not use for general C++ editing.
---

# Clangd Setup Skill

Use this skill when `clangd-mcp` is missing or needs configuration for the
current workspace.

## Core Principles

- **Proactive Execution**: Do all necessary work to set up `clangd` once user
  approval is obtained.
- **Workspace Specific**: Configuration (`compile_commands.json`) must be
  generated for the specific V8 checkout.

## Setup Workflow

Follow these steps to ensure `clangd` is ready:

### 1. Check and Install `clangd-mcp`

1. Check if the `clangd-mcp` server is available in the list of registered
   servers.
2. If not available:
   - Ask the user: "Would you like me to install and configure `clangd-mcp` for
     this workspace?"
   - If approved, proceed to installation steps if known, or ask the user for
     guidance on how to enable it in their specific environment.

### 2. Generate `compile_commands.json`

`clangd` requires a compilation database to understand the codebase.

1. **Identify Build Directory**: Check for common build directories like
   `out/x64.debug`, `out/x64.release`.
2. **Check for Existing File**: Check if `compile_commands.json` already exists
   in the identified build directory or at the project root.
3. **Generate if Missing**: If it doesn't exist, you MUST attempt to generate
   it.
   - **Method**: Run `gn gen <build_dir> --export-compile-commands`.
     - Example: `gn gen out/x64.debug --export-compile-commands`
   - Ask the user before running the command to ensure it targets the correct
     build directory they are working with.

### 3. Verify Configuration

1. Once `compile_commands.json` is generated, verify that `clangd-mcp` tools
   (like `find_symbol`, `get_definition`) return meaningful results for a known
   symbol in the codebase.

## Troubleshooting

- If `clangd` cannot find headers, ensure `compile_commands.json` is at the root
  or linked correctly, and that the build has been run at least once to generate
  all necessary headers (like Torque generated files).
