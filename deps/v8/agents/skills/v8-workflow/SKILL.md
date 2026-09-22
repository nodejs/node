---
name: v8-workflow
description: Manages git task isolation and environment setup in V8. Use at the start of any new task or bug fix. Do not use for general C++ editing.
---

# V8 Workflow Skill

This skill defines the standard process for setting up and managing your
development environment in V8.

## 1. Task Initialization (Isolation Strategy)

Before starting any feature or bug fix, you MUST decide on a strategy:

- **Isolated Strategy (Recommended)**: Create a dedicated git worktree and
  branch. This prevents cross-contamination between tasks. Use
  `create_worktree.sh`.

- **Reuse Strategy**: Work in the current directory and branch.

  - Use this only if the user explicitly requests to continue previous work or
    prefers a single-branch workflow.

- **Issue Reset**: If starting a *new* task without a worktree, always ask
  before resetting the `git cl` issue.

## 2. Path Management in Worktrees

When using an isolated worktree (e.g., `worktrees/task-fix-10010110`), you MUST
ensure that all file-modifying tools (`replace`, `write_file`, etc.) target the
correct directory:

- **Explicit Prefixing**: Always prefix `file_path` arguments with the worktree
  directory. For example, use `worktrees/task-fix-10010110/src/wasm/...` instead
  of `src/wasm/...`.
- **Tool Context**: Be aware that relative paths starting with `src/` or `test/`
  will default to the main repository's root, violating task isolation.

## 3. Environment Setup

Once the branch is ready, ensure your local environment is configured for V8
development:

- **depot_tools**: Ensure it is in your PATH.
  - `export PATH=$PATH:$HOME/depot_tools`
- **Output Directory**: Use standard V8 output directories (e.g.,
  `out/x64.debug`).

## 4. Available Tools

- **[create_worktree.sh](../../scripts/create_worktree.sh)**: Automates task
  isolation and setup. Automatically detects whether the repository is in a
  standard Git checkout (creating a Git worktree) or a Rift Btrfs workspace
  (forking an isolated Btrfs subvolume). Use this at the start of every new
  task.
- **[cleanup_worktree.sh](../../scripts/cleanup_worktree.sh)**: Safely removes
  task workspaces (Git worktrees or Rift subvolumes) and prunes remnants once a
  CL is landed or abandoned.
