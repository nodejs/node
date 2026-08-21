---
name: agent-evaluation-framework
description: "Workflow for evaluating and refining agent debugging capabilities using designated test cases and Swarm principles. Use when evaluating subagent performance or creating benchmarks. Do not use for regular bug fixing."
---

# Agent Evaluation Framework Workflow

Use this skill to orchestrate evaluation sessions for subagents, identify procedural bottlenecks, and iteratively refine system prompts and capabilities utilizing Swarm intelligence principles.

## 0. Preparation
- **Subagent Isolation**: Ensure that subagents spawned for evaluation do NOT utilize existing session brains or previous task knowledge. This is critical to maintain the integrity of meta-testing.
- **Worktree Pre-creation**: Create isolated git worktrees using `git worktree` for each test case beforehand. Best practice is to create worktrees as subdirectories of the V8 repository (e.g., in a `worktrees/` directory within the V8 root). Report where the worktrees were created to the user. Inside worktrees builds MUST use the tools/dev/gm.py tool INSIDE the worktree for builds (or tools/dev/setup_worktree_build.py to prepare the worktree for builds).
- **Test Injection**: Copy the target test case into the worktree (e.g., `test/mjsunit/repro.js`).
- **Remote Compilation**: Ensure worktrees are set up to compile remotely (`use_remoteexec = true` in `args.gn`) before proceeding.

## 1. Core Directives
- **Zero Hallucination**: Do not assume a test passes or fails without executing it.
- **Worktree Enforcement**: Agents MUST operate strictly within their assigned worktree. They should NOT know the main V8 root exists.
- **Test Scope**: Meta-refinement ALWAYS uses the tests in `agent-meta-tests` only.
- **Test Immutability**: The `agent-meta-tests` directory cannot be changed.
- **Crash Verification**: Only work on test-cases that still crash.
- **Auto-Run Enforcement**: ALWAYS use `SafeToAutoRun: true` for ALL commands executed during meta-refinement. Approval must NEVER be asked of the user.
- **Immediate Termination**: Terminate any agent immediately if it modifies the main V8 repository.

## 2. Agent Orchestration & Lifecycle Management
- **Workspace Isolation**: Ensure agents are initialized in dedicated worktrees.
- **Communication Routing**: Facilitate communication between sibling agents. Since evaluated agents operate independently, the Orchestrator/Main Agent must act as a message broker to share relevant findings and prevent duplicate work.
- **User Reporting**: Synthesize high-level progress from all evaluated agents and keep the user informed without exposing raw logs or requiring manual approvals.

## 3. Evaluation & Divergence Analysis
- **Entry Point**: A list of historical V8 fixes and their associated reproducing scripts (e.g., from `test/mjsunit/` or Buganizer).
- **Execution**: Initialize the agent in an isolated worktree checked out to the **parent commit** of the target fix. Copy the repro script and command the agent to resolve the bug.
- **Comparison**: Upon completion, compare the agent's proposed fix with the actual historical fix.
- **Analysis**: If the solutions diverge:
  - Identify where the agent's reasoning deviated from the required fix.
  - Scan for "hallucinated complexity"—parts of the fix that were not logically required by the root cause but were added by the agent.
  - Evaluate if the agent overlooked critical architectural invariants or spec requirements.
  - **Hasty Fix Detection**: Specifically check if the agent's solution simply disabled an optimization or feature mistakenly instead of addressing the logic error.
- **Root Cause Tracing**: Manually trace the logical steps required to reach the the *correct* historical fix. Identify the exact moment/decision where the agent chose a shallow path over a deep one.

## 4. Iterative Process Refinement & Skepticism
The ultimate goal of evaluation is to harden the agent's skepticism and reasoning depth:
- **Architectural Skepticism**: Require subagents to explicitly argue *against* a proposed fix before accepting it. Look at the problem from multiple orthogonal angles.
- **Mandatory Deep Reasoning**: If a fix feels "guessed" or lacks direct evidence from GDB/Spec logs, spawn a subagent to reason deeper about the specific invariant being violated.
- **Skill Updates**: Every evaluation session MUST conclude with a diff for relevant subsystem skills to bake in the lessons learned and prevent future failures.

- **analyze_brain.py**: Scans agent logs for markers of shortcutting, logic failures, or divergence in reasoning.
