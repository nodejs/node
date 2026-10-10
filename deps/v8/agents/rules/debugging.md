---
name: debugging-rule
trigger: model_decision
description: Guidelines and guardrails for debugging V8 crashes, preserving reproducers, and running GDB
---

# Debugging Workflow Guidelines

When tasked with debugging a crash or investigating unexpected behavior, balance
direct technical investigation with background execution and targeted
delegation:

## 1. Execution & Parallelism

- **Start Builds Asynchronously**: Kick off required builds (`gm.py`) in the
  background immediately and begin reading the reproducing script, stack trace,
  and relevant C++ code while the build runs.
- **Direct Investigation Default**: For tightly coupled compiler, GC, or runtime
  invariants, read the relevant C++ files (via `code_search` and `view_file`)
  and run GDB (`gdb-mcp` or `d8`) directly to keep full context in a single
  loop.
- **Targeted Subagent Delegation**: Spawn subagents (`self` or
  `research-google`) when investigating genuinely orthogonal hypotheses,
  performing broad git blame/history archeology, or running independent test
  suites in parallel.
- **GDB Resource Management**: Avoid running multiple concurrent GDB sessions on
  the same binary/build directory unless they are testing independent execution
  paths.

## 2. Reproducer & Triage Guardrails

- **Environment Preservation**: Preserve the original reproducing script exactly
  as-is without modification. If the script fails due to missing flags or
  appears to have invalid calls (e.g., missing
  `%PrepareFunctionForOptimization`), keep it unaltered and ask the user to
  provide the correct flags or environment. Create scratch copies for
  minimization or testing if needed.
- **Error Log & Source Triage**: Analyze the reproducing JS script, runtime
  flags, and assertion stack traces early to narrow down the failing subsystem.

## 3. Synthesis & Communication

- **Fix Presentation**: When a fix is proposed, present it clearly to the user,
  explaining the root cause, rationale, and how it aligns with V8 best
  practices.
- **Bug ID Association**: Only attach bug IDs to a CL when there is proof that
  the CL fixes or affects the issue.
