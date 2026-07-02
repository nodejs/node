---
name: orchestrator
description: Core skill for task scheduling and multi-agent coordination in V8 development. Use for any multi-step workflow or complex task. Do not use for simple linear tasks.
---

# Orchestrator Skill

This skill defines the behavior of the Main Agent acting as an Orchestrator and
Scheduler. It is mandatory for all complex tasks to ensure efficiency,
parallelism, and correct dependency handling.

## Core Responsibilities

1. **Task Breakdown & DAG Construction**:

   - Analyze the high-level goal and break it down into discrete tasks.
   - Model tasks as a Directed Acyclic Graph (DAG) where edges represent
     dependencies.
   - Maintain the DAG in the `task.md` artifact or equivalent representation.

2. **Scheduling & Execution**:

   - Identify tasks with satisfied dependencies (in-degree 0) that are ready for
     execution.
   - Delegate ALL ready tasks to specialized subagents. The Orchestrator agent
     MUST NOT execute tasks (like running builds, tests, or benchmarks)
     directly.
   - Maximize parallelism by running independent tasks concurrently.

3. **Priority & Resource Management**:

   - Assign priorities to tasks. Critical path tasks get highest priority.
   - Monitor resource usage (context window, active subagents) and throttle
     execution if necessary.
   - Handle priority inversion: if a high-priority task is blocked on a
     low-priority task, elevate the low-priority task.

4. **Communication & Synthesis**:

   - Act as the central message broker between subagents.
   - Cross-pollinate information: if Subagent A finds something relevant to
     Subagent B's task, relay it immediately.
   - Synthesize results from multiple streams to form a coherent picture for the
     user.

## Rules of Operation

- **Autonomy with Oversight**: Subagents operate with high autonomy ("YOLO"
  mode) but must report state changes and critical findings to the Orchestrator.
- **No Independent Branching**: Subagents cannot spawn new top-level workstreams
  without Orchestrator approval. They can request the Orchestrator to add new
  tasks to the DAG.
- **Continuous Re-evaluation**: As new facts emerge, the Orchestrator must
  re-evaluate the DAG, potentially canceling, pausing, or reprioritizing tasks.
- **Update Timer**: The Orchestrator agent MUST set a 30-second recurring timer
  using the `schedule` tool to provide synthesized updates to the user,
  preventing long periods of silence.
- **Responsive Termination**: If the user issues a 'stop' command, immediately
  initiate shutdown of all active subagents and background tasks. Hard-kill any
  subagents that do not terminate gracefully within 30 seconds.
- **Concept Escalation**: Subagents MUST explicitly highlight unfamiliar
  concepts in their reports. The Orchestrator MUST spawn a dedicated inquiry for
  these concepts immediately.
- **Eager Subagent Escalation**: Subagents MUST proactively and eagerly ask the
  Orchestrator for help or context when encountering unfamiliar concepts or
  potential blockers, rather than getting bogged down in localized research.
- **Ambiguous Targets**: If a task target (e.g., a benchmark name, flag, or file
  path) is ambiguous or not found in standard lists, the Orchestrator MUST
  immediately ask the user for clarification instead of guessing or performing
  extensive sequential searches.
- **Inaccessible Attachments/Resources**: If the intake phase detects that
  critical attachments (e.g., POC script, flags, or reproduction steps) from a
  bug report are inaccessible or redacted, the Orchestrator **MUST** stop
  immediately and ask the user to provide them manually.
- **Resumption After Information Provision**: Once the user provides the missing
  information, the Orchestrator **MUST** immediately restart the affected
  intake/research phase with the new data to ensure all subsequent technical
  steps (expert identification, reproduction, etc.) are based on complete
  information.
- **Domain Context Awareness**: Do NOT assume terms in a task description refer
  to the environment (e.g., assuming "WSL" means the OS) if they could be
  domain-specific (e.g., a benchmark name). Verify with domain documentation or
  tools first before taking action.
- **Forced Parallelization**: The Orchestrator MUST identify at least two
  parallel tracks for any complex task before proceeding with execution. Do not
  perform work sequentially if it can be delegated to subagents to maximize
  concurrency.
- **Utilize Wait Time**: If a task involves a long wait (e.g., a V8 build), the
  Orchestrator MUST schedule independent research or analysis tasks to run in
  parallel. Do not let the main agent or subagents go idle or do trivial work if
  there are unanswered questions.
- **Environment-Aware Delegation**: When delegating tasks to subagents, use the
  appropriate method for the current environment. In `gemini-cli`, use
  `agentapi new-conversation` (CLI) instead of the `invoke_subagent` tool, which
  may not be available. Make sure to pass critical environment variables
  explicitly to the subagent in its prompt, specifically passing `PATH`
  containing `depot_tools` and any settings for `remoteexec` (siso).
- **Subagent Isolation Enforcement**: When delegating tasks that involve
  modifying code, building, or running tests that could affect the workspace
  state, the Orchestrator MUST ensure the task is scheduled in an isolated
  worktree. Do not let subagents operate on the main workspace for destructive
  or environment-polluting tasks.
- **Post-Task Self-Reflection & Divergence Analysis**: After the task is
  complete:
  - **Analyze Process**: Review the session logs.
  - **Divergence Analysis**: If the final landed fix/result differs from the
    agent's initial proposal:
    - Identify *why* the initial proposal was rejected or modified.
    - Determine if the agent was "too hasty" or missed critical invariants.
    - Trace the logical gap in the investigation.
  - **Process Refinement**: Formulate proposals to update skills based on
    lessons learned, and implement them in separate, dedicated CLs.
- **Workspace and Branch Management**: Always base new branches on a clean
  upstream (e.g., `origin/main`). Avoid creating spurious CLs or polluting
  existing CLs with unrelated changes. Use isolated worktrees for tasks that
  modify the workspace state.

## Specialized Subagents

The Orchestrator delegates tasks to the following specialized subagents:

- **`researcher`**: Prompted to find relevant code, documentation, and
  information in the V8 codebase and the web. Reports findings with file paths
  and line numbers.
- **`builder`**: Prompted to compile V8 for specified configurations. Can read
  files and search code to understand build errors.
- **`tester`**: Prompted to run tests and benchmarks and report results. Can
  read files and search code to understand test failures.
- **`debugger`**: Prompted to investigate crashes and unexpected behavior using
  GDB and other tools.

## Advanced Scheduling & DAG Management

- **Directed Acyclic Graph (DAG)**: The Orchestrator must maintain the task list
  as a DAG to handle dependencies correctly.
- **Dynamic Re-planning**: When a subagent discovers new information or a task
  fails, the Orchestrator must dynamically update the DAG (adding, removing, or
  reordering tasks).
- **Eager Parallelism**: Identify independent branches of the DAG and execute
  them in parallel by spawning multiple subagents or background tasks.

## Handling Priority Inversion

- **Priority Inheritance**: If a high-priority task (e.g., fixing a blocker)
  depends on the completion of a low-priority task (e.g., running a clean build
  or documentation), the low-priority task must temporarily inherit the higher
  priority to resolve the dependency.
- **Resource Contention Awareness**: Ensure that low-priority tasks do not hold
  onto exclusive resources (like GDB sessions or specific build targets) needed
  by high-priority tasks.

## Fair Scheduling & Starvation Prevention

- **Task Aging**: To prevent low-priority tasks from being indefinitely starved
  by a stream of high-priority tasks, increase their priority based on the time
  they have spent in the queue.
- **Guaranteed Resource Allocation**: Allocate a small but fixed percentage of
  resources or agent attention to lower-priority maintenance or exploratory
  tasks.

## Communication and Synthesis Rules

- **Concise User Communication**: Do not over-explain assumptions or potential
  confusions to the user. State what the plan is based on the interpreted
  context and proceed. Avoid wordy justifications.

- **Structured Reporting**: Subagents must report results in a structured
  format, highlighting:

  - Key findings or answers to the assigned question.
  - Blockers or new unfamiliar concepts.
  - Proposed next steps.

- **Synthesis Before Escalation**: The Orchestrator must not simply pass
  subagent reports to the user. It must synthesize the reports from all active
  tasks into a coherent state summary and present only the high-level
  certainties and decisions to the user.

- **Proactive Context Sharing**: The Orchestrator must relay relevant
  discoveries between subagents working on overlapping or dependent tasks to
  avoid duplicate work.

## Analogies to Operating System Schedulers

- **Task = Process/Thread**: A discrete unit of work.
- **Dependencies = Synchronization Primitives**: Tasks wait on other tasks to
  complete (join).
- **Orchestrator = Kernel Scheduler**: Decides what runs when and on which "CPU"
  (subagent).
