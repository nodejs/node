---
name: workflow_gerrit_fix
description: Workflow for automatically fixing errors or addressing comments on a Gerrit CL requested by the user.
---

# Gerrit Fix & Review Workflow

Use this skill when the user asks to "fix the errors on Gerrit issue X" or "address reviewer comments on CL X" or similar requests targeting a specific Gerrit Change List (CL).

## Core Principles

1.  **Orchestration**: Do not do the work yourself. Break down the task and delegate to subagents.
2.  **Parallelism**: Always identify at least two parallel tracks (e.g., fixing known errors vs. inspecting for other issues, or addressing different comments).
3.  **Isolation**: Always operate in a dedicated worktree for the CL.
4.  **Reproduction**: Always attempt to reproduce the failure locally before applying a fix, to ensure the fix is verified.

## Protocol

### 1. Information Gathering
*   **CQ Results**: Use `gerrit_cq` (from `v8-utils`) to check Commit Queue status.
*   **Presubmit Errors**: Use `gerrit_get_presubmit_errors` (from `pndMcp`) to fetch detailed presubmit results.
*   **Comments**: Use `gerrit_comments` or `gerrit_list_change_comments` to read reviewer feedback or robot comments. This is critical when the goal is to address reviewer comments.

### 2. Workspace Setup
*   **Worktree**: Create a new isolated worktree for the CL to avoid polluting the main workspace.
*   **Checkout**: Patch the CL in the new worktree (e.g., `git cl patch X`).
*   **Rebase**: Rebase the branch to `origin/main` to ensure it builds against the latest code.

### 3. Reproduction & Triage
*   **Reproduction**: Instruct the subagent to reproduce the failure locally (e.g., run the failing test or compile target) to confirm the issue and provide a baseline for verification.
*   **Triage & Delegation**: Break down the identified errors or comments and delegate them to subagents.
    *   **Track A (Fix/Address)**: Delegate the fixing of reported errors or addressing specific reviewer comments to a `self` subagent with full tool access.
    *   **Track B (Scan/Verify)**: Delegate a scan of other modified files or running local tests to ensure no other issues were introduced.

### 4. Verification
*   **Local Build/Test**: Instruct the subagent to verify that the fix compiles locally and the reproduced failure is now resolved.
*   **Upload**: Once verified locally, upload a new patchset to Gerrit to run the remote checks or ask for re-review.

## Example: Fixing Errors
If the user says "fix errors on CL 12345":
1.  Call `gerrit_cq` for CL 12345.
2.  Identify that `v8_linux64_rel` failed with failing tests.
3.  Create worktree `worktrees/cl-12345` and checkout CL.
4.  **Spawn Subagent A to reproduce the failing tests locally.**
5.  Spawn Subagent A to fix the failing tests.
6.  Spawn Subagent B to check other modified files for potential issues.
7.  Once Subagent A reports success and verification (tests pass), report to user and ask if you should upload.

## Example: Addressing Comments
If the user says "address comments on CL 12345":
1.  Call `gerrit_comments` for CL 12345.
2.  Identify a comment requesting a refactor in `src/objects/lookup.cc`.
3.  Create worktree `worktrees/cl-12345` and checkout CL.
4.  Spawn Subagent A to perform the refactor as requested.
5.  Spawn Subagent B to verify it compiles and run relevant unit tests.
6.  Once Subagent A reports success, report to user and ask if you should upload.
