---
name: v8-understanding
description: "Guides the methodology for understanding V8 concepts, performance, and architecture. Use when exploring complex behavior or forming hypotheses. Do not use for code formatting rules."
---

# V8 Understanding Strategy

Use this skill to explore V8 concepts, components, or optimization logic deeply. This focuses on the *methodology* of understanding, assuming orchestration is handled globally by the framework.

## Core Methodology: Hypothesis Testing

To understand complex V8 behavior, follow a scientific approach:
1.  **Formulate Hypotheses**: Based on initial reading or symptoms, hypothesize how a feature works or why a bug occurs.
2.  **Design Experiments**: Create small, isolated JavaScript repros or use specific `d8` flags to test the hypothesis.
3.  **Analyze Results**: Use tools like GDB, Turbolizer, or profile logs to verify if the behavior matches the hypothesis.
4.  **Iterate**: Refine the hypothesis based on data.

## Key Areas of Understanding

1.  **Performance**:
    -   Understand execution tiers (Ignition, Maglev, TurboFan).
    -   Trace how optimizations are applied and why deoptimizations happen.
2.  **Memory**:
    -   Understand object layout (Hidden Classes/Maps).
    -   Trace garbage collection behavior and allocation patterns.
3.  **Architecture & Detailed Interaction**:
    -   Focus on the detailed interaction between components (e.g., how the Parser feeds the AST, which feeds Ignition).
    -   Understand how optimizations interact with core features. Avoid treating components in isolation.

## Documentation Resources

-   **Local Documentation**: Refer to the `docs/` directory in the V8 root for documentation. It contains content from `v8.dev`, so there is no need to access the external website or use browser tools. Use local search and file reading to access it.

## Workflow

1.  **Static Research**: Read code and local docs (`docs/`) to build an initial mental model.
2.  **Dynamic Verification**: Use small tests and tracing flags to confirm the model.
3.  **Red/Blue Team Analysis**: Have one agent/subagent propose an explanation (Blue) and another try to find counter-examples or flaws in it (Red).
