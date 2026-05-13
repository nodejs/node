# Gemini Workspace for V8

This is the workspace configuration for V8 when using Gemini.

For understanding V8 concepts and structure, refer to the
[v8-understanding](agents/skills/v8-understanding/SKILL.md) skill.

Some hints:

- You are an expert C++ developer.
- V8 is shipped to users and running untrusted code; make sure that the code is
  absolutely correct and bug-free as correctness bugs usually lead to security
  issues for end users.
- V8 is providing support for running JavaScript and WebAssembly on the web. As
  such, it is critical to aim for best possible performance when optimizing V8.

## Subagents Setup

To use the subagents in this repository, you need to run the appropriate
installation script depending on your environment:

- **For Gemini CLI**: Run `vpython3 agents/scripts/install_for_gemini_cli.py` to
  generate the subagent files in `.gemini/agents/`.
- **For Jetski**: Run `vpython3 agents/scripts/install_for_jetski.py` to create
  symlinks in `.agents/agents/`.

## Workspace Subagents

Detailed information has been moved to specialized subagents in
`agents/agents/`. Use them on demand:

- **Researcher**: Explores the codebase and finds information. See
  `agents/agents/researcher/`.
- **Builder**: Compiles V8. See `agents/agents/builder/`.
- **Tester**: Runs tests and benchmarks. See `agents/agents/tester/`.
- **Debugger**: Investigates crashes using GDB. See `agents/agents/debugger/`.

## Workspace Skills & Rules

General guidance and reference information are available as skills or rules:

- **Folder/Directory Structure**: Understand the layout of the V8 repository.
  See [v8-structure](agents/skills/v8-structure/SKILL.md).
- **Key Commands**: Find commands for building and debugging. See
  [v8-commands](agents/skills/v8-commands/SKILL.md).
- **Testing**: Detailed guide for running and interpreting tests. See
  [v8-testing](agents/skills/v8-testing/SKILL.md).
- **Best Practices**: Common pitfalls and fix proposal guidelines. See
  [v8_best_practices](agents/rules/v8_best_practices.md).
- **Setup**: Handles missing dependencies and configuration for V8 tools. See
  [v8-setup](agents/skills/v8-setup/SKILL.md).
- **Git CL Conventions**: Commit message format and usage. See
  [git_cl](agents/rules/git_cl.md).
- **Torque**: Expert guidance for Torque. See
  [torque](agents/skills/torque/SKILL.md).
- **Debugging Workflow**: Guide for issue-based debugging. See
  [workflow-debugging](agents/skills/workflow-debugging/SKILL.md).
- **General Debugging Workflow**: Guide for general debugging. See
  [workflow-general-debugging](agents/skills/workflow-general-debugging/SKILL.md).
- **Performance Workflow**: Guide for performance evaluation. See
  [workflow-perf](agents/skills/workflow-perf/SKILL.md).
- **Porting Guide**: Guide for porting objects to HeapObjectLayout. See
  [port-to-heapobjectlayout](agents/skills/port-to-heapobjectlayout/SKILL.md).
- **Agent Evaluation**: Workflow for evaluating agents. See
  [agent-evaluation-framework](agents/skills/agent-evaluation-framework/SKILL.md).
- **Agent Self-Improvement**: Workflow for agent self-improvement. See
  [agent-self-improvement](agents/skills/agent-self-improvement/SKILL.md).
- **V8 Workflow**: General workflow for V8 development. See
  [v8-workflow](agents/skills/v8-workflow/SKILL.md).

## Coding and Committing

- Always follow the style conventions used in code surrounding your changes.
- Otherwise, follow
  [Chromium's C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md).
- Use `git cl format` to automatically format your changes.
- Follow [git_cl](agents/rules/git_cl.md) for commit conventions.
- For best practices and common pitfalls, see
  [v8_best_practices](agents/rules/v8_best_practices.md).

## Agent Framework

- **Mandatory Orchestration**: For any task in V8, the agent MUST act as an
  Orchestrator and use the specialized subagents defined in `agents/agents/`.
- Follow the rules in `agents/rules/framework.md` and
  `agents/rules/execution_constraints.md`.
- This ensures efficiency, parallelism, and consistency across all tasks.
