---
name: debugging-rule
trigger: always_on
---

# Mandatory Debugging Workflow (ORCHESTRATION ONLY)

When tasked with debugging a crash or investigating unexpected behavior, the
main agent **MUST** adhere to the following strict constraints:

## 1. Role Restriction: Orchestrator Only

- Delegate execution of heavy technical investigation tools to specialized
  subagents.
- Use subagents when running GDB (`gdb-mcp`).
- Delegate extensive searches or reading large C++ files to subagents.
- Your sole job is to **Orchestrate**, **Delegate**, and **Synthesize**.
- **Continuous Delegation**: Even after receiving reports, delegate the next
  investigation steps. Do not take over.
- **Aggressive Parallelization**: Do not wait for a subagent to finish or fail
  before spawning others for independent tasks. If a subagent reports a failure,
  a new hypothesis, or a new lead, immediately spawn a focused subagent to
  investigate that specific dimension in parallel.
- **User Interface**: Act strictly as the interface to the user. Keep them
  informed of high-level certainties. You may provide brief status updates on
  running agents.
- **Naming**: Do not refer to subagents by specific titles (e.g., "Spec Expert")
  in user communications. Use generic descriptions of the inquiry instead.

## 2. Mandatory First Actions (Three Parallel Tracks)

Upon receiving a debugging task, you MUST immediately initialize the following
tracks in parallel:

### Track A: Heavy Infrastructure

- Start building (`gm.py`) and preparing for runtime analysis (`gdb-mcp` or
  `rr`) immediately.

### Track B: Conceptual Triage

- **JS Source Reading**: Analyze the reproducing JS script to identify language
  features (e.g., closures, `eval`, TDZ) and runtime flags/hints.
- **Environment Preservation**: Preserve the original reproducing script exactly
  as-is without modification. If the script fails due to missing flags or
  appears to have invalid calls (e.g., missing
  `%PrepareFunctionForOptimization`), keep it unaltered and ask the user to
  provide the correct flags or environment. Create scratch copies for
  minimization or testing if needed.
- **Error Log Analysis**: Analyze error logs and stack traces to identify the
  failing component or assertion.

### Track C: Static Research & Eager Delegation

- **V8/Spec Exploration**: Use your internal knowledge to start reading relevant
  V8 C++ code (via `code_search`) and consulting ECMAScript spec requirements
  simultaneously.
- **Work Branching**: PROACTIVELY spawn subagents (e.g., via `self`) for every
  independent dimension of the task that can be parallelized.

## 3. Iterative Concept Discovery & Overlap Analysis

During investigation (especially via GDB or subagent reports):

- **Continuous Discovery**: If you encounter a new internal concept, spawn a
  subagent to research it immediately.
- **Overlap & Re-evaluation**: Actively check for overlaps between concepts.
  When an overlap is detected, spawn a subagent to analyze the interaction and
  re-evaluate previous findings.

## 4. Synthesis & Communication

- **Proactive Updates**: Keep the user informed of key milestones (e.g., triage
  results, parallel task status).
- **Fix Presentation**: When a fix is proposed, **YOU MUST** present it clearly
  to the user immediately, explaining the rationale and how it aligns with V8
  best practices.
- **Bug ID Association**: Only attach bug IDs to a CL when there is proof that
  the CL fixes or affects the issue.

## 5. Subagent Coordination & Anti-Duplication

- **Centralized Routing**: The main agent MUST monitor subagent tasks to prevent
  duplicate investigations.
- **Cross-Pollination**: If Subagent A discovers information relevant to
  Subagent B, the main agent must immediately relay that information.
- **Branching**: If tasks overlap, instruct subagents to pivot to uninvestigated
  areas and share findings.
- **GDB Resource Management**: Avoid spawning multiple subagents running GDB
  concurrently unless they are explicitly investigating independent execution
  paths or orthogonal hypotheses.
