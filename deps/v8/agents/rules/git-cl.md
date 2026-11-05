---
name: git-cl
trigger: model_decision
description: Rules, mandates, and workflows for using git-cl, managing CLs, formatting code, and uploading patches in V8
---

# Git CL Conventions

These rules enforce V8-specific workflow mandates and guardrails when working
with Chromium's `git cl` tool.

For general Chromium Gerrit CL workflows, commands (such as creating commits,
uploading CLs, updating descriptions with `git cl description -n +`, running try
jobs, polling presubmit status, and commenting), and options, refer to the
**`git-cl-helper`** skill
([SKILL.md](/agents/shared/skills/git-cl-helper/SKILL.md)).

## V8 Upload & Commit Mandates

- **Always Format**: Run `git cl format` before creating a commit or uploading a
  CL (as detailed in the `git-cl-helper` skill). If it makes any changes, commit
  them before proceeding with the upload.
- **Uploading CLs**: Use `git cl upload` (as detailed in the `git-cl-helper`
  skill) with `SKIP_GCE_AUTH_FOR_GIT=1 GIT_EDITOR=true` to avoid interactive
  editor or auth prompts:
  - **Initial Upload (Always WIP)**: For a new CL without an existing issue
    association (`git cl issue` is `None`), pass `-T -o wip` so the CL starts in
    **WIP** mode without prompting for a patchset title:
    `SKIP_GCE_AUTH_FOR_GIT=1 GIT_EDITOR=true git cl upload -T -o wip`
  - **Subsequent Uploads (Default)**: While iterating on a CL, running
    self-reviews, fixing bugs/tests, or addressing review comments, pass
    `-t "Brief patchset description"` (and omit `-o wip`, `--ready`, and
    `--commit-description`):
    `SKIP_GCE_AUTH_FOR_GIT=1 GIT_EDITOR=true git cl upload -t "Brief patchset description"`
    This lets Gerrit preserve the CL's existing WIP or Ready-for-Review state
    and online description without switching a CL that is already in review back
    to WIP. Only pass `-o wip` on a subsequent upload if explicitly switching a
    CL back to WIP.
  - **Ready for Review (Non-WIP + Mandatory Dry-Run)**: **ALWAYS obtain explicit
    user confirmation before setting a CL to ready (non-WIP) or passing
    `--ready`.** Only switch a CL to non-WIP once the user explicitly asks to
    mark the CL "ready for review", "unjail/un-WIP", or "send to reviewers", and
    only after a CQ dry-run (`-d` / `--cq-dry-run` or `git cl try`) has passed
    on the current patchset. (User confirmation to *"upload"*, *"trigger a
    dry-run"*, or *"watch the CQ"* is **NOT** confirmation to exit WIP.) Once
    confirmed and the dry-run has passed, exit WIP using `--ready` on upload (if
    uploading a new patchset) or via `gerrit_client.py`:
    `gerrit_client.py rawapi --host https://chromium-review.googlesource.com --path "/changes/<cl_number>/ready" --method POST --accept_status "200,204"`
    If a dry-run fails, do not mark the CL ready; investigate the failing bots
    (inspecting logs via `gerrit_cq`), determine the root cause, and ask the
    user about next steps.
- **Updating Description**: If the changes in a new patchset make the existing
  CL description **out-of-date** or inaccurate, you **MUST** explicitly update
  it in the local git commit message and sync it to Gerrit:
  - **When uploading code changes**: Pass `--commit-description=+` to
    `git cl upload` so the patchset and description update together.
  - **When updating description only**: Amend the local git commit message and
    run `git cl description -n +` (as detailed in the `git-cl-helper` skill).
    **⚠️ CRITICAL SAFETY CONSTRAINT**: Before passing `--commit-description=+`
    or running `git cl description -n +`, you must first inspect the live remote
    description on Gerrit (using either `git cl description -d` or a Gerrit MCP
    tool) to verify whether the user has manually edited it. Carry over any
    human-authored online edits, rationale, or formatting — as well as the
    `Change-Id:` footer line — into your local commit message while surgically
    updating only the outdated technical statements.
- **Commit Message Length Limits**: Keep the commit title at most 72 characters
  (aiming for ~50 characters) and wrap description body lines at 72 characters.
- Verify `git diff @{upstream}...HEAD` is not empty before uploading.

## Safeguards & Code Quality

- **Mandatory Reproducer Rule**: Before running `git cl upload`, you MUST verify
  that the diff contains a valid, working regression test (typically under
  `test/mjsunit/` or its component-specific subfolders like `compiler/`,
  `turboshaft/`, `maglev/`, etc.). Uploading code-modifying CLs without an
  accompanying regression test is strictly forbidden (unless explicitly waived
  by the user or if it's a process-only/documentation change).
- **No Trailing Whitespace**: NEVER add trailing whitespace in any file you
  create or modify.
- **Always Test Before Upload**: Always run tests (using `release` or `optdebug`
  mode, avoiding `debug` mode due to performance) before uploading.
- **ALWAYS keep CLs separate**: If you are working on more than one task at the
  same time always use
  `agents/scripts/create_worktree.sh <task_id> [base_branch]` to create an
  isolated workspace for each independent task or CL. Worktrees are created at
  `REPOSITORY_ROOT/worktrees/`. NEVER upload a CL from a workspace containing
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
  run `git diff --name-only @{upstream}...HEAD` to ensure no unrelated files are
  included. If you see commits or files that are not part of your specific task,
  reset your branch to `@{upstream}` and cherry-pick only your intended commits
  instead of uploading.
- **Verify Alignment**: Verify alignment of the subsystem/context in
  `git cl status` before proceeding, and consult the user if it seems
  mismatched.
- **Process-Only CLs**: For CLs intended to contain ONLY process/documentation
  changes, verify that no code files (e.g., `.cc`, `.h`, `.js`, `.py`) are
  modified using `git diff --name-only @{upstream}...HEAD`.

## Proactive Alert Handling & Post-Upload

- **Proactive Alert Handling**: After uploading, check the presubmit and tryjob
  status:
  - Run `git cl status` after uploading to verify branch alignment and issue
    status.
  - When a CQ dry-run is triggered (`-d` / `--cq-dry-run` or `git cl try`), wait
    for completion asynchronously in a background task (e.g., via
    `vpython3 agents/shared/skills/git-cl-helper/scripts/git_cl_helper.py poll --gerrit_url <GERRIT_URL>`
    or monitoring `git cl status --field=status`) and inspect bot results using
    `gerrit_cq`.
  - If any builds fail, zoom into the failure logs
    (`gerrit_cq(builder="<name>")`), triage whether it is an infrastructure
    issue, a known flake, or a real regression, and ask the user about next
    steps with proposed fixes.
- **Post-Upload**: Provide the `chromium-review.googlesource.com` (or
  `https://crrev.com/c/<number>`) link to the user.

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
  branch (e.g., `agents/scripts/create_worktree.sh <task_id> <parent-branch>` or
  `git branch --set-upstream-to=parent-branch`). The base branch at the bottom
  of the stack tracks `origin/main`.
- **Rebasing**: When the parent branch changes, rebase the child branch on top
  of the updated parent.
- **Unique Change-Ids**: Ensure each CL in the stack has a unique `Change-Id`.
  Make sure to remove `Change-Id` from commit messages of new CLs.

## Helpful Commands

- Use `git cl patch <CL_NUMBER>` to fetch and checkout the latest patchset of a
  CL.
- Use `git cl land` to land the CL after approval.
- Use `git cl description -d` to view the CL description.
- Use `git cl description -n +` to update a CL description on Gerrit from the
  latest local git commit (after checking `git cl description -d` and preserving
  `Change-Id:` and any online edits).
