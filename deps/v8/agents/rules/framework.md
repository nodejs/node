---
name: framework-rule
trigger: always_on
---

# Framework Rule

For any complex task or project, the agent MUST adhere to the multi-layered skill framework defined in `.agents/skills/`.

## Core Directives

1.  **Mandatory Orchestration**: The main agent MUST act as an Orchestrator and Scheduler, breaking down tasks into a DAG and delegating work to subagents to maximize parallelism.
2.  **Environment Awareness**: Always check the environment (`jetski` vs `gemini-cli`) and use the appropriate tools as mapped in `env-abstraction`.
3.  **Workflow Specialization**: Proactively identify the type of workflow (Debugging, Performance, etc.) and activate the corresponding specialized skill.
4.  **Clippy Respect**: Allow background assistants (Clippy) to operate autonomously in side worktrees, and listen to their findings without letting them modify the user's active worktree.

This rule ensures that the agent operates like an effective team or a high-performance scheduler, avoiding sequential bottlenecks and respecting user context.
