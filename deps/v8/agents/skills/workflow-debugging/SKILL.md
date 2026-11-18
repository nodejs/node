---
name: workflow-debugging
description: Workflow for issue-based debugging in V8. Use when tasked with debugging a specific issue, usually associated with a Buganizer ID or a specific reproduction script. Do not use for performance regressions.
---

# Workflow: Issue Debugging

Use this skill when tasked with debugging a specific issue, usually associated
with a Buganizer ID or a specific reproduction script. This specializes the
general debugging approach for tracked issues.

## Activation Criteria

- User provides an issue ID or Buganizer URL.
- User provides a reproduction script that crashes or behaves unexpectedly.
- The goal is to find a root cause and propose a fix for a specific bug.

## Core Principles (Inherited & Specialized)

1. **Flexible Execution**: Investigate directly or delegate subtasks to
   subagents as you judge best for the issue; if waiting on subagents or
   background jobs, wait reactively for completion notifications instead of
   polling output directories.
2. **Issue Association**: When an issue ID is known, use it to label tests and
   commits.
3. **Handling Inaccessible URLs & Missing Info**:
   - If a URL cannot be accessed directly (e.g., due to authentication), or if
     the intake phase detects that critical attachments (repro script, flags,
     etc.) cannot be accessed or are redacted, stop immediately and ask the user
     to provide them manually.
   - Avoid opening a browser to access the page if it is inaccessible through
     tools.
   - Use separate, sequential interactive prompts (e.g., via `ask_question` or
     direct message to user) to gather missing information:
     - **Input 1**: The JavaScript test source code.
     - **Input 2**: The relevant `d8` flags or command-line arguments.
   - Rely on user-provided reproduction script or flags.
   - If the user denies or does not provide the information, seek explicit
     guidance on how to proceed.
   - Store the test source in the workspace (e.g.,
     `scratch/regress-<issue_id>.js`) to avoid modifying the user's environment
     directly.
4. **Worktree Isolation**: If you debug with a repository in use (e.g., with
   unrelated changes) or debug multiple issues at once you MUST create and
   switch to an isolated git worktree using
   `agents/scripts/create_worktree.sh <issue_id>` BEFORE running any builds or
   tests, to avoid clobbering the main workspace or rebuilding unnecessarily.

## Workflow

### 1. Worktree Setup & Initial Triage

- Analyze the request and extract the Issue ID if available.
- **Worktree Setup**: If an issue ID is identified, create and switch to a
  separate git worktree using `agents/scripts/create_worktree.sh <issue_id>` to
  isolate the investigation.
- **Initial Triage**: Kick off any needed `gm.py` build asynchronously and begin
  inspecting the reproducing JS source, stack trace, and relevant C++ code
  directly or with subagents as you judge best.

### 2. Investigation & Escalation

- **Reproduction Verification (MANDATORY)**: Before launching GDB or doing any
  deep debugging, you MUST first execute the reproduction script normally
  (without GDB) using your compiled `d8` binary to verify that the crash indeed
  reproduces locally. This establishes the baseline.
- Use GDB to inspect crash state.
- Escalate unfamiliar concepts immediately.

### 3. Synthesis

- Combine findings to locate the root cause.

### 4. Fix & Verify

- Propose a fix following V8 Best Practices.
- **Create a Regression Test**: Design and implement a clean, semantic
  regression test. This test must be developed and verified to crash/fail on the
  unfixed codebase and pass cleanly on the fixed codebase.
- **Architectural Skepticism (MANDATORY)**: Before presenting the fix, the agent
  MUST explicitly argue *against* its own proposal.
  - *Skepticism Prompt*: "Is this fix too hasty? Does it accidentally disable a
    valid optimization path? Am I fixing a symptom (crashing line) rather than
    the root cause (invariant violation)?"
- **Root-Cause Verification**: Trace upstream in the relevant C++ code and
  verify why the failing line exists and what invariant it protects.
- **Check & Update Documentation (`docs/`)**: Search `docs/`
  (`git --no-pager grep` across `docs/`) to check whether the bug fix or newly
  uncovered invariant changes, expands, or corrects any V8 architecture, data
  structure, pipeline pass, bytecode/IR node, flag, or cross-tier contract
  documented in `docs/` (or whether any `docs/` file read during triage had
  stale symbols or invariants). Update the relevant `.md` file(s) in `docs/` as
  part of the fix (keep `docs/security/triaging.md` unchanged unless explicitly
  requested by the user).
- **Verify**: Confirm with tests and `d8` flags. Run tests on both debug and
  release builds if possible to ensure no assertions are violated and
  performance is not regressed.

### 5. Interactive Review

Before committing or uploading, you MUST present the proposed solution to the
user for review.

- **Present the Diff**: Show the exact code changes proposed (and any
  accompanying `docs/` updates).
- **Present the Test**: Show the working regression test file and where it will
  be placed. A bug fix must **always** be accompanied by a working regression
  test. Proposing a fix without a reproducer is strictly forbidden unless the
  user has explicitly waived it.
- **Explain the Root Cause**: Provide a clear explanation of *why* the bug
  happened.
- **Share Skepticism Results**: Briefly mention the counter-arguments considered
  during the Skepticism phase and why the proposed fix is still the right
  choice.
- **Ask for Approval**: Explicitly ask the user if you should proceed with
  committing and uploading.

### 6. Commit & Upload (Specialized)

Once the fix is approved by the user:

- **Issue Association in Commits**:

  - The commit message must follow the V8 convention.
  - When a CL fixes a bug, it MUST include a `Fixed: <issue_id>` line at the
    bottom (rather than `Bug: <issue_id>`).
  - Example Commit Message:
    ```
    [compiler] Fix crash in Turboshaft during loop unrolling

    The loop unroller failed to handle cases where...

    Fixed: 12345
    ```

- **Test Naming and Placement**:

  - Name the reproduction test file `regress-<issue_id>.js`.
  - If there is no issue ID, use a descriptive name like
    `regress-short-description.js`.
  - Place the test in the appropriate directory:
    - General regressions: `test/mjsunit/regress/`
    - Component-specific regressions (if applicable): e.g.,
      `test/mjsunit/compiler/regress-12345.js`.
  - Ensure the test file contains a standard header:
    ```javascript
    // Copyright 2026 the V8 project authors. All rights reserved.
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.

    // Flags: --allow-natives-syntax --any-other-flags
    ```

- **Finalization and Upload**:

  - Verify that any affected `.md` files in `docs/` have been updated to match
    the code changes.
  - Run `git cl format` to ensure style compliance.
  - Create the commit locally.
  - Follow git-cl rules to upload the change list (CL) to Gerrit.
  - Report the CL URL back to the user/caller if the tool output provides it.
