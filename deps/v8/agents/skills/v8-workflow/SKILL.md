---
name: v8-workflow
description: "Manages git task isolation and environment setup in V8. Use at the start of any new task or bug fix. Do not use for general C++ editing."
---

# V8 Workflow Skill

This skill defines the standard process for setting up and managing your development environment in V8.

## 1. Task Initialization (Isolation Strategy)

Before starting any feature or bug fix, you MUST PROMPT the user to choose an isolation strategy:

- **Isolated Strategy (Recommended)**: Create a dedicated git worktree and branch. This prevents cross-contamination between tasks.
  - Ask: "Would you like me to set up a dedicated worktree and fresh branch for this task?"
  - Use `new_git_task.sh` if accepted.
- **Reuse Strategy**: Work in the current directory and branch.
  - Use this only if the user explicitly requests to continue previous work or prefers a single-branch workflow.
  - Ask: "Would you like to reuse the current branch and CL association?"

- **Issue Reset**: If starting a *new* task without a worktree, always ask before resetting the `git cl` issue:
  - Ask: "Should I reset the current git cl issue to start clean?"

## 2. Environment Setup

Once the branch is ready, ensure your local environment is configured for V8 development:

- **depot_tools**: Ensure it is in your PATH.
  - `export PATH=$PATH:$HOME/depot_tools`
- **Output Directory**: Use standard V8 output directories (e.g., `out/x64.debug`).

## 3. Available Tools

- **[new_git_task.sh](../../scripts/new_git_task.sh)**: Automates the git worktree isolation and setup process. Use this at the start of every new task.
- **[cleanup_git_task.sh](../../scripts/cleanup_git_task.sh)**: Safely removes task worktrees and prunes remnants once a CL is landed or abandoned.
