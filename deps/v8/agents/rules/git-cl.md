---
name: git-cl
trigger: model_decision
description: Rules, mandates, and workflows for using git-cl, managing CLs, formatting code, and uploading patches in V8
---

# Git CL Conventions

These rules enforce V8-specific workflow mandates and guardrails when working
with Chromium's `git cl` tool.

For general Chromium Gerrit CL workflows, commands (such as creating commits,
running try jobs, polling presubmit status, and commenting), and options, refer
to the **`git-cl-helper`** skill
([SKILL.md](agents/shared/skills/git-cl-helper/SKILL.md)).

## V8 Upload & Commit Mandates

- **Always Format**: Run `git cl format` before creating a commit or uploading a
  CL (as detailed in the `git-cl-helper` skill). Note that `upload_cl.sh`
  enforces formatting compliance and will abort if uncommitted formatting diffs
  exist.
- **Upload Script**: In V8, all initial and patchset uploads MUST be performed
  using
  `agents/scripts/upload_cl.sh <new|cur> <check|nocheck> [patchset_message] [additional flags...]`.
  This script enforces formatting checks, performs non-interactive uploads, runs
  release test checks, and validates commit description line lengths.
  - **Initial Upload**: For a new CL without an existing issue association,
    execute `agents/scripts/upload_cl.sh new check`.
  - **Subsequent Uploads**: For updating an existing patchset, run
    `agents/scripts/upload_cl.sh cur check "Brief patchset description"`.
  - **Checks**: The "check" flag runs all mjsunit tests. When no code was
    updated, or tests were already successfully run, "nocheck" can be passed.
- **Updating Description**: If the changes in a new patchset make the existing
  CL description **out-of-date** or inaccurate, you **MUST** explicitly update
  it. Choose the approach based on your task:
  - **When uploading code changes (Default)**: Pass `--commit-description="..."`
    directly to `upload_cl.sh` so the patchset and description update atomically
    in one step:
    `agents/scripts/upload_cl.sh cur check "Patchset title" --commit-description="New cohesive description content"`
  - **When updating description only**: If you are refining wording, fixing
    typos, or updating tags without uploading new code changes, run the
    standalone editor script:
    `agents/scripts/edit_cl_description.sh "New cohesive description content"`
    **⚠️ CRITICAL SAFETY CONSTRAINT**: Before passing `--commit-description` or
    running `edit_cl_description.sh`, you must first inspect the live remote
    description on Gerrit (using either `git cl desc -d` or the
    `gerrit_get_change_details` MCP tool) to verify whether the user has
    manually edited it. If manual user edits or refinements are detected, you
    must carefully evaluate them: preserve any human-authored background
    context, rationale, or formatting, while surgically updating only the
    outdated technical statements resulting from the new patchset.
- **Wrap Commit Message**: Ensure commit message descriptions are strictly
  wrapped at 78 characters. `upload_cl.sh` automatically enforces this limit.
- Verify `git diff` is not empty before uploading.

## Safeguards & Code Quality

- **Mandatory Reproducer Rule**: Before running `git cl upload` or
  `upload_cl.sh`, you MUST verify that the diff contains a valid, working
  regression test (typically under `test/mjsunit/` or its component-specific
  subfolders like `compiler/`, `turboshaft/`, `maglev/`, etc.). Uploading
  code-modifying CLs without an accompanying regression test is strictly
  forbidden (unless explicitly waived by the user or if it's a
  process-only/documentation change).
- **No Trailing Whitespace**: NEVER add trailing whitespace in any file you
  create or modify.
- **Always Test Before Upload**: Always run tests (using `release` or `optdebug`
  mode, avoiding `debug` mode due to performance) before uploading.
- **ALWAYS keep CLs separate**: If you are working on more than one task at the
  same time always use `agents/scripts/create_worktree.sh <task_id>` to create
  an isolated workspace for each independent task or CL. Worktrees are created
  at `REPOSITORY_ROOT/worktrees/`. NEVER upload a CL from a workspace containing
  unrelated modifications. Always verify that the diff relative to upstream
  contains ONLY the intended changes before uploading.
- **Consult user on external changes**: When `git cl upload` detects external
  changes on Gerrit and prompts to fetch them or override, the agent MUST stop
  and ask the user for guidance, unless explicitly instructed otherwise.
- **Verify Isolation Strategy**: Before performing any CL operations, ensure you
  have consulted the user regarding the isolation strategy as defined in the
  `v8-workflow` skill (e.g., branch vs. worktree).
- **Preserve Issue Number**: Preserve the issue number when reusing a CL instead
  of running `git cl issue 0`.
- **Verify Issue Description**: Before uploading, run `git cl status` to verify
  the Issue Description matches your task.
- **Review Modified Files**: Review the modified files (`git show --stat`) or
  run `git diff --name-only origin/main..HEAD` to ensure no unrelated files are
  included. If you see commits or files that are not part of your specific task,
  reset your branch to `origin/main` and cherry-pick only your intended commits
  instead of uploading.
- **Verify Alignment**: Verify alignment of the subsystem/context in
  `git cl status` before proceeding, and consult the user if it seems
  mismatched.
- **Process-Only CLs**: For CLs intended to contain ONLY process/documentation
  changes, verify that no code files (e.g., `.cc`, `.h`, `.js`, `.py`) are
  modified using `git diff --name-only origin/main`.

## Proactive Alert Handling & Post-Upload

- **Proactive Alert Handling**: After uploading, check the presubmit and tryjob
  status. Choose the approach based on your situation:
  - **Immediate Check (Default)**: Run `git cl status` immediately after
    uploading to verify branch alignment and quickly check high-level issue
    status.
  - **When Waiting for Trybots**: If the user explicitly asks to monitor or wait
    for tryjobs to complete, or if you need detailed Buildbucket failure logs to
    debug presubmit build errors, run the polling script from the
    `git-cl-helper` skill:
    `vpython3 agents/shared/skills/git-cl-helper/scripts/git_cl_helper.py poll --gerrit_url <URL>`
    If you identify failing checks or try jobs, proactively suggest addressing
    these alerts to the user.
- **Post-Upload**: Provide the `chromium-review.googlesource.com` link to the
  user.

## Retrieving and Addressing Comments

- Use this rule when asked about retrieving, addressing, reviewing, or fixing
  comments on the current CL.
- If a CL number is specified, check first that it matches the current
  associated CL ID (find with `git cl issue`).
- If there is no associated CL, ask the user whether you should associate it
  first with `git cl issue <cl_number>`.
- Note: If you have access to a dedicated tool for reviewing comments (e.g., a
  Gerrit MCP tool), prefer using that. Otherwise, follow the instructions in the
  **`git-cl-helper`** skill for fetching and replying to comments with
  `git cl comments`.

## Stacked CL Workflow

When working on a chain of dependent changes (stacked CLs):

- **Upstream Configuration**: Set the upstream of a child branch to its parent
  branch (e.g., `git branch --set-upstream-to=parent-branch`). The base branch
  at the bottom of the stack tracks `origin/main`.
- **Rebasing**: When the parent branch changes, rebase the child branch on top
  of the updated parent.
- **Unique Change-Ids**: Ensure each CL in the stack has a unique `Change-Id`.
  Make sure to remove `Change-Id` from commit messages of new CLs.

## Helpful Commands

- Use `git cl patch <CL_NUMBER>` to fetch and checkout the latest patchset of a
  CL.
- Use `git cl land` to land the CL after approval.
- Use `git cl desc -d` to view the CL description.
- Use `agents/scripts/edit_cl_description.sh "New description"` (or `-f <file>`)
  to non-interactively update a CL description after validating line lengths and
  code formatting.
