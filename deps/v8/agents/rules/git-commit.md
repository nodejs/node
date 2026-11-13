---
name: git-commit
trigger: always_on
---

# Git Commit Conventions

When drafting or updating commit messages and CL descriptions, follow the
**`cl-description`** skill
([SKILL.md](/agents/shared/skills/cl-description/SKILL.md)) (including 72-column
wrapping via `wrap_lines.py`), with these V8-specific additions and overrides:

## V8 Commit Message Overrides & Additions

- **Mandatory `[component]` Prefix**: Every commit title must start with a
  `[component]` prefix (`[component] Title`, typically lowercase in V8; omitting
  the tag is rejected by `PRESUBMIT.py`). Use `[agents] ...` ONLY for changes to
  the `agents/` directory itself.
- **Body Length & Amendments**: When a body is needed, aim for at most 10 lines.
  When amending an existing commit message, rewrite it into a single cohesive
  summary instead of appending new paragraphs.
- **V8 `TAG=` / `CONV=` Paragraph**: Always include `TAG=agy` and
  `CONV=<conversation_id>` on agent-generated changes. Unlike `Key: Value`
  Gerrit footers, place V8 `KEY=VALUE` tags in their own paragraph immediately
  above the final `Bug:`/`Fixed:` and `Change-Id:` footer block, separated by a
  blank line.
- **Multiple Bugs per Line**: Limit `Bug:` and `Fixed:` lines to at most 4 or 5
  bug IDs per line (Gerrit stops rendering links beyond that); add a second
  `Bug:` or `Fixed:` line if needed.

## Non-Interactive Git Commands

- **Bypass Terminal Editors**: Always run `git` commands non-interactively: pass
  `-m` or `-F` when committing, and prefix commands that may invoke an editor
  with `EDITOR=true` (e.g., `EDITOR=true git commit --amend`).
- **Timeout for Stuck Commands**: If a `git` command shows no progress within 1
  minute, kill the task and retry or report to the user.
- **Explicit Worktree Base Branch**: When creating a worktree via
  `agents/scripts/create_worktree.sh <task_id> [base_branch]`, always explicitly
  pass `[base_branch]` (defaulting to `origin/main`, or the parent branch for
  stacked CLs) to isolate changes from unrelated local commits.
