---
name: agent-self-improvement
description: Workflow for agent self-improvement via isolated execution, evaluation, and process refinement. Use when evaluating historical bugs or self-correcting skills. Do not use for new feature development.
---

# Agent Self-Improvement Workflow

Use this skill to orchestrate self-improvement sessions for agents. The workflow
involves running debugging or implementation tasks in isolation, evaluating the
results against known good solutions, and identifying improvements in the
various layers of the system.

## Activation Criteria

- The task involves analyzing agent behavior, evaluating agent performance on
  historical issues, or updating skills and rules based on past runs
  (meta-refinement).
- Agents should detect when they are performing self-evaluation (e.g., when
  asked to analyze logs of other agents or refine skills based on failures) and
  activate this skill.

## 1. Core Principles

- **Isolation**: Subagents must run in isolated environments (e.g., separate git
  worktrees) to prevent interference and ensure realistic testing. They MUST
  ignore any gathered knowledge (KIs) and session brains from the outside to
  ensure a self-contained test-run.
- **Evaluation**: A separate evaluation agent analyzes the work of the execution
  agent.
- **Layered Improvement**: The goal is to identify specific procedural or
  knowledge gaps across tools, skills, and prompts to get to a better solution.
- **Anonymization & Generalization**: When updating skills or rules based on
  self-evaluation, avoid recording anything specific to the user or the detailed
  user's installation (e.g., local paths).
- **Process Abstraction**: Keep principles and process improvements abstract and
  procedural. Avoid recording specific tests used, specific issue IDs debugged,
  or specific components affected in the general skill files.

## 2. Workflow: Isolated Execution

- **Trigger**: This workflow can be triggered by a new capability to test, a
  reported bug in agent behavior, or as a regular benchmark using existing CLs
  (Changelists).
- **Setup Isolation**:
  - Create a separate worktree for the subagent using standard workspace tools.
  - **Example (Reimposition from CL)**: If starting from an existing CL:
    - Check out the **parent commit** of the CL (without the fix).
    - Extract the **regression test** or reproduction script from the CL.
    - Place the test in the isolated worktree.
- **Launch Subagent**:
  - Spawn a subagent with the specific task (e.g., "Fix the crash reproducer in
    `test/mjsunit/...`").
  - The subagent should follow the standard `workflow_debugging` or relevant
    skill.
  - The subagent operates independently and reports back its final result (code
    changes, analysis) when complete.

## 3. Workflow: Evaluation

- **Trigger**: Triggered automatically after the execution subagent completes
  its task.
- **Launch Evaluation Agent**:
  - Spawn an evaluation agent (or the main agent takes this role).
  - Provide the evaluation agent with:
    - The execution subagent's logs and final result.
    - The reference solution (e.g., the actual good fix from the original CL).
- **Process & Result Assessment**:
  - The evaluation agent compares the subagent's result with the reference
    solution.
  - **Divergence Analysis**:
    - Did the subagent find the same root cause?
    - Is the proposed fix equivalent, better, or worse?
    - Did the subagent take unnecessary shortcuts or hallucinate complexity?
  - **Layered Improvement Analysis**: The evaluation agent must analyze how the
    process could be improved in the various layers to get to a better solution:
    - **Tool Layer**: Did the agent lack necessary tools or misuse existing
      ones?
    - **Skill Layer**: Did the agent fail to follow skill instructions, or are
      the skills missing critical advice?
    - **Prompt/Orchestration Layer**: Was the initial prompt unclear, or did the
      orchestrator fail to coordinate effectively?

## 4. Actionable Outcomes

- **Skill Updates**: Propose specific diffs to existing skills based on the
  evaluation findings.
- **Process Refinement**: Document new best practices or anti-patterns
  discovered during the evaluation.
- **Report**: Send a synthesized report to the main agent detailing the
  execution, evaluation, and proposed improvements.
