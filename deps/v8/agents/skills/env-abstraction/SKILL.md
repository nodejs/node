---
name: env-abstraction
description: "Handles abstraction of environment-specific commands between Jetski and Gemini-CLI. Use when switching execution context between local terminal environments. Do not use for debugging logic issues."
---

# Environment Abstraction Skill

This skill provides a layer of abstraction over environment-specific tools and commands, allowing skills to be reused across different platforms like `jetski` and `gemini-cli`.

## Core Principles

1.  **Decoupling**: Keep tool calls independent from platform specific implementations inside general workflow skills.
2.  **Detection**: Determine the environment at startup (e.g., via environment variables or tool availability).
3.  **Mapping**: Map abstract actions to concrete commands or tool calls based on the environment.

## Environment Definitions

### Jetski Environment
-   **Characteristics**: Rich set of specialized tools (e.g., `code_search`, `moma_search`, `gdb-mcp`, `v8-utils`).
-   **Usage**: Prefer specialized tools over generic shell commands to maximize efficiency and respect workspace constraints (e.g., avoid heavy searches in large workspaces).

### Gemini-CLI Environment
-   **Characteristics**: May rely more on standard shell commands or a different set of MCP servers.
-   **Usage**: Fall back to standard tools like `grep`, `find`, or standard `gdb` via `run_command` if specialized Jetski tools are unavailable.

## Abstract Action Mapping

When performing common actions, refer to this mapping to choose the correct tool:

| Abstract Action | Jetski Implementation | Gemini-CLI Implementation (Fallback) |
| :--- | :--- | :--- |
| **Code Search** | `code_search` | `grep_search` (if outside Google3) or standard `grep` |
| **File Search** | `find_by_name` (if safe) | Standard `find` or `fd` |
| **Debugging** | `gdb-mcp` | Standard `gdb` via `run_command` |
| **Building** | `tools/dev/gm.py` (use `use_remoteexec=true`) | `tools/dev/gm.py` |
| **Testing** | `tools/run-tests.py` | `tools/run-tests.py` |
| **Starting Agents** | `invoke_subagent` (tool) | `agentapi new-conversation` (CLI) |

## Implementation Guidelines

-   When writing or updating skills, refer to actions by their abstract names.
-   Check for the existence of specialized tools before falling back to generic commands.
-   Document any environment-specific assumptions in the skill file.
