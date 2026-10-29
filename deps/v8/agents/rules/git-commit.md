---
name: git-commit
trigger: always_on
---

# Git Commit Conventions

These rules ensure correct formatting of commits and usage of standard Git
commands in V8.

## Commit Message Format

- **Title**: Must follow the format `[component] Title`.
  - Use `[agents] title` ONLY for changes to the agent automation suite itself.
- **Description**: Provide a clear explanation of "why" and "what". Wrap lines
  at 72 characters. Focus on content and effects, not the process. Highlight the
  rationale and key non-obvious design decisions.
  - Maintain a cohesive summary: Rewrite or integrate new descriptions into a
    cohesive summary instead of simply appending them.
- **Tone**: Use concise, declarative, and natural engineering language.
- **Tags**: All tag-like lines (e.g., `TAG=`, `BUG=`, `CONV=`) must be at the
  very bottom of the CL description.
  - Always include `TAG=agy` and the Conversation ID for agent-generated
    changes.
  - Ensure the `Change-Id` footer is preserved in the commit message.
  - **Bug Lines**:
    - For **public bugs** (e.g., `issues.chromium.org`), use the plain numerical
      bug ID ONLY (e.g., `Bug: 509339491`) without any prefix.
    - For **internal bugs** (e.g., Google-internal Buganizer), use the `b:`
      prefix (e.g., `Bug: b:349680002`).
    - When listing multiple bugs, limit bug listings to a maximum of 4 or 5 per
      line (Gerrit stops rendering them as links). If you have more, add a
      second `Bug:` line.

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
  `origin/main`). When creating a new worktree or branch (e.g.,
  `git worktree add -b branch_name path origin/main`), **always explicitly
  specify `origin/main`** as the base commit to ensure isolation from unrelated
  local commits.
