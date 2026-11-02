---
name: git-commit
trigger: always_on
---

# Git Commit Conventions

These rules ensure correct formatting of commits and usage of standard Git
commands in V8.

## Commit Message Format

- **Title**: Must follow the format `[component] Title`.
  - Keep the title short, concise, and punchy: aim for ~50 characters, and at
    most 72 characters (including the `[component]` prefix). Prefer high-level
    intent over verbose implementation details.
  - Use `[agents] title` ONLY for changes to the agent automation suite itself.
- **Description**: Keep the body concise (aim for at most 10 lines) and include
  only essential information. Explain "why" and the high-level "what" without
  restating details that are obvious from the diff. Focus on content, rationale,
  and key non-obvious design decisions, not the process. Wrap lines at 72
  characters.
  - Maintain a cohesive summary: Rewrite or integrate new descriptions into a
    cohesive summary instead of simply appending them.
- **Tone**: Use concise, declarative, and natural engineering language.
- **Tags**: All tag-like lines (e.g., `TAG=`, `BUG=`, `CONV=`) must be at the
  very bottom of the CL description.
  - Always include `TAG=agy` and the Conversation ID for agent-generated
    changes.
  - Ensure the `Change-Id` footer is preserved in the commit message.
  - **Bug Lines**:
    - When a CL fixes a bug, write `Fixed: <bug_id>` (e.g., `Fixed: 509339491`).
      Write `Bug:` when the CL is related to a bug without fully resolving it.
    - For **public bugs** (e.g., `issues.chromium.org`), use the plain numerical
      bug ID ONLY (e.g., `Fixed: 509339491` or `Bug: 509339491`) without any
      prefix.
    - For **internal bugs** (e.g., Google-internal Buganizer), use the `b:`
      prefix (e.g., `Fixed: b:349680002` or `Bug: b:349680002`).
    - When listing multiple bugs, limit bug listings to a maximum of 4 or 5 per
      line (Gerrit stops rendering them as links). If you have more, add a
      second `Fixed:` or `Bug:` line.

## Git Command Usage & Avoiding Interactive Hangs

- **Ensure Non-Interactive Behavior**: Jetski cannot interact with
  terminal-based editors. Always run commands non-interactively.
  - Bypass editors by prefixing with `EDITOR=true` (e.g.,
    `EDITOR=true git commit --amend`).
  - Use `-m` or `-F` for commit messages to avoid opening an editor.
  - Use `git --no-pager` or `PAGER=cat` for commands with long output (e.g.,
    `git branch`, `git log`).
- **Timeout for Stuck Commands**: If a git command does not show progress within
  1 minute, kill the task and retry or report to the user.
- **Branching**: Create a new branch for unrelated changes. Keep changes focused
  and isolated to specific CLs. Preferably, inform the user about the unrelated
  changes and ask how they want to proceed (e.g., create new branch, discard, or
  hold) before uploading.
- **Base on Clean Upstream**: Base new branches on a clean upstream (e.g.,
  `origin/main`, or the parent branch for stacked CLs). When creating a new
  worktree or branch (e.g., via
  `agents/scripts/create_worktree.sh <task_id> [base_branch]`), **always
  explicitly specify the base branch** (defaulting to `origin/main`) to ensure
  isolation from unrelated local commits.
