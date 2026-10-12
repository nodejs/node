# V8 Agent Workspace

This is the workspace configuration for V8 coding agents (`jetski-cli`).

For understanding V8 concepts and structure, refer to the
[v8-understanding](/agents/skills/v8-understanding/SKILL.md) skill.

Some hints:

- You are an expert C++ developer.
- V8 is shipped to users and running untrusted code; make sure that the code is
  absolutely correct and bug-free as correctness bugs usually lead to security
  issues for end users.
- V8 is providing support for running JavaScript and WebAssembly on the web. As
  such, it is critical to aim for best possible performance when optimizing V8.

## Skills & Rules Setup

To install workspace skills and rules:

- **For Jetski**: Run `vpython3 agents/scripts/install_for_jetski.py` to create
  symlinks in `.agents/`.
- **For GitHub Copilot CLI**: Run
  `vpython3 agents/scripts/install_for_copilot_cli.py` to install repository
  instructions and compatible rules/skills in `.github/`.

## Workspace Skills & Rules

General guidance and reference information are available as skills or rules:

- **Folder/Directory Structure**: Understand the layout of the V8 repository.
  See [v8-structure](/agents/skills/v8-structure/SKILL.md).
- **Key Commands**: Find commands for building and debugging. See
  [v8-commands](/agents/skills/v8-commands/SKILL.md).
- **Testing**: Detailed guide for running and interpreting tests. See
  [v8-testing](/agents/skills/v8-testing/SKILL.md).
- **Best Practices**: Common pitfalls and fix proposal guidelines. See
  [v8-best-practices](/agents/rules/v8-best-practices.md).
- **Setup**: Handles missing dependencies and configuration for V8 tools. See
  [v8-setup](/agents/skills/v8-setup/SKILL.md).
- **Git Commit & CL Conventions**: Commit message format and `git cl` usage. See
  [git-commit](/agents/rules/git-commit.md) and
  [git-cl](/agents/rules/git-cl.md).
- **Torque**: Expert guidance for Torque. See
  [torque](/agents/skills/torque/SKILL.md).
- **Debugging Workflow**: Guide for issue-based debugging. See
  [workflow-debugging](/agents/skills/workflow-debugging/SKILL.md).
- **General Debugging Workflow**: Guide for general debugging. See
  [workflow-general-debugging](/agents/skills/workflow-general-debugging/SKILL.md).
- **Performance Workflow**: Guide for performance evaluation. See
  [workflow-perf](/agents/skills/workflow-perf/SKILL.md).
- **Agent Evaluation**: Workflow for evaluating agents. See
  [agent-evaluation-framework](/agents/skills/agent-evaluation-framework/SKILL.md).
- **Agent Self-Improvement**: Workflow for agent self-improvement. See
  [agent-self-improvement](/agents/skills/agent-self-improvement/SKILL.md).
- **V8 Workflow**: General workflow for V8 development. See
  [v8-workflow](/agents/skills/v8-workflow/SKILL.md).

## Coding and Committing

- Always follow the style conventions used in code surrounding your changes.
- Otherwise, follow
  [Chromium's C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md).
- Use `git cl format` to automatically format your changes.
- Follow [git-commit](/agents/rules/git-commit.md) and
  [git-cl](/agents/rules/git-cl.md) for commit conventions.
- For best practices and common pitfalls, see
  [v8-best-practices](/agents/rules/v8-best-practices.md).

## Agent Framework

- Follow the rules in
  [execution-constraints](/agents/rules/execution-constraints.md) for background
  execution, subagent delegation, and workspace isolation.
