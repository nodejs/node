---
name: workflow-debugging
description: "Workflow for issue-based debugging in V8. Use when tasked with debugging a specific issue, usually associated with a Buganizer ID or a specific reproduction script. Do not use for performance regressions."
---

# Workflow: Issue Debugging

Use this skill when tasked with debugging a specific issue, usually associated with a Buganizer ID or a specific reproduction script. This specializes the general debugging approach for tracked issues.

## Activation Criteria
-   User provides an issue ID or Buganizer URL.
-   User provides a reproduction script that crashes or behaves unexpectedly.
-   The goal is to find a root cause and propose a fix for a specific bug.

## Core Principles (Inherited & Specialized)

1.  **Strict Orchestration**: The main agent acts purely as a dispatcher. Delegate technical tasks.
2.  **Eager Parallelism**: Initialize Heavy Infrastructure, Conceptual Triage, and Static Research in parallel.
3.  **Issue Association**: When an issue ID is known, use it to label tests and commits.
4.  **Handling Inaccessible URLs & Missing Info**:
    - If a URL cannot be accessed directly (e.g., due to authentication), or if the user just mentions an issue without providing the repro, you MUST prompt for it.
    - Avoid opening a browser to access the page if it is inaccessible through tools.
    - Use separate, sequential interactive prompts (e.g., via `ask_question` or direct message to user) to gather missing information:
        - **Input 1**: The JavaScript test source code.
        - **Input 2**: The relevant `d8` flags or command-line arguments.
    - Rely on user-provided reproduction script or flags.
    - If the user denies or does not provide the information, seek explicit guidance on how to proceed.
    - Store the test source in the workspace (e.g., `scratch/regress-<issue_id>.js`) to avoid modifying the user's environment directly.
5.  **Worktree Isolation**: For issue debugging, you MUST create and switch to an isolated git worktree BEFORE running any builds or tests, to avoid clobbering the main workspace or rebuilding unnecessarily.
6.  **Cross-Pollination & Communication**: The Orchestrator MUST actively relay relevant discoveries and hypotheses between subagents working on overlapping or related tasks to avoid duplicate work and find matching patterns.


## Workflow

### 1. Triage & Parallel Track Initialization
-   Analyze the request and extract the Issue ID if available.
-   **Worktree Setup**: If an issue ID is identified, immediately create and switch to a separate git worktree to isolate the investigation.
-   **Eager Delegation (MANDATORY)**: The Orchestrator MUST proactively spawn subagents to handle all of the following tracks in PARALLEL:
    -   **Track A: Heavy Infrastructure**: Spawn a subagent to build `d8` and prepare GDB/rr in the worktree.
    -   **Track B: Conceptual Triage**: Spawn a subagent to analyze the reproducing JS source.
    -   **Track C: Static Research**: Spawn a subagent to search for related code and spec text, and understand affected areas super deeply.

### 2. Work Branching
-   Spawn subagents for independent dimensions (e.g., Spec vs. Runtime).

### 3. Investigation & Escalation
-   Use GDB to inspect crash state.
-   Escalate unfamiliar concepts immediately.

### 4. Synthesis
-   Combine findings to locate the root cause.

### 5. Fix & Verify
-   Propose a fix following V8 Best Practices.
-   **Architectural Skepticism (MANDATORY)**: Before presenting the fix, the agent MUST explicitly argue *against* its own proposal.
    -   *Skepticism Prompt*: "Is this fix too hasty? Does it accidentally disable a valid optimization path? Am I fixing a symptom (crashing line) rather than the root cause (invariant violation)?"
-   **Deep Reasoning**: If the root cause isn't fully understood, spawn a subagent to reason deeper about why the failing line exists and what invariant it's protecting.
-   **Verify**: Confirm with tests and `d8` flags. Run tests on both debug and release builds if possible to ensure no assertions are violated and performance is not regressed.

### 6. Interactive Review
Before committing or uploading, you MUST present the proposed solution to the user for review.
- **Present the Diff**: Show the exact code changes proposed.
- **Present the Test**: Show the regression test file and where it will be placed.
- **Explain the Root Cause**: Provide a clear explanation of *why* the bug happened.
- **Share Skepticism Results**: Briefly mention the counter-arguments considered during the Skepticism phase and why the proposed fix is still the right choice.
- **Ask for Approval**: Explicitly ask the user if you should proceed with committing and uploading.

### 7. Commit & Upload (Specialized)
Once the fix is approved by the user:

- **Issue Association in Commits**:
    - The commit message must follow the V8 convention.
    - It MUST include a `Bug: <issue_id>` line at the bottom.
    - Example Commit Message:
      ```
      [compiler] Fix crash in Turboshaft during loop unrolling

      The loop unroller failed to handle cases where...

      Bug: 12345
      ```

- **Test Naming and Placement**:
    - Name the reproduction test file `regress-<issue_id>.js`.
    - If there is no issue ID, use a descriptive name like `regress-short-description.js`.
    - Place the test in the appropriate directory:
        - General regressions: `test/mjsunit/regress/`
        - Component-specific regressions (if applicable): e.g., `test/mjsunit/compiler/regress-12345.js`.
    - Ensure the test file contains a standard header:
      ```javascript
      // Copyright 2026 the V8 project authors. All rights reserved.
      // Use of this source code is governed by a BSD-style license that can be
      // found in the LICENSE file.

      // Flags: --allow-natives-syntax --any-other-flags
      ```

- **Finalization and Upload**:
    - Run `git cl format` to ensure style compliance.
    - Create the commit locally.
    - Run `git cl upload` to upload the change list (CL) to Gerrit.
    - Report the CL URL back to the user/caller if the tool output provides it.
