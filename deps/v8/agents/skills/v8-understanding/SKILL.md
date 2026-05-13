---
name: v8-understanding
description: Guides the methodology for investigating V8 concepts, codebase structure, performance, memory, and architecture. MANDATORY to use whenever triaging bugs, investigating crashes, reading C++ files, analyzing compiler optimizations (Ignition, Maglev, TurboFan, Turboshaft), or doing static research on V8 subsystems. Essential for locating high-level design documents in the local docs/ directory before reading C++ code. Do not use for raw build commands or Git workflows.
---

# V8 Understanding Strategy

Use this skill to explore V8 concepts, components, or optimization logic deeply.
This focuses on the *methodology* of understanding, assuming orchestration is
handled globally by the framework.

## Core Methodology: Hypothesis Testing

To understand complex V8 behavior, follow a scientific approach:

1. **Formulate Hypotheses**: Based on initial reading or symptoms, hypothesize
   how a feature works or why a bug occurs.
2. **Design Experiments**: Create small, isolated JavaScript repros or use
   specific `d8` flags to test the hypothesis.
3. **Analyze Results**: Use tools like GDB, Turbolizer, or profile logs to
   verify if the behavior matches the hypothesis.
4. **Iterate**: Refine the hypothesis based on data.

## Key Areas of Understanding

1. **Performance**:
   - Understand execution tiers (Ignition, Maglev, TurboFan).
   - Trace how optimizations are applied and why deoptimizations happen.
2. **Memory**:
   - Understand object layout (Hidden Classes/Maps).
   - Trace garbage collection behavior and allocation patterns.
3. **Architecture & Detailed Interaction**:
   - Focus on the detailed interaction between components (e.g., how the Parser
     feeds the AST, which feeds Ignition).
   - Understand how optimizations interact with core features. Avoid treating
     components in isolation.

## Documentation Resources

- **Local Documentation**: Refer to the `docs/` directory in the V8 root for
  documentation. It contains content from `v8.dev`, so there is no need to
  access the external website or use browser tools. Use local search and file
  reading to access it.
  - **High-Level & Cross-Cutting Concepts**: Many important design principles,
    API designs, and cross-cutting features (e.g., sandboxing, embedding,
    debugging, profiling, security) are documented in files directly at the root
    of the `docs/` directory (e.g., `docs/embed.md`, `docs/gdb.md`,
    `docs/untrusted-code-mitigations.md`). Always scan the root `docs/` folder.
  - **Subsystem Docs**: When investigating a specific subsystem, check if there
    is a corresponding subdirectory in `docs/` (e.g., `docs/compiler/`,
    `docs/heap/`). Do not assume a strict 1:1 directory mapping between `src/`
    and `docs/`.
  - **Global Docs Search**: As a first step, always run a local search (e.g.
    `grep` or find tools) across the entire `docs/` directory to find all files
    mentioning the key classes, APIs, or optimization concepts you are
    investigating (e.g. search for `BytecodeGenerator` in all `.md` files).

## Workflow

1. **Static Research**:
   - Identify the main concepts, subsystems, and classes being investigated.
   - **Search globally** in the `docs/` directory for these concepts/classes to
     locate relevant design files, whether they live at the root of `docs/` or
     in a subsystem subfolder.
   - Read the discovered documentation files to build a high-level mental model
     *before* reading complex C++ code.
   - Read code in `src/` to fill in details.
2. **Dynamic Verification**: Use small tests and tracing flags to confirm the
   model.
3. **Red/Blue Team Analysis**: Have one agent/subagent propose an explanation
   (Blue) and another try to find counter-examples or flaws in it (Red).
