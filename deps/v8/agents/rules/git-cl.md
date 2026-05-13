---
name: git-cl
trigger: always_on
---

# Git CL Conventions

These rules ensure correct usage of the Chromium-specific `git cl` tool in V8.

## Git Command Usage (git cl)

- **Always Format**: Run `git cl format` before creating a commit or uploading a
  CL.
- **Uploading to Gerrit**:
  - **Wrap Commit Message**: Ensure commit message descriptions are strictly
    wrapped at 78 characters.
  - **Always use `-t` and/or `--commit-description`** for `git cl upload`
    instead of the deprecated `-m` flag, to avoid interactive editors.
  - **Ensure Non-Interactive Behavior**: When committing, amending, or uploading
    (e.g., `git cl upload`), prefix the command with `EDITOR=cat` (e.g.,
    `EDITOR=cat git cl upload`) if there is a risk of an editor opening.
  - Provide a descriptive patch message:
    `git cl upload -t "Brief description of what changed"`.
  - **Initial Upload**: On the first upload of a new CL, use
    `git cl upload --commit-description=+` to set the description from the
    commit message.
  - **Subsequent Uploads**: On subsequent uploads (new patchsets), always use
    `-t` to provide a patchset message that very briefly summarizes the
    differences since the last patchset (e.g.,
    `git cl upload -t "Address review comments"`,
    `git cl upload -t "Fix lint in parser.cc"`,
    `git cl upload -t "Add unit test for edge case"`). Avoid using
    `--commit-description=+` by default, as it can combine messages or overwrite
    the main description incorrectly. Preferred is omitting it unless modifying
    description intentionally.
  - **Updating Description**: If the changes in a new patchset make the existing
    CL description **out-of-date** or inaccurate, you **MUST** explicitly update
    it by passing `--commit-description="New cohesive description content"`.
  - Verify `git diff` is not empty before uploading.
- **Safeguards & Code Quality**:
  - **No Trailing Whitespace**: NEVER add trailing whitespace in any file you
    create or modify.
  - **Always Test Before Upload**: Always run tests (at least in release mode)
    before uploading, unless conditional testing (e.g., via `gm.py` or
    `check_and_test.sh`) indicates that it is unnecessary.
  - **ALWAYS keep CLs separate**: Use standard `git worktree` to create an
    isolated workspace for each independent task or CL. NEVER upload a CL from a
    workspace containing unrelated modifications. Always verify that the diff
    relative to upstream contains ONLY the intended changes before uploading.
  - **Consult user on external changes**: When `git cl upload` detects external
    changes on Gerrit and prompts to fetch them or override, the agent MUST stop
    and ask the user for guidance, unless explicitly instructed otherwise.
  - **Verify Isolation Strategy**: Before performing any CL operations, ensure
    you have consulted the user regarding the isolation strategy as defined in
    the `v8-workflow` skill (e.g., branch vs. worktree).
  - **Preserve Issue Number**: Preserve the issue number when reusing a CL
    instead of running `git cl issue 0`.
  - **Verify Issue Description**: Before uploading, run `git cl status` to
    verify the Issue Description matches your task.
  - **Review Modified Files**: Review the modified files (`git show --stat`) or
    run `git diff --name-only origin/main..HEAD` to ensure no unrelated files
    are included. If you see commits or files that are not part of your specific
    task, reset your branch to `origin/main` and cherry-pick only your intended
    commits instead of uploading.
  - **Verify Alignment**: Verify alignment of the subsystem/context in
    `git cl status` before proceeding, and consult the user if it seems
    mismatched.
  - **Process-Only CLs**: For CLs intended to contain ONLY process/documentation
    changes, verify that no code files (e.g., `.cc`, `.h`, `.js`, `.py`) are
    modified using `git diff --name-only origin/main`.
- **Proactive Alert Handling**: Always run `git cl status` after uploading. If
  you identify failing checks or try jobs, proactively suggest addressing these
  alerts to the user.
- **Post-Upload**: Provide the `chromium-review.googlesource.com` link to the
  user.
- **Retrieving and Addressing Comments**:
  - Use this rule when asked about retrieving, addressing, reviewing, or fixing
    comments on the current CL.
  - If a CL number is specified, check first that it matches the current
    associated CL ID (find with `git cl issue`).
  - If there is no associated CL, ask the user whether you should associate it
    first with `git cl issue <cl_number>`.
  - Once associated (or if using current), fetch comments using
    `git cl comments`.
  - Note: If you have access to a dedicated tool for reviewing comments (e.g., a
    Gerrit MCP tool), prefer using that instead of `git cl comments`.

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
- Use `git cl desc` to view or edit the CL description.
