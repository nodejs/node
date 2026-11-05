---
name: cl-priority-prefix
description: Enforces the [P0]-[P4] CL priority prefix workflow for V8 Gerrit CLs. Always use whenever drafting commit messages, running git commit, uploading CLs with git cl upload, or preparing CLs for submission (Commit-Queue+2, git cl set-commit, or git cl land). Do not use for read-only git log inspection.
---

# CL Priority Prefix Workflow (`cl-priority-prefix`)

This skill defines the `[P0]`–`[P4]` CL priority prefix workflow enforced by
V8's `PRESUBMIT.py` (`_CheckPriorityPrefix`) when
`git config --bool v8.presubmit-priority-prefix` is `true` (or
`V8_PRESUBMIT_PRIORITY_PREFIX=1`).

## Step-by-Step Procedure

### Step 1: Determine the CL Priority Before Uploading

Before running `git cl upload` on a new CL:

1. **When the CL has a `Fixed:` or `Bug:` footer**: Use your available bug or
   issue-tracker skills/tools to look up the priority (`P0`–`P4`) of the
   associated issue(s). When multiple bugs are listed, use the highest priority
   (lowest P-number, e.g., `P1` over `P2`).
2. **When the CL does not have an associated bug** (or if the bug priority
   cannot be determined): Ask the user what priority (`P0`–`P4`) the CL has
   before running `git cl upload`.

### Step 2: Prefix the Commit Title on Upload and During Review

Format the first line of the commit message with the `[P<0-4>]` prefix
immediately before the `[<component>]` tag:

```text
[P<0-4>][<component>] <Title>
```

Both `[P1][compiler] Fix bug` and `[P1] [compiler] Fix bug` are accepted by
`PRESUBMIT.py`, as long as the entire title line stays within 72 characters.
Keep the `[P<0-4>]` prefix on the CL while uploading patchsets and running CQ
dry-runs (`Commit-Queue+1` / `git cl try`).

### Step 3: Strip the Priority Prefix Before Merging

`PRESUBMIT.py` (`CheckChangeOnCommit` on non-dry-run commits) rejects any CL
whose commit title still starts with `[P0]`–`[P4]`. Before setting
`Commit-Queue+2` (`git cl set-commit`) or running `git cl land`, run
[strip_priority_prefix.py](scripts/strip_priority_prefix.py) to strip the
`[P<0-4>]` prefix from both the local commit and the Gerrit CL description:

```bash
vpython3 agents/skills/cl-priority-prefix/scripts/strip_priority_prefix.py
```
