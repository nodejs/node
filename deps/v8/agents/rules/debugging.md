---
name: debugging-rule
trigger: model_decision
description: Guidelines and guardrails for debugging V8 crashes, preserving reproducers, and running GDB
---

# Debugging Workflow Guidelines

When tasked with debugging a crash or investigating unexpected behavior, choose
the mix of direct investigation, background execution, and subagent delegation
that best fits the problem:

## 1. Execution & Parallelism

- **Start Builds Asynchronously**: Kick off required builds (`gm.py`) in the
  background immediately and begin reading the reproducing script, stack trace,
  and relevant C++ code while the build runs.
- **Wait Reactively (No Polling)**: When running background jobs or subagents,
  rely on completion notifications instead of polling output directories with
  repeated `ls` commands.
- **GDB Resource Management**: Run GDB via the CLI or a debugger MCP tool if
  available, and avoid running multiple concurrent GDB sessions on the same
  binary/build directory unless they are testing independent execution paths.

## 2. Reproducer & Triage Guardrails

- **Environment Preservation**: Preserve the original reproducing script exactly
  as-is without modification. If the script fails due to missing flags or
  appears to have invalid calls (e.g., missing
  `%PrepareFunctionForOptimization`), keep it unaltered and ask the user to
  provide the correct flags or environment. Create scratch copies for
  minimization or testing if needed.
- **Scripted Reproducer Minimization**: Use
  `vpython3 agents/skills/minimize-reproducer/scripts/minimize_dd.py` on a
  scratch copy when reducing bloated JS reproducers instead of deleting lines or
  flags manually one by one.
- **Error Log & Source Triage**: Analyze the reproducing JS script, runtime
  flags, and assertion stack traces early to narrow down the failing subsystem.

## 3. Synthesis & Communication

- **Fix Presentation**: When a fix is proposed, present it clearly to the user,
  explaining the root cause, rationale, and how it aligns with V8 best
  practices.
- **Bug ID Association**: Only attach bug IDs to a CL when there is proof that
  the CL fixes or affects the issue.
