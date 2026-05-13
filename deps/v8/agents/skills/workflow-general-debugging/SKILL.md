---
name: workflow-general-debugging
description: Workflow for general debugging tasks without the overhead of issue tracking. Use when investigating crashes, hangs, or unexpected behavior where no specific issue ID is provided. Do not use when issue ID is provided.
---

# Skill: Workflow - General Debugging

Workflow for general debugging tasks in a pristine branch or project work,
without the overhead of issue tracking. Use this skill when investigating
crashes, hangs, or unexpected behavior where no specific issue ID is provided.

## Principles

1. **Lightweight & Focused**: Focus on the immediate problem without the
   overhead of issue tracking conventions (e.g., `regress-<id>` naming, `Bug:`
   tags) unless requested.
2. **Efficient Investigation**: Prioritize quick root cause analysis using GDB
   and targeted code reading over exhaustive documentation.
3. **Pristine Branch Work**: Assume you are working on a clean branch or ongoing
   project work where changes are local.
4. **Architectural Skepticism**: Always question your fix. Ensure it addresses
   the root cause and not just a symptom.

## Workflow

### 1. Problem Triage

- Reproduce the issue if possible.
- Analyze the symptom (crash, hang, incorrect output).
- If a repro script is available, do not modify it directly. Create a scratch
  copy for investigation.

### 2. Static & Dynamic Analysis

- Use `code_search` or `grep_search` to locate relevant code based on stack
  traces or symptoms.
- Use GDB (`gdb-mcp`) to inspect runtime state, set breakpoints, and trace
  execution.
- Focus on understanding the immediate failure context before expanding the
  scope.

### 3. Propose Fix

- Develop a minimal fix targeting the root cause.
- Follow V8 Best Practices.
- **Architectural Skepticism**: Argue against your own proposal. Does it disable
  optimizations? Does it violate invariants?

### 4. Verify

- Verify the fix against the repro or by running relevant unit tests.
- Ensure no new failures are introduced in related areas.

### 5. Finalization

- Format the code using `git cl format`.
- Present the fix to the user/main agent.
- Do not create commits or upload CLs unless explicitly requested.
